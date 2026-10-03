// ---------------------------------------------------------------------------
// The editor's window.
//
// A Win32 window hosting a WebView2 that loads the interface from a resource.
// The interface is compiled in and loaded with NavigateToString, so there is
// no temporary file to write and no local HTTP server to run — both of which
// would be a way for the app to load something other than its own UI.
//
// The window owns the document and the bridge. The bridge owns the protocol.
// The renderer owns the GL context. Each of those is testable on its own,
// which is the reason for the split.
// ---------------------------------------------------------------------------

#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <wrl.h>

#include <WebView2.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "app/Bridge.h"
#include "core/EffectCatalog.h"
#include "core/Model.h"
#include "gl/Renderer.h"
#include "gl/ShaderTable.h"
#include "media/Media.h"
#include "media/Png.h"
#include "ui/assets/resource.h"

using namespace Microsoft::WRL;

namespace keyflow {

namespace {

/// The interface HTML, from the executable's own resources.
std::string loadInterfaceHtml();

/// UTF-8 to UTF-16.
///
/// The project is UTF-8 throughout -- JSON, the page, project files -- and
/// Win32 is UTF-16, so this is the one conversion point. Byte-wise
/// construction from a UTF-8 string looks like it works and mangles anything
/// outside ASCII.
std::wstring toWide(const std::string& text)
{
    if (text.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                           static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                        out.data(), length);
    return out;
}

/// UTF-16 to UTF-8.
std::string toUtf8(const std::wstring& text)
{
    if (text.empty()) return {};
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                                           static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                        out.data(), length, nullptr, nullptr);
    return out;
}

/// The application.
///
/// One object rather than a pile of globals: the window procedure needs to
/// reach the document, the bridge and the renderer, and passing a pointer to
/// this through the window's user data is the way to do that without a global
/// per field.
class App : public BridgeHost
{
public:
    App() = default;
    ~App() override = default;

    bool create(HINSTANCE instance, int showCommand);
    int run();

    // --- BridgeHost -------------------------------------------------------

    std::string pickOpenFile(const std::string& filterName,
                             const std::string& filterPattern) override;
    std::string pickSaveFile(const std::string& filterName,
                             const std::string& filterPattern,
                             const std::string& suggestedName) override;

    std::string ffmpegPath() override { return _tools.ffmpeg; }
    std::string ffprobePath() override { return _tools.ffprobe; }

    void startRender(const std::string& outputPath, const std::string& videoCodec,
                     int quality, bool useBitrate, int bitrateKbps,
                     bool resize, int width, int height) override;
    void cancelRender() override;
    void requestQuit() override;
    void reportStatus(const std::string& message) override;

    /// Write the page's layout report to a file. Set from the command line.
    void setShowLayoutReport(bool enabled) { _showLayoutReport = enabled; }

    /// Start the renderer without a window or a page, for the self-test.
    bool initialiseForSelfTest(std::string& error);

    /// Render one frame of a synthetic composition and write it out, then
    /// quit. Set from the command line.
    ///
    /// This is the pipeline check: it exercises the model, the renderer, the
    /// shader compilation, the compositor and the PNG writer without a window,
    /// a page, or a mouse. A harness that has to drive the interface with
    /// synthetic clicks is a harness that fails for reasons that have nothing
    /// to do with the code.
    int runSelfTest(const std::string& outputPath);

private:
    static LRESULT CALLBACK windowProc(HWND window, UINT message,
                                       WPARAM wParam, LPARAM lParam);

    bool createWindow(HINSTANCE instance, int showCommand);
    bool createWebView();
    void onResize(int width, int height);
    void sendToPage(const std::string& text);

    /// Handle a message from the page.
    void onPageMessage(const std::string& text);

    /// Send the current frame to the page as a data URL.
    void sendFrame(double time);

    /// Run a render on a worker thread.
    void renderThread(std::string outputPath, std::string videoCodec, int quality,
                      bool useBitrate, int bitrateKbps, bool resize,
                      int width, int height);

