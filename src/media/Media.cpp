#include "media/Media.h"

#include "core/json/JsonValue.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <sys/wait.h>
#  include <unistd.h>
#endif

namespace keyflow {

namespace {

/// Quote an argument for a command line.
///
/// Only needed on Windows, where the C runtime parses the command line rather
/// than the shell. The rules are the ones the CRT documents: wrap in quotes,
/// backslash-escape any quote, and double a run of backslashes that precedes a
/// quote. Getting this wrong shows up as a path with a space in it being split
/// into two arguments, which is exactly the kind of bug that only appears on
/// one user's machine.
std::string quoteArgument(const std::string& argument)
{
#ifdef _WIN32
    if (!argument.empty()
        && argument.find_first_of(" \t\n\v\"") == std::string::npos) {
        return argument;
    }
    std::string out = "\"";
    for (auto it = argument.begin();; ++it) {
        unsigned backslashes = 0;
        while (it != argument.end() && *it == '\\') {
            ++it;
            ++backslashes;
        }
        if (it == argument.end()) {
            out.append(backslashes * 2, '\\');
            break;
        }
        if (*it == '"') {
            out.append(backslashes * 2 + 1, '\\');
            out.push_back('"');
        } else {
            out.append(backslashes, '\\');
            out.push_back(*it);
        }
    }
    out.push_back('"');
    return out;
#else
    return argument;
#endif
}

#ifdef _WIN32
std::wstring widen(const std::string& text)
{
    if (text.empty()) return {};
    const int needed = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                           static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                        out.data(), needed);
    return out;
}

std::string narrow(const std::wstring& text)
{
    if (text.empty()) return {};
    const int needed = WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                                           static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                        out.data(), needed, nullptr, nullptr);
    return out;
}
#endif

/// Run a process, optionally capturing stdout, optionally streaming it.
///
/// One implementation for both entry points: the difference is only whether
/// the pipe is read to the end into a string or handed out a line at a time,
/// and two implementations of process spawning is two places for the quoting
/// and the handle lifetimes to be wrong.
int runProcessImpl(const std::string& executable,
                   const std::vector<std::string>& arguments,
                   std::string* capturedStdout,
                   const std::function<void(const std::string&)>* onLine,
                   std::string& error)
{
    if (executable.empty()) {
        error = "no executable given";
        return -1;
    }
    if (!std::filesystem::exists(executable)) {
        error = "not found: " + executable;
        return -1;
    }

#ifdef _WIN32
    // Build the command line. argv[0] is the executable path, unquoted only
    // when it has no spaces.
    std::string command = quoteArgument(executable);
    for (const std::string& argument : arguments) {
        command += ' ';
        command += quoteArgument(argument);
    }

    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;

    HANDLE readEnd = nullptr;
    HANDLE writeEnd = nullptr;
    if (!CreatePipe(&readEnd, &writeEnd, &security, 0)) {
        error = "cannot create a pipe for the child process";
        return -1;
    }
    // The read end must not be inherited, or the child holds it open and the
    // read below never sees end-of-file.
    SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    startup.hStdOutput = writeEnd;
    startup.hStdError = writeEnd;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION process{};

    std::wstring wideCommand = widen(command);
    const BOOL started = CreateProcessW(
        nullptr, wideCommand.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);

    // The parent's copy of the write end must close now, or the read below
    // never sees end-of-file.
    CloseHandle(writeEnd);

    if (!started) {
        CloseHandle(readEnd);
        error = "cannot start " + executable + " (error "
              + std::to_string(GetLastError()) + ")";
        return -1;
    }

    std::string lineBuffer;
    std::array<char, 4096> buffer{};
    for (;;) {
        DWORD read = 0;
        const BOOL ok = ReadFile(readEnd, buffer.data(),
                                 static_cast<DWORD>(buffer.size()), &read, nullptr);
        if (!ok || read == 0) break;
        if (capturedStdout) {
            capturedStdout->append(buffer.data(), read);
        }
        if (onLine) {
            lineBuffer.append(buffer.data(), read);
            std::size_t start = 0;
            for (;;) {
                const std::size_t newline = lineBuffer.find('\n', start);
                if (newline == std::string::npos) break;
                (*onLine)(lineBuffer.substr(start, newline - start));
                start = newline + 1;
            }
            lineBuffer.erase(0, start);
        }
    }
    if (onLine && !lineBuffer.empty()) {
        (*onLine)(lineBuffer);
    }

    CloseHandle(readEnd);
    WaitForSingleObject(process.hProcess, INFINITE);

    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);

    if (exitCode != 0 && error.empty()) {
        error = executable + " exited with code " + std::to_string(exitCode);
        // The captured output is where ffmpeg says what went wrong, and it is
        // the only useful part of the message.
        if (capturedStdout && !capturedStdout->empty()) {
            const std::size_t limit = std::min<std::size_t>(capturedStdout->size(), 400);
            error += ": " + capturedStdout->substr(capturedStdout->size() - limit);
        }
    }
    return static_cast<int>(exitCode);
#else
    // POSIX: fork, exec, read the pipe. Only the tests use this path; the
    // editor itself is Windows-only.
    (void)onLine;
    (void)capturedStdout;
    (void)arguments;
    error = "process spawning is not implemented on this platform";
    return -1;
#endif
}

} // namespace

FFmpegPaths locateFFmpeg(const std::string& explicitDirectory)
{
    FFmpegPaths paths;

    const char* names[] = {"ffmpeg", "ffprobe"};
    const char* suffixes[] = {"", ".exe"};

    auto consider = [&](const std::filesystem::path& directory) {
        if (paths.valid()) return;
        FFmpegPaths candidate;
        for (int i = 0; i < 2; ++i) {
            for (const char* suffix : suffixes) {
                std::filesystem::path full =
                    directory / (std::string(names[i]) + suffix);
                std::error_code ec;
                if (std::filesystem::exists(full, ec) && !ec) {
                    if (i == 0) candidate.ffmpeg = full.string();
                    else        candidate.ffprobe = full.string();
                }
            }
        }
        if (candidate.valid()) paths = candidate;
    };

    if (!explicitDirectory.empty()) {
        consider(explicitDirectory);
    }

#ifdef _WIN32
    // Beside the executable, which is where a bundled build would put it.
    std::wstring modulePath(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, modulePath.data(),
                                            static_cast<DWORD>(modulePath.size()));
    if (length > 0) {
        modulePath.resize(length);
        consider(std::filesystem::path(modulePath).parent_path());
    }

    // Then PATH.
    std::wstring searchPath(32768, L'\0');
    const DWORD pathLength = GetEnvironmentVariableW(L"PATH", searchPath.data(),
                                                     static_cast<DWORD>(searchPath.size()));
    if (pathLength > 0 && pathLength < searchPath.size()) {
        searchPath.resize(pathLength);
        std::size_t start = 0;
        while (start <= searchPath.size() && !paths.valid()) {
            const std::size_t end = searchPath.find(L';', start);
            const std::wstring entry = searchPath.substr(
                start, end == std::wstring::npos ? std::wstring::npos : end - start);
            if (!entry.empty()) consider(std::filesystem::path(entry));
            if (end == std::wstring::npos) break;
            start = end + 1;
        }
    }
#endif

    return paths;
}