    HINSTANCE _instance = nullptr;
    HWND _window = nullptr;

    ComPtr<ICoreWebView2Environment> _environment;
    ComPtr<ICoreWebView2Controller> _controller;
    ComPtr<ICoreWebView2> _webView;

    Project _document;
    std::unique_ptr<Bridge> _bridge;
    Renderer _renderer;
    FFmpegPaths _tools;

    /// True while a render is running, so a second one is refused rather than
    /// starting a thread that fights the first for the GPU.
    std::atomic<bool> _rendering{false};
    std::atomic<bool> _cancelRender{false};
    std::thread _renderWorker;

    /// The frame the page last asked for, so the render thread and the preview
    /// do not disagree about which time is current.
    std::atomic<double> _requestedTime{0.0};

    /// Why the renderer could not start, when it could not. Reported to the
    /// page so a blank viewer has an explanation rather than a mystery.
    std::string _rendererError;

    /// Write the page's layout report to a file, for the harness and CI.
    bool _showLayoutReport = false;
};

// --- window ---------------------------------------------------------------

LRESULT CALLBACK App::windowProc(HWND window, UINT message,
                                 WPARAM wParam, LPARAM lParam)
{
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = static_cast<App*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        app->_window = window;
    }

    if (app == nullptr) return DefWindowProcW(window, message, wParam, lParam);

    switch (message) {
    case WM_SIZE:
        if (app->_controller) {
            RECT bounds;
            GetClientRect(window, &bounds);
            app->_controller->put_Bounds(bounds);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_CLOSE:
        // Ask the page whether there is unsaved work, rather than quitting
        // silently and losing it.
        app->sendToPage(R"({"type":"confirmClose"})");
        return 0;

    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

bool App::createWindow(HINSTANCE instance, int showCommand)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &App::windowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"KeyflowWindow";
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP));
    wc.hIconSm = wc.hIcon;

    if (!RegisterClassExW(&wc)) return false;

    // Size to the work area rather than a fixed 1600x1000. A window larger
    // than the screen puts the timeline and the status bar off the bottom edge
    // where they cannot be reached, which is what a fixed size does on any
    // display under about 1600x1000.
    RECT work{};
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) {
        work = RECT{0, 0, 1280, 800};
    }
    const int workWidth = work.right - work.left;
    const int workHeight = work.bottom - work.top;

    // Leave a margin so the window is not flush against the screen edges, and
    // never exceed the work area.
    const int width = std::min(1600, workWidth - 40);
    const int height = std::min(1000, workHeight - 40);
    const int x = work.left + (workWidth - width) / 2;
    const int y = work.top + (workHeight - height) / 2;

    _window = CreateWindowExW(
        0, wc.lpszClassName, L"Keyflow",
        WS_OVERLAPPEDWINDOW,
        x, y, width, height,
        nullptr, nullptr, instance, this);

    if (_window == nullptr) return false;

    ShowWindow(_window, showCommand);
    UpdateWindow(_window);
    return true;
}

// --- WebView2 -------------------------------------------------------------

bool App::createWebView()
{
    // The user data folder goes beside the executable rather than in the
    // default per-user location, so an unzip-and-run copy keeps its state with
    // itself and a machine with no write access to the profile still works.
    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    std::filesystem::path dataFolder =
        std::filesystem::path(modulePath).parent_path() / "KeyflowData";

    const HRESULT created = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, dataFolder.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                if (FAILED(result) || environment == nullptr) {
                    MessageBoxW(_window,
                                L"Keyflow needs the Microsoft Edge WebView2 runtime.\n\n"
                                L"Install it from Microsoft's website and start Keyflow again.",
                                L"Keyflow", MB_OK | MB_ICONERROR);
                    return result;
                }
                _environment = environment;

                return environment->CreateCoreWebView2Controller(
                    _window,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT controllerResult,
                               ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(controllerResult) || controller == nullptr) {
                                return controllerResult;
                            }
                            _controller = controller;
                            _controller->get_CoreWebView2(&_webView);

                            RECT bounds;
                            GetClientRect(_window, &bounds);
                            _controller->put_Bounds(bounds);

                            // The page must not be able to open a second
                            // window, navigate away, or reach the file system
                            // through a link. The editor's UI is a document
                            // that talks to the host, not a browser.
                            ComPtr<ICoreWebView2Settings> settings;
                            if (SUCCEEDED(_webView->get_Settings(&settings))) {
                                settings->put_AreDefaultContextMenusEnabled(FALSE);
                                settings->put_AreDevToolsEnabled(TRUE);
                                settings->put_IsStatusBarEnabled(FALSE);
                                settings->put_AreHostObjectsAllowed(FALSE);
                                settings->put_IsZoomControlEnabled(FALSE);
                            }

                            // Every message from the page goes to the bridge.
                            EventRegistrationToken token;
                            _webView->add_WebMessageReceived(
                                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [this](ICoreWebView2*,
                                           ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                        // The page posts a JSON *string*,
                                        // so the string accessor is the one
                                        // that returns it verbatim.
                                        // get_WebMessageAsJson would wrap it in
                                        // another layer of quoting and the
                                        // bridge would see a JSON string where
                                        // it expects an object — which rejects
                                        // every message, including ready, and
                                        // leaves the window empty.
                                        LPWSTR json = nullptr;
                                        if (SUCCEEDED(args->TryGetWebMessageAsString(&json))
                                            && json != nullptr) {
                                            const std::string utf8 = toUtf8(json);
                                            CoTaskMemFree(json);
                                            onPageMessage(utf8);
                                        }
                                        return S_OK;
                                    }).Get(),
                                &token);

                            // Block navigation away from the loaded document.
                            // NavigateToString gives the page an about:blank
                            // origin, and a link would otherwise be able to
                            // take the editor somewhere else entirely.
                            _webView->add_NavigationStarting(
                                Callback<ICoreWebView2NavigationStartingEventHandler>(
                                    [](ICoreWebView2*,
                                       ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                                        LPWSTR uri = nullptr;
                                        if (SUCCEEDED(args->get_Uri(&uri))
                                            && uri != nullptr) {
                                            const std::wstring target = uri;
                                            CoTaskMemFree(uri);
                                            if (target.rfind(L"about:", 0) != 0
                                                && target.rfind(L"data:", 0) != 0) {
                                                args->put_Cancel(TRUE);
                                            }
                                        }
                                        return S_OK;
                                    }).Get(),
                                &token);

                            // Report whether the page loaded. A blank window
                            // is otherwise the same symptom for "the document
                            // never arrived" and "the document threw on its
                            // first line", and the two need different fixes.
                            {
                                EventRegistrationToken doneToken;
                                _webView->add_NavigationCompleted(
                                    Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                        [this](ICoreWebView2*,
                                               ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                                            BOOL success = FALSE;
                                            args->get_IsSuccess(&success);
                                            if (!success) {
                                                COREWEBVIEW2_WEB_ERROR_STATUS status{};
                                                args->get_WebErrorStatus(&status);
                                                MessageBoxW(
                                                    _window,
                                                    L"Keyflow's interface failed to load.",
                                                    L"Keyflow", MB_OK | MB_ICONERROR);
                                                (void)status;
                                            }
                                            return S_OK;
                                        }).Get(),
                                    &doneToken);
                            }

                            // The interface, from the compiled-in resource.
                            const std::string html = loadInterfaceHtml();
                            if (html.empty()) {
                                MessageBoxW(_window,
                                            L"Keyflow's interface resource is missing.",
                                            L"Keyflow", MB_OK | MB_ICONERROR);
                                return E_FAIL;
                            }
                            const std::wstring wide = toWide(html);
                            _webView->NavigateToString(wide.c_str());
                            return S_OK;
                        }).Get());
            }).Get());

    return SUCCEEDED(created);
}