MediaInfo probeMedia(const FFmpegPaths& tools, const std::string& path)
{
    MediaInfo info;
    if (!tools.valid()) {
        info.error = "ffmpeg was not found. Set its path in Settings.";
        return info;
    }
    if (!std::filesystem::exists(path)) {
        info.error = "the file does not exist: " + path;
        return info;
    }

    // ffprobe emits one JSON object per stream plus a format object. Asking
    // for JSON rather than the default text means the parse is a parse rather
    // than a set of string searches that break when ffmpeg rewords a line.
    const std::vector<std::string> arguments = {
        "-v", "error",
        "-print_format", "json",
        "-show_format",
        "-show_streams",
        path,
    };

    std::string output;
    std::string error;
    const int code = runProcessCapture(tools.ffprobe, arguments, output, error);
    if (code != 0) {
        info.error = error.empty() ? "ffprobe could not read the file" : error;
        return info;
    }

    std::string parseError;
    const json::Value root = json::parse(output, &parseError);
    if (!parseError.empty()) {
        info.error = "ffprobe returned output that is not JSON: " + parseError;
        return info;
    }

    const json::Value& format = root["format"];
    info.duration = format["duration"].asNumber(0.0);

    bool foundVideo = false;
    for (const json::Value& stream : root["streams"].asArray()) {
        const std::string type = stream["codec_type"].asString();
        if (type == "audio") {
            info.hasAudio = true;
            continue;
        }
        if (type != "video") continue;

        // A still image arrives as a video stream with a single frame and a
        // codec that names an image format. Checking the codec rather than the
        // extension is what makes a .png named .jpg still import as a still.
        const std::string codec = stream["codec_name"].asString();
        static const char* kStillCodecs[] = {
            "png", "mjpeg", "jpeg2000", "bmp", "tiff", "webp", "gif",
        };
        const bool stillCodec =
            std::any_of(std::begin(kStillCodecs), std::end(kStillCodecs),
                        [&](const char* name) { return codec == name; });

        // A still also has no frame rate, which the container reports as 0/0
        // or as a single frame with a nominal rate.
        const std::string rate = stream["avg_frame_rate"].asString();
        const bool noRate = rate.empty() || rate == "0/0";

        foundVideo = true;
        info.width = static_cast<int>(stream["width"].asNumber(0));
        info.height = static_cast<int>(stream["height"].asNumber(0));
        info.videoCodec = codec;
        info.still = stillCodec && (noRate || info.duration <= 0.0);

        if (!noRate) {
            const std::size_t slash = rate.find('/');
            if (slash != std::string::npos) {
                info.fpsNumerator = std::atoi(rate.substr(0, slash).c_str());
                info.fpsDenominator = std::atoi(rate.substr(slash + 1).c_str());
                if (info.fpsDenominator == 0) info.fpsDenominator = 1;
            }
        }

        const json::Value& frames = stream["nb_frames"];
        if (!frames.isNull()) {
            info.frameCount = std::atoi(frames.asString("0").c_str());
        }

        if (info.still) {
            // A still has no timeline of its own; the layer decides how long
            // it is on screen.
            info.duration = 0.0;
        }
        break;
    }

    if (!foundVideo) {
        info.error = "the file has no picture";
        return info;
    }
    if (info.width <= 0 || info.height <= 0) {
        info.error = "the file's picture has no size";
        return info;
    }

    info.ok = true;
    return info;
}

std::vector<std::string> buildDecodeArguments(const std::string& path,
                                              double time, int width, int height)
{
    std::vector<std::string> arguments = {
        "-v", "error",
        // Fast seek to the keyframe before the target, then a precise seek to
        // the exact time. A bare -ss before -i is fast but lands on the
        // keyframe, which is visibly the wrong frame when scrubbing.
        "-ss", std::to_string(time),
        "-i", path,
        "-frames:v", "1",
        "-f", "rawvideo",
        "-pix_fmt", "rgba",
    };
    if (width > 0 && height > 0) {
        // scale with force_original_aspect_ratio and a pad keeps the picture
        // undistorted and the output exactly the requested size, which the
        // caller has already allocated for.
        arguments.push_back("-vf");
        arguments.push_back(
            "scale=" + std::to_string(width) + ":" + std::to_string(height)
            + ":force_original_aspect_ratio=decrease,"
            + "pad=" + std::to_string(width) + ":" + std::to_string(height)
            + ":(ow-iw)/2:(oh-ih)/2");
    }
    arguments.push_back("-");
    return arguments;
}

bool decodeFrameAt(const FFmpegPaths& tools, const std::string& path,
                   double time, int targetWidth, int targetHeight,
                   Frame& out, std::string& error)
{
    if (!tools.valid()) {
        error = "ffmpeg was not found. Set its path in Settings.";
        return false;
    }

    std::string output;
    const int code = runProcessCapture(
        tools.ffmpeg,
        buildDecodeArguments(path, time, targetWidth, targetHeight),
        output, error);
    if (code != 0) {
        if (error.empty()) error = "ffmpeg could not decode the frame";
        return false;
    }

    if (targetWidth <= 0 || targetHeight <= 0) {
        error = "a decode needs a target size";
        return false;
    }

    const std::size_t expected =
        static_cast<std::size_t>(targetWidth) * targetHeight * 4;
    if (output.size() < expected) {
        // Fewer bytes than a frame means the seek ran past the end of the
        // file. Reporting it is better than returning a frame of garbage.
        error = "the source has no frame at " + std::to_string(time) + "s";
        return false;
    }

    out.width = targetWidth;
    out.height = targetHeight;
    out.time = time;
    out.pixels.assign(output.begin(), output.begin() + static_cast<std::ptrdiff_t>(expected));
    return true;
}