/// The interface HTML, from the executable's own resources.
std::string loadInterfaceHtml()
{
    HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(IDR_UI_HTML), RT_RCDATA);
    if (resource == nullptr) return {};

    const HGLOBAL loaded = LoadResource(nullptr, resource);
    if (loaded == nullptr) return {};

    const DWORD size = SizeofResource(nullptr, resource);
    const void* data = LockResource(loaded);
    if (data == nullptr || size == 0) return {};

    return std::string(static_cast<const char*>(data), size);
}

void App::sendToPage(const std::string& text)
{
    if (!_webView) return;

    // ExecuteScript takes a JavaScript expression, so the JSON is handed over
    // as a string literal and the page parses it. Passing it bare would
    // evaluate it as code, which is both wrong and a way for a document field
    // to become script.
    std::wstring script = L"window.__keyflowReceive && window.__keyflowReceive(";
    script += toWide(json::write(json::Value(text)));
    script += L");";
    _webView->ExecuteScript(script.c_str(), nullptr);
}

void App::onPageMessage(const std::string& text)
{
    OutputDebugStringW(L"[host] message: ");
    OutputDebugStringW(toWide(text.substr(0, 120)).c_str());
    OutputDebugStringW(L"\n");
    // A message that arrives before the bridge exists is dropped: the bridge
    // is created with the window.
    if (!_bridge) return;

    // The page reports its own layout once, which is how the host learns the
    // driver string without polling.
    std::string parseError;
    const json::Value message = json::parse(text, &parseError);
    if (!parseError.empty()) {
        _bridge->SendError("the page sent something that is not JSON");
        return;
    }

    const std::string type = message["type"].asString();

    if (type == "requestFrame") {
        _requestedTime = message["time"].asNumber(0.0);
        sendFrame(_requestedTime);
        return;
    }
    if (type == "layoutReport") {
        // The page measures itself and sends the numbers. Written to a
        // file when the switch is set, so CI and the harness can check the
        // interface without a human looking at it.
        const std::string report = json::write(message["report"], 2);
        OutputDebugStringA("[host] layout report:\n");
        OutputDebugStringA(report.c_str());
        OutputDebugStringA("\n");

        if (_showLayoutReport) {
            std::ofstream out("layout_report.json", std::ios::binary);
            if (out) out << report;
        }
        return;
    }
    if (type == "requestDriver") {
        json::Value out;
        out.set("type", "driver");
        out.set("driver", _renderer.ready() ? _renderer.driverInfo() : _rendererError);
        out.set("rendererReady", _renderer.ready());
        out.set("ffmpeg", _tools.ffmpeg.empty() ? "not found" : "found");
        out.set("ffmpegPath", _tools.ffmpeg);
        sendToPage(json::write(out));
        return;
    }
    if (type == "pickExportPath") {
        const std::string chosen = pickSaveFile("Video", "*.mp4", "output.mp4");
        if (!chosen.empty()) {
            json::Value out;
            out.set("type", "exportPath");
            out.set("path", chosen);
            sendToPage(json::write(out));
        }
        return;
    }
    if (type == "confirmCloseReply") {
        if (message["quit"].asBool(false)) {
            DestroyWindow(_window);
        }
        return;
    }

    _bridge->HandleMessage(text);
}

void App::onResize(int width, int height)
{
    (void)width;
    (void)height;
    if (_controller) {
        RECT bounds;
        GetClientRect(_window, &bounds);
        _controller->put_Bounds(bounds);
    }
}

// --- preview --------------------------------------------------------------

void App::sendFrame(double time)
{
    Composition* comp = _document.activeComposition();
    if (comp == nullptr) return;
    if (!_renderer.ready()) {
        // Reported rather than dropped: a viewer that stays black with no
        // message is the worst version of this failure.
        json::Value out;
        out.set("type", "frameFailed");
        out.set("reason", _rendererError.empty()
                             ? "the renderer is not ready" : _rendererError);
        sendToPage(json::write(out));
        return;
    }

    // Decode the picture for every media layer. A decode per layer per frame
    // is what the frame cache exists to avoid; until that lands, this is the
    // honest cost of scrubbing.
    std::vector<Frame> frames(comp->layers.size());

    for (std::size_t index = 0; index < comp->layers.size(); ++index) {
        const Layer& layer = comp->layers[index];
        if (layer.kind != LayerKind::Media || layer.assetId.empty()) continue;
        if (!layer.activeAt(time)) continue;

        const Asset* asset = nullptr;
        for (const Asset& candidate : _document.assets) {
            if (candidate.id == layer.assetId) { asset = &candidate; break; }
        }
        if (asset == nullptr || !std::filesystem::exists(asset->path)) continue;

        // Decode at the composition's size. A preview at full resolution for a
        // 4K composition would be slower than the render.
        const int targetWidth = std::min(comp->width, 1280);
        const int targetHeight = std::max(1, targetWidth * comp->height / std::max(comp->width, 1));

        std::string error;
        const double sourceTime = std::max(0.0, time - layer.inPoint);
        if (!decodeFrameAt(_tools, asset->path, sourceTime,
                           targetWidth, targetHeight, frames[index], error)) {
            continue; // a layer that will not decode draws nothing
        }
    }

    RenderSettings settings;
    settings.width = std::min(comp->width, 1280);
    settings.height = std::max(1, settings.width * comp->height / std::max(comp->width, 1));
    settings.time = time;
    settings.frame = static_cast<int>(time * comp->fps());
    settings.preview = true;

    std::string error;
    if (!_renderer.renderFrame(*comp, settings, frames, error)) {
        json::Value out;
        out.set("type", "frameFailed");
        out.set("reason", "the preview could not be drawn: " + error);
        sendToPage(json::write(out));
        return;
    }

    Frame result;
    if (!_renderer.readback(result, error)) {
        json::Value out;
        out.set("type", "frameFailed");
        out.set("reason", "the preview could not be read back: " + error);
        sendToPage(json::write(out));
        return;
    }

    // A data URL rather than a file: the page cannot reach the file system,
    // and a temporary PNG per frame would be a write per mouse move.
    const std::string dataUrl = encodePngDataUrl(result);

    OutputDebugStringW(L"[host] sending a frame\n");

    json::Value out;
    out.set("type", "frame");
    out.set("time", time);
    out.set("data", dataUrl);
    sendToPage(json::write(out));
}

// --- file dialogs ---------------------------------------------------------

std::string App::pickOpenFile(const std::string& filterName,
                              const std::string& filterPattern)
{
    // The common dialog wants a double-null-terminated filter pair.
    std::wstring filter = toWide(filterName);
    filter.push_back(L'\0');
    filter += toWide(filterPattern);
    filter.push_back(L'\0');
    filter.push_back(L'\0');

    wchar_t path[MAX_PATH * 4]{};

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = _window;
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = path;
    ofn.nMaxFile = static_cast<DWORD>(std::size(path));
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&ofn)) return {};

    return toUtf8(path);
}

std::string App::pickSaveFile(const std::string& filterName,
                              const std::string& filterPattern,
                              const std::string& suggestedName)
{
    std::wstring filter = toWide(filterName);
    filter.push_back(L'\0');
    filter += toWide(filterPattern);
    filter.push_back(L'\0');
    filter.push_back(L'\0');

    wchar_t path[MAX_PATH * 4]{};
    const std::wstring suggestion = toWide(suggestedName);
    if (suggestion.size() < std::size(path)) {
        std::copy(suggestion.begin(), suggestion.end(), path);
    }

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = _window;
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = path;
    ofn.nMaxFile = static_cast<DWORD>(std::size(path));
    ofn.lpstrDefExt = L"mp4";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetSaveFileNameW(&ofn)) return {};

    return toUtf8(path);
}

// --- rendering ------------------------------------------------------------