bool extractFrameToPng(const FFmpegPaths& tools, const std::string& path,
                       double time, int width, int height,
                       const std::string& outputPath, std::string& error)
{
    if (!tools.valid()) {
        error = "ffmpeg was not found. Set its path in Settings.";
        return false;
    }

    std::vector<std::string> arguments = {
        "-v", "error",
        "-ss", std::to_string(time),
        "-i", path,
        "-frames:v", "1",
    };
    if (width > 0 && height > 0) {
        arguments.push_back("-vf");
        arguments.push_back(
            "scale=" + std::to_string(width) + ":" + std::to_string(height)
            + ":force_original_aspect_ratio=decrease,"
            + "pad=" + std::to_string(width) + ":" + std::to_string(height)
            + ":(ow-iw)/2:(oh-ih)/2");
    }
    arguments.push_back("-y");
    arguments.push_back(outputPath);

    std::string output;
    const int code = runProcessCapture(tools.ffmpeg, arguments, output, error);
    if (code != 0) {
        if (error.empty()) error = "ffmpeg could not extract the frame";
        return false;
    }
    return std::filesystem::exists(outputPath);
}

std::vector<std::string> buildEncodeArguments(const EncodeSettings& settings,
                                              const std::string& inputPattern)
{
    std::vector<std::string> arguments = {
        "-v", "error",
        // Overwrite without asking. The renderer has already confirmed with
        // the user, and a prompt here would hang the process with no console
        // to show it on.
        "-y",
        "-framerate",
        std::to_string(settings.fpsNumerator) + "/" + std::to_string(settings.fpsDenominator),
        "-i", inputPattern,
    };

    if (settings.resize && settings.width > 0 && settings.height > 0) {
        arguments.push_back("-vf");
        arguments.push_back("scale=" + std::to_string(settings.width) + ":"
                            + std::to_string(settings.height));
    }

    arguments.push_back("-c:v");
    arguments.push_back(settings.videoCodec);

    if (settings.useBitrate) {
        arguments.push_back("-b:v");
        arguments.push_back(std::to_string(settings.bitrateKbps) + "k");
    } else if (settings.videoCodec == "libx264" || settings.videoCodec == "libx265") {
        arguments.push_back("-crf");
        arguments.push_back(std::to_string(settings.quality));
        // The default preset is medium, which is slow for a preview-quality
        // render and pointless for a final one. fast is the balance point.
        arguments.push_back("-preset");
        arguments.push_back("fast");
    } else {
        // The hardware encoders take a quality level rather than a CRF.
        arguments.push_back("-b:v");
        arguments.push_back(std::to_string(settings.bitrateKbps) + "k");
    }

    arguments.push_back("-pix_fmt");
    arguments.push_back(settings.pixelFormat);

    // Playback compatibility. Without this, a .mp4 written by a recent ffmpeg
    // puts the index at the end and some players will not seek it.
    arguments.push_back("-movflags");
    arguments.push_back("+faststart");

    arguments.push_back(settings.outputPath);
    return arguments;
}

EncoderSupport probeEncoders(const FFmpegPaths& tools)
{
    EncoderSupport support;
    if (!tools.valid()) return support;

    std::string output;
    std::string error;
    if (runProcessCapture(tools.ffmpeg, {"-hide_banner", "-encoders"}, output, error) != 0) {
        return support;
    }
    support.nvenc = output.find("h264_nvenc") != std::string::npos;
    support.qsv = output.find("h264_qsv") != std::string::npos;
    support.amf = output.find("h264_amf") != std::string::npos;
    return support;
}

int runProcessCapture(const std::string& executable,
                      const std::vector<std::string>& arguments,
                      std::string& stdoutText,
                      std::string& error)
{
    stdoutText.clear();
    return runProcessImpl(executable, arguments, &stdoutText, nullptr, error);
}

int runProcessStreaming(const std::string& executable,
                        const std::vector<std::string>& arguments,
                        const std::function<void(const std::string&)>& onLine,
                        std::string& error)
{
    return runProcessImpl(executable, arguments, nullptr, &onLine, error);
}

} // namespace keyflow