void App::startRender(const std::string& outputPath, const std::string& videoCodec,
                      int quality, bool useBitrate, int bitrateKbps,
                      bool resize, int width, int height)
{
    if (_rendering.load()) {
        _bridge->SendError("a render is already running");
        return;
    }
    if (!_tools.valid()) {
        _bridge->SendError("ffmpeg was not found. Put ffmpeg.exe beside Keyflow, "
                           "or add it to PATH.");
        return;
    }

    // Join the previous worker before starting a new one, or the thread object
    // is assigned over a joinable thread and the program terminates.
    if (_renderWorker.joinable()) _renderWorker.join();

    _rendering = true;
    _cancelRender = false;
    _renderWorker = std::thread(&App::renderThread, this, outputPath, videoCodec,
                                quality, useBitrate, bitrateKbps, resize, width, height);
}

void App::cancelRender()
{
    _cancelRender = true;
}

void App::renderThread(std::string outputPath, std::string videoCodec, int quality,
                       bool useBitrate, int bitrateKbps, bool resize,
                       int width, int height)
{
    // Copy the composition: the render runs on another thread and the user can
    // keep editing while it does. A reference into the live document would be
    // a data race on every layer edit.
    const Composition comp = *(_document.activeComposition()
                                   ? _document.activeComposition()
                                   : &_document.compositions.front());
    const std::vector<Asset> assets = _document.assets;

    const int frameCount = comp.frameCount();
    const double rate = comp.fps();

    std::filesystem::path temp = std::filesystem::temp_directory_path()
                               / ("keyflow-render-" + std::to_string(GetTickCount64()));
    std::error_code ec;
    std::filesystem::create_directories(temp, ec);
    if (ec) {
        _bridge->sendRenderComplete(outputPath, false,
                                    "could not create a temporary directory");
        _rendering = false;
        return;
    }

    RenderSettings settings;
    settings.width = resize ? width : comp.width;
    settings.height = resize ? height : comp.height;
    settings.preview = false;

    // Frame by frame: render, read back, write a PNG.
    for (int frame = 0; frame < frameCount; ++frame) {
        if (_cancelRender.load()) {
            std::filesystem::remove_all(temp, ec);
            _bridge->sendRenderComplete(outputPath, false, "cancelled");
            _rendering = false;
            return;
        }

        const double time = frame / rate;
        settings.time = time;
        settings.frame = frame;

        std::vector<Frame> frames(comp.layers.size());
        for (std::size_t index = 0; index < comp.layers.size(); ++index) {
            const Layer& layer = comp.layers[index];
            if (layer.kind != LayerKind::Media || layer.assetId.empty()) continue;
            if (!layer.activeAt(time)) continue;

            const Asset* asset = nullptr;
            for (const Asset& candidate : assets) {
                if (candidate.id == layer.assetId) { asset = &candidate; break; }
            }
            if (asset == nullptr) continue;

            std::string error;
            const double sourceTime = std::max(0.0, time - layer.inPoint);
            decodeFrameAt(_tools, asset->path, sourceTime,
                          settings.width, settings.height, frames[index], error);
        }

        std::string error;
        if (!_renderer.renderFrame(comp, settings, frames, error)) {
            std::filesystem::remove_all(temp, ec);
            _bridge->sendRenderComplete(outputPath, false, "frame " + std::to_string(frame)
                                                               + ": " + error);
            _rendering = false;
            return;
        }

        Frame result;
        if (!_renderer.readback(result, error)) {
            std::filesystem::remove_all(temp, ec);
            _bridge->sendRenderComplete(outputPath, false, "readback: " + error);
            _rendering = false;
            return;
        }

        // A numbered PNG sequence, which ffmpeg then turns into the video. The
        // alternative — piping raw frames to ffmpeg's stdin — needs the render
        // and the encode to stay in lockstep, and a stall on either side
        // deadlocks both.
        char name[64];
        std::snprintf(name, sizeof(name), "frame_%06d.png", frame);
        const std::filesystem::path framePath = temp / name;

        if (!writePng(result, framePath.string(), error)) {
            std::filesystem::remove_all(temp, ec);
            _bridge->sendRenderComplete(outputPath, false, "could not write " + framePath.string());
            _rendering = false;
            return;
        }

        _bridge->sendRenderProgress(frame + 1, frameCount, "Rendering");
    }

    // Encode.
    _bridge->sendRenderProgress(frameCount, frameCount, "Encoding");

    EncodeSettings encode;
    encode.outputPath = outputPath;
    encode.videoCodec = videoCodec;
    encode.quality = quality;
    encode.useBitrate = useBitrate;
    encode.bitrateKbps = bitrateKbps;
    encode.fpsNumerator = comp.fpsNumerator;
    encode.fpsDenominator = comp.fpsDenominator;
    encode.resize = resize;
    encode.width = width;
    encode.height = height;

    const std::string pattern = (temp / "frame_%06d.png").string();
    std::string error;
    std::string ffmpegOutput;
    const int code = runProcessCapture(_tools.ffmpeg,
                                       buildEncodeArguments(encode, pattern),
                                       ffmpegOutput, error);

    std::filesystem::remove_all(temp, ec);

    if (code != 0) {
        _bridge->sendRenderComplete(outputPath, false,
                                    error.empty() ? "ffmpeg failed" : error);
    } else {
        _bridge->sendRenderComplete(outputPath, true, "done");
    }
    _rendering = false;
}

void App::requestQuit()
{
    if (_window) DestroyWindow(_window);
}

void App::reportStatus(const std::string& message)
{
    json::Value out;
    out.set("type", "status");
    out.set("message", message);
    sendToPage(json::write(out));
}

// --- run ------------------------------------------------------------------

bool App::create(HINSTANCE instance, int showCommand)
{
    _instance = instance;

    if (!createWindow(instance, showCommand)) {
        MessageBoxW(nullptr, L"Keyflow could not create its window.",
                    L"Keyflow", MB_OK | MB_ICONERROR);
        return false;
    }

    _tools = locateFFmpeg();

    // The renderer is initialised before the page loads, so the first frame
    // request does not race it.
    std::string rendererError;
    if (!_renderer.initialise(rendererError)) {
        // Not fatal: the model, the project files and the interface all work
        // without a GL context. The preview says why it is blank.
        _rendererError = rendererError;
    }

    _document = Project::createDefault("Untitled");
    _bridge = std::make_unique<Bridge>(
        _document,
        [this](const std::string& text) { sendToPage(text); },
        this);

    if (!createWebView()) {
        MessageBoxW(_window,
                    L"Keyflow could not start its interface. The Microsoft Edge "
                    L"WebView2 runtime is required.",
                    L"Keyflow", MB_OK | MB_ICONERROR);
        return false;
    }

    return true;
}

int App::run()
{
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    // Stop the render worker before the renderer is destroyed: it holds a
    // reference to the GL context, and tearing that down under it is a crash
    // on exit.
    _cancelRender = true;
    if (_renderWorker.joinable()) _renderWorker.join();

    return static_cast<int>(message.wParam);
}

bool App::initialiseForSelfTest(std::string& error)
{
    return _renderer.initialise(error);
}

namespace {

/// Append a line to the self-test log.
///
/// OutputDebugString reaches a debugger, which a CI runner does not have. The
/// self-test is meant to be checked by a script, so the same lines also go to
/// a file beside the frame it writes.
void logSelfTest(const std::string& line)
{
    OutputDebugStringA(line.c_str());
    OutputDebugStringA("\n");
    std::ofstream out("self_test.log", std::ios::app | std::ios::binary);
    if (out) out << line << "\n";
}

} // namespace

int App::runSelfTest(const std::string& outputPath)
{
    // A fresh log per run: a reader must never see a previous run's failures
    // and mistake them for this one's.
    { std::ofstream truncate("self_test.log", std::ios::trunc); }

    // A composition with one of each generated layer kind, so the test covers
    // the paths a real project would take rather than a single flat fill.
    Project project = Project::createDefault("Self test");
    Composition& comp = project.compositions.front();
    comp.width = 320;
    comp.height = 180;
    comp.duration = 1.0;
    comp.backgroundR = 0.05;
    comp.backgroundG = 0.06;
    comp.backgroundB = 0.09;

    Layer solid;
    solid.id = "self-solid";
    solid.name = "Solid";
    solid.kind = LayerKind::Solid;
    solid.solidR = 0.9;
    solid.solidG = 0.3;
    solid.solidB = 0.15;
    solid.outPoint = comp.duration;
    comp.layers.push_back(solid);

    _document = project;

    std::string error;
    if (!_renderer.ready()) {
        logSelfTest("[self-test] the renderer did not start: " + _rendererError);
        return 2;
    }
    logSelfTest("[self-test] renderer: " + _renderer.driverInfo());

    // Every shader in the table must compile. This is the one check the unit
    // tests cannot make, because they have no GL context — and a shader that
    // does not compile is an effect that renders nothing with no error.
    std::string shaderError;
    const std::vector<std::string> failedShaders = _renderer.validateShaders(shaderError);
    for (const std::string& stem : failedShaders) {
        logSelfTest("[self-test] SHADER FAILED: " + stem + ": " + shaderError);
    }
    logSelfTest("[self-test] shaders compiled: "
                + std::to_string(shaderStems().size() - failedShaders.size())
                + " of " + std::to_string(shaderStems().size()));

    RenderSettings settings;
    settings.width = comp.width;
    settings.height = comp.height;
    settings.time = 0.0;
    settings.preview = false;

    std::vector<Frame> frames(comp.layers.size());
    if (!_renderer.renderFrame(comp, settings, frames, error)) {
        logSelfTest("[self-test] RENDER FAILED: " + error);
        return 3;
    }

    Frame result;
    if (!_renderer.readback(result, error)) {
        logSelfTest("[self-test] READBACK FAILED: " + error);
        return 4;
    }

    if (!writePng(result, outputPath, error)) {
        logSelfTest("[self-test] WRITE FAILED: " + error);
        return 5;
    }

    // Report the corner pixel, so a caller can tell a rendered frame from a
    // black one without decoding the PNG.
    const std::size_t last = result.pixels.size() - 4;
    // --- the blend modes ---------------------------------------------------
    //
    // A blend that is not applied at all renders every mode as a plain
    // source-over, which looks like "the colours are wrong" rather than like a
    // missing feature. The shader-compilation check cannot see it: the shader
    // compiles either way.
    {
        std::string blendError;
        if (!_renderer.checkBlendModes(blendError)) {
            logSelfTest("[self-test] BLEND CHECK FAILED: " + blendError);
            return 7;
        }
        logSelfTest("[self-test] blend modes: Normal, Multiply and Screen correct");
    }

    logSelfTest("[self-test] frame written: " + outputPath);
    logSelfTest("[self-test] corner pixel rgba: "
                + std::to_string(result.pixels[last]) + " "
                + std::to_string(result.pixels[last + 1]) + " "
                + std::to_string(result.pixels[last + 2]));

    return failedShaders.empty() ? 0 : 1;
}

} // namespace
} // namespace keyflow

// --- entry point ----------------------------------------------------------

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int showCommand)
{
    // A layout report is written when this switch is present. It is how CI and
    // the harness check the interface without a human looking at it.
    const bool layoutReport =
        wcsstr(GetCommandLineW(), L"--layout-report") != nullptr;

    // --self-test <path>: render one frame and quit. The exit code says what
    // failed, so a script can check it without reading any output.
    if (wcsstr(GetCommandLineW(), L"--self-test") != nullptr) {
        keyflow::App app;
        std::string error;
        if (!app.initialiseForSelfTest(error)) {
            OutputDebugStringA("[self-test] could not start: ");
            OutputDebugStringA(error.c_str());
            OutputDebugStringA("\n");
            return 6;
        }
        return app.runSelfTest("self_test.png");
    }
    // Per-monitor DPI aware, so the interface is crisp on a scaled display
    // rather than blurry upscaled by the compositor.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // The common controls the file dialogs use.
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);

    keyflow::App app;
    app.setShowLayoutReport(layoutReport);
    if (!app.create(instance, showCommand)) return 1;
    return app.run();
}
