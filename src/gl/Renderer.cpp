#include "gl/Renderer.h"

#include "core/EffectCatalog.h"
#include "gl/GlslCompat.h"
#include "gl/ShaderTable.h"

#include <algorithm>
#include <cstring>
#include <cmath>
#include <sstream>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#endif

// ---------------------------------------------------------------------------
// The GL entry points.
//
// Loaded at runtime rather than linked, because Windows only exports OpenGL
// 1.1 from opengl32.dll. Anything past that — shaders, framebuffers, vertex
// buffer objects — has to come through wglGetProcAddress. Loading the whole
// set explicitly also means a machine with a 1.1-only driver fails at startup
// with a clear message instead of at the first glCreateShader call.
// ---------------------------------------------------------------------------

namespace keyflow {
namespace {

// Types, spelled out so this header set does not need <GL/gl.h>.
using GLenum = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLfloat = float;
using GLchar = char;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLubyte = unsigned char;

constexpr GLenum GL_TEXTURE_2D = 0x0DE1;
constexpr GLenum GL_TEXTURE0 = 0x84C0;
constexpr GLenum GL_FRAMEBUFFER = 0x8D40;
constexpr GLenum GL_COLOR_ATTACHMENT0 = 0x8CE0;
constexpr GLenum GL_FRAMEBUFFER_COMPLETE = 0x8CD5;
constexpr GLenum GL_RGBA = 0x1908;
constexpr GLenum GL_RGBA8 = 0x8058;
constexpr GLenum GL_UNSIGNED_BYTE = 0x1401;
constexpr GLenum GL_FLOAT = 0x1406;
constexpr GLenum GL_TRIANGLE_STRIP = 0x0005;
constexpr GLenum GL_ARRAY_BUFFER = 0x8892;
constexpr GLenum GL_STATIC_DRAW = 0x88E4;
constexpr GLenum GL_VERTEX_SHADER = 0x8B31;
constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30;
constexpr GLenum GL_COMPILE_STATUS = 0x8B81;
constexpr GLenum GL_LINK_STATUS = 0x8B82;
constexpr GLenum GL_INFO_LOG_LENGTH = 0x8B84;
constexpr GLenum GL_TEXTURE_MIN_FILTER = 0x2801;
constexpr GLenum GL_TEXTURE_MAG_FILTER = 0x2800;
constexpr GLenum GL_TEXTURE_WRAP_S = 0x2802;
constexpr GLenum GL_TEXTURE_WRAP_T = 0x2803;
constexpr GLenum GL_LINEAR = 0x2601;
constexpr GLenum GL_CLAMP_TO_EDGE = 0x812F;
constexpr GLenum GL_BLEND = 0x0BE2;
constexpr GLenum GL_ONE = 1;
constexpr GLenum GL_ONE_MINUS_SRC_ALPHA = 0x0303;
constexpr GLenum GL_NO_ERROR = 0;
constexpr GLenum GL_COLOR_BUFFER_BIT = 0x00004000;
constexpr GLenum GL_SCISSOR_TEST = 0x0C11;
constexpr GLenum GL_VIEWPORT = 0x0BA2;
constexpr GLenum GL_TEXTURE_BINDING_2D = 0x8069;
constexpr GLenum GL_ACTIVE_UNIFORMS = 0x8B86;

/// The subset of GL this renderer uses, resolved at startup.
struct GLFunctions
{
    // 1.1, exported directly.
    void (*Clear)(GLbitfield) = nullptr;
    void (*ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
    void (*Enable)(GLenum) = nullptr;
    void (*Disable)(GLenum) = nullptr;
    void (*Viewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
    void (*GenTextures)(GLsizei, GLuint*) = nullptr;
    void (*DeleteTextures)(GLsizei, const GLuint*) = nullptr;
    void (*BindTexture)(GLenum, GLuint) = nullptr;
    void (*TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) = nullptr;
    void (*TexParameteri)(GLenum, GLenum, GLint) = nullptr;
    void (*GetIntegerv)(GLenum, GLint*) = nullptr;
    const GLubyte* (*GetString)(GLenum) = nullptr;
    void (*DrawArrays)(GLenum, GLint, GLsizei) = nullptr;
    void (*ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*) = nullptr;
    void (*PixelStorei)(GLenum, GLint) = nullptr;
    void (*BlendFunc)(GLenum, GLenum) = nullptr;
    void (*Flush)() = nullptr;
    void (*Finish)() = nullptr;
    GLenum (*GetError)() = nullptr;

    // 2.0+, through wglGetProcAddress.
    GLuint (*CreateShader)(GLenum) = nullptr;
    void (*ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*) = nullptr;
    void (*CompileShader)(GLuint) = nullptr;
    void (*GetShaderiv)(GLuint, GLenum, GLint*) = nullptr;
    void (*GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
    void (*DeleteShader)(GLuint) = nullptr;
    GLuint (*CreateProgram)() = nullptr;
    void (*AttachShader)(GLuint, GLuint) = nullptr;
    void (*LinkProgram)(GLuint) = nullptr;
    void (*GetProgramiv)(GLuint, GLenum, GLint*) = nullptr;
    void (*GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
    void (*UseProgram)(GLuint) = nullptr;
    void (*DeleteProgram)(GLuint) = nullptr;
    GLint (*GetUniformLocation)(GLuint, const GLchar*) = nullptr;
    void (*Uniform1f)(GLint, GLfloat) = nullptr;
    void (*Uniform1i)(GLint, GLint) = nullptr;
    void (*Uniform2f)(GLint, GLfloat, GLfloat) = nullptr;
    void (*Uniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
    void (*UniformMatrix3fv)(GLint, GLsizei, GLboolean, const GLfloat*) = nullptr;
    void (*GenFramebuffers)(GLsizei, GLuint*) = nullptr;
    void (*DeleteFramebuffers)(GLsizei, const GLuint*) = nullptr;
    void (*BindFramebuffer)(GLenum, GLuint) = nullptr;
    void (*FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint) = nullptr;
    GLenum (*CheckFramebufferStatus)(GLenum) = nullptr;
    void (*GenBuffers)(GLsizei, GLuint*) = nullptr;
    void (*DeleteBuffers)(GLsizei, const GLuint*) = nullptr;
    void (*BindBuffer)(GLenum, GLuint) = nullptr;
    void (*BufferData)(GLenum, long long, const void*, GLenum) = nullptr;
    void (*EnableVertexAttribArray)(GLuint) = nullptr;
    void (*VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) = nullptr;
    void (*BindAttribLocation)(GLuint, GLuint, const GLchar*) = nullptr;
    GLint (*GetAttribLocation)(GLuint, const GLchar*) = nullptr;
    void (*ActiveTexture)(GLenum) = nullptr;
};

GLFunctions gl;

/// wglGetProcAddress returns a function pointer as a plain address; the cast
/// through void(*)() is what the API requires.
template <typename T>
bool loadProc(T& fn, const char* name)
{
#ifdef _WIN32
    fn = reinterpret_cast<T>(wglGetProcAddress(name));
    return fn != nullptr;
#else
    (void)fn;
    (void)name;
    return false;
#endif
}

/// Load the GL 1.1 entry points from opengl32.dll.
template <typename T>
bool loadGL11(T& fn, const char* name)
{
#ifdef _WIN32
    static HMODULE module = LoadLibraryA("opengl32.dll");
    if (module == nullptr) return false;
    fn = reinterpret_cast<T>(GetProcAddress(module, name));
    return fn != nullptr;
#else
    (void)fn;
    (void)name;
    return false;
#endif
}

/// 3x3 matrices, column-major, as GL wants them.
///
/// Only what the effects need: an affine map from the layer's own 0..1 space
/// into the target's uv space, and its inverse. Several shaders take one of
/// these and reconstruct the other direction from it.
struct Mat3
{
    float m[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};

    static Mat3 identity() { return {}; }

    static Mat3 translation(float x, float y)
    {
        Mat3 out;
        out.m[6] = x;
        out.m[7] = y;
        return out;
    }

    static Mat3 scale(float x, float y)
    {
        Mat3 out;
        out.m[0] = x;
        out.m[4] = y;
        return out;
    }

    static Mat3 rotation(float radians)
    {
        Mat3 out;
        const float c = std::cos(radians);
        const float s = std::sin(radians);
        out.m[0] = c;  out.m[1] = s;
        out.m[3] = -s; out.m[4] = c;
        return out;
    }

    Mat3 operator*(const Mat3& other) const
    {
        Mat3 out;
        for (int col = 0; col < 3; ++col) {
            for (int row = 0; row < 3; ++row) {
                float sum = 0.0f;
                for (int k = 0; k < 3; ++k) {
                    sum += m[k * 3 + row] * other.m[col * 3 + k];
                }
                out.m[col * 3 + row] = sum;
            }
        }
        return out;
    }
};

/// The affine map from a layer's local uv space to the target's uv space.
///
/// The layer's transform is in composition pixels with the anchor as the pivot;
/// this converts it to the normalised space the shaders work in. Getting the
/// anchor wrong here is the classic bug that makes a layer rotate about the
/// frame's corner instead of its own centre.
Mat3 layerToTarget(const Layer::Transform& t, const Composition& comp,
                   int layerWidth, int layerHeight)
{
    const float compW = static_cast<float>(comp.width);
    const float compH = static_cast<float>(comp.height);

    // The layer's own size in composition pixels, at unit scale.
    const float lw = static_cast<float>(layerWidth);
    const float lh = static_cast<float>(layerHeight);

    // Anchor in composition pixels, relative to the layer's top-left.
    const float anchorPx = static_cast<float>(t.anchorX) * lw;
    const float anchorPy = static_cast<float>(t.anchorY) * lh;

    // Position is the anchor's location in composition space.
    const float px = static_cast<float>(t.x);
    const float py = static_cast<float>(t.y);

    // Compose: local uv -> layer pixels -> translate by -anchor -> scale ->
    // rotate -> translate to the anchor's position -> composition uv.
    Mat3 toPixels = Mat3::scale(lw, lh);
    Mat3 toAnchor = Mat3::translation(-anchorPx, -anchorPy);
    Mat3 scale = Mat3::scale(static_cast<float>(t.scaleX),
                             static_cast<float>(t.scaleY));
    Mat3 rotate = Mat3::rotation(static_cast<float>(t.rotation) * 3.14159265358979f / 180.0f);
    Mat3 place = Mat3::translation(px, py);
    // Composition pixels to uv. Y is flipped: composition space counts down
    // from the top, and GL's uv counts up from the bottom.
    Mat3 toUv = Mat3::scale(1.0f / compW, -1.0f / compH);
    toUv.m[7] = 1.0f; // the flip's translation: y_uv = 1 - y_px / height

    return toUv * place * rotate * scale * toAnchor * toPixels;
}

/// The layer's box in target uv, as (minX, minY, maxX, maxY).
///
/// Every shader that holds or feathers an edge takes this, so it has to be the
/// real box after the transform rather than the layer's own 0..1.
void layerRect(const Mat3& m, float& minX, float& minY, float& maxX, float& maxY)
{
    const float corners[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    minX = minY = 1e9f;
    maxX = maxY = -1e9f;
    for (const auto& corner : corners) {
        const float x = m.m[0] * corner[0] + m.m[3] * corner[1] + m.m[6];
        const float y = m.m[1] * corner[0] + m.m[4] * corner[1] + m.m[7];
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    }
}

/// UTF-8 to UTF-16, for the text rasteriser.
std::wstring toWideForText(const std::string& text)
{
#ifdef _WIN32
    if (text.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                           static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                        out.data(), length);
    return out;
#else
    return std::wstring(text.begin(), text.end());
#endif
}

} // namespace
} // namespace keyflow

namespace keyflow {

// --- construction ---------------------------------------------------------

Renderer::Renderer() = default;

Renderer::~Renderer()
{
#ifdef _WIN32
    if (_glContext) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(static_cast<HGLRC>(_glContext));
    }
    if (_deviceContext && _window) {
        ReleaseDC(static_cast<HWND>(_window), static_cast<HDC>(_deviceContext));
    }
    if (_window) {
        DestroyWindow(static_cast<HWND>(_window));
    }
#endif
}

bool Renderer::initialise(std::string& error)
{
    if (_ready) return true;

#ifdef _WIN32
    // Load GL 1.1 first. Without even this there is no point making a window.
    bool ok = true;
    ok &= loadGL11(gl.Clear, "glClear");
    ok &= loadGL11(gl.ClearColor, "glClearColor");
    ok &= loadGL11(gl.Enable, "glEnable");
    ok &= loadGL11(gl.Disable, "glDisable");
    ok &= loadGL11(gl.Viewport, "glViewport");
    ok &= loadGL11(gl.GenTextures, "glGenTextures");
    ok &= loadGL11(gl.DeleteTextures, "glDeleteTextures");
    ok &= loadGL11(gl.BindTexture, "glBindTexture");
    ok &= loadGL11(gl.TexImage2D, "glTexImage2D");
    ok &= loadGL11(gl.TexParameteri, "glTexParameteri");
    ok &= loadGL11(gl.GetIntegerv, "glGetIntegerv");
    ok &= loadGL11(gl.GetString, "glGetString");
    ok &= loadGL11(gl.DrawArrays, "glDrawArrays");
    ok &= loadGL11(gl.ReadPixels, "glReadPixels");
    ok &= loadGL11(gl.PixelStorei, "glPixelStorei");
    ok &= loadGL11(gl.BlendFunc, "glBlendFunc");
    ok &= loadGL11(gl.Flush, "glFlush");
    ok &= loadGL11(gl.Finish, "glFinish");
    ok &= loadGL11(gl.GetError, "glGetError");

    if (!ok) {
        error = "opengl32.dll is missing the functions the renderer needs";
        return false;
    }

    // A hidden window, because a GL context needs a device context and a
    // device context needs a window. Hidden rather than a child of the app's
    // window so the renderer has no dependency on the UI's lifetime.
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"KeyflowOffscreenGL";

    // Registering twice fails harmlessly; the class already exists.
    RegisterClassExW(&wc);

    HWND window = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
                                  0, 0, 64, 64, nullptr, nullptr,
                                  GetModuleHandleW(nullptr), nullptr);
    if (window == nullptr) {
        error = "could not create the renderer's window";
        return false;
    }
    _window = window;
    HDC dc = GetDC(window);
    _deviceContext = dc;

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.cDepthBits = 0;
    pfd.iLayerType = PFD_MAIN_PLANE;

    const int format = ChoosePixelFormat(dc, &pfd);
    if (format == 0) {
        error = "the display driver offers no usable pixel format";
        return false;
    }
    if (!SetPixelFormat(dc, format, &pfd)) {
        error = "the display driver refused the pixel format";
        return false;
    }

    HGLRC context = wglCreateContext(dc);
    if (context == nullptr) {
        error = "could not create an OpenGL context";
        return false;
    }
    if (!wglMakeCurrent(dc, context)) {
        wglDeleteContext(context);
        error = "could not make the OpenGL context current";
        return false;
    }
    _glContext = context;

    // Now the 2.0 entry points, which only exist once a context is current.
    bool ok2 = true;
    ok2 &= loadProc(gl.CreateShader, "glCreateShader");
    ok2 &= loadProc(gl.ShaderSource, "glShaderSource");
    ok2 &= loadProc(gl.CompileShader, "glCompileShader");
    ok2 &= loadProc(gl.GetShaderiv, "glGetShaderiv");
    ok2 &= loadProc(gl.GetShaderInfoLog, "glGetShaderInfoLog");
    ok2 &= loadProc(gl.DeleteShader, "glDeleteShader");
    ok2 &= loadProc(gl.CreateProgram, "glCreateProgram");
    ok2 &= loadProc(gl.AttachShader, "glAttachShader");
    ok2 &= loadProc(gl.LinkProgram, "glLinkProgram");
    ok2 &= loadProc(gl.GetProgramiv, "glGetProgramiv");
    ok2 &= loadProc(gl.GetProgramInfoLog, "glGetProgramInfoLog");
    ok2 &= loadProc(gl.UseProgram, "glUseProgram");
    ok2 &= loadProc(gl.DeleteProgram, "glDeleteProgram");
    ok2 &= loadProc(gl.GetUniformLocation, "glGetUniformLocation");
    ok2 &= loadProc(gl.Uniform1f, "glUniform1f");
    ok2 &= loadProc(gl.Uniform1i, "glUniform1i");
    ok2 &= loadProc(gl.Uniform2f, "glUniform2f");
    ok2 &= loadProc(gl.Uniform4f, "glUniform4f");
    ok2 &= loadProc(gl.UniformMatrix3fv, "glUniformMatrix3fv");
    ok2 &= loadProc(gl.GenFramebuffers, "glGenFramebuffers");
    ok2 &= loadProc(gl.DeleteFramebuffers, "glDeleteFramebuffers");
    ok2 &= loadProc(gl.BindFramebuffer, "glBindFramebuffer");
    ok2 &= loadProc(gl.FramebufferTexture2D, "glFramebufferTexture2D");
    ok2 &= loadProc(gl.CheckFramebufferStatus, "glCheckFramebufferStatus");
    ok2 &= loadProc(gl.GenBuffers, "glGenBuffers");
    ok2 &= loadProc(gl.DeleteBuffers, "glDeleteBuffers");
    ok2 &= loadProc(gl.BindBuffer, "glBindBuffer");
    ok2 &= loadProc(gl.BufferData, "glBufferData");
    ok2 &= loadProc(gl.EnableVertexAttribArray, "glEnableVertexAttribArray");
    ok2 &= loadProc(gl.VertexAttribPointer, "glVertexAttribPointer");
    ok2 &= loadProc(gl.BindAttribLocation, "glBindAttribLocation");
    ok2 &= loadProc(gl.GetAttribLocation, "glGetAttribLocation");
    ok2 &= loadProc(gl.ActiveTexture, "glActiveTexture");

    if (!ok2) {
        error = "this machine's OpenGL driver is too old: the editor needs "
                "OpenGL 2.0 for its effect shaders. Updating the display "
                "driver usually fixes this.";
        return false;
    }

    // The quad: two triangles as a strip, position and uv interleaved.
    const float quad[] = {
        0.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
    };
    gl.GenBuffers(1, &_quadBuffer);
    gl.BindBuffer(GL_ARRAY_BUFFER, _quadBuffer);
    gl.BufferData(GL_ARRAY_BUFFER, static_cast<long long>(sizeof(quad)),
                  quad, GL_STATIC_DRAW);

    const GLubyte* version = gl.GetString(0x1F02);       // GL_VERSION
    const GLubyte* rendererName = gl.GetString(0x1F01);  // GL_RENDERER
    std::ostringstream info;
    info << (version ? reinterpret_cast<const char*>(version) : "unknown")
         << " -- " << (rendererName ? reinterpret_cast<const char*>(rendererName) : "unknown");
    _driverInfo = info.str();

    _ready = true;
    return true;
#else
    error = "the renderer is Windows-only";
    return false;
#endif
}

// --- programs -------------------------------------------------------------

unsigned int Renderer::compileProgram(const std::string& vertexSource,
                                      const std::string& fragmentSource,
                                      std::string& error)
{
    auto compile = [&](GLenum type, const std::string& source, const char* stage) -> GLuint {
        const GLuint shader = gl.CreateShader(type);
        if (shader == 0) {
            error = std::string("could not create the ") + stage + " shader";
            return 0;
        }
        const GLchar* text = source.c_str();
        gl.ShaderSource(shader, 1, &text, nullptr);
        gl.CompileShader(shader);

        GLint status = 0;
        gl.GetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (status == 0) {
            GLint length = 0;
            gl.GetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
            std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
            gl.GetShaderInfoLog(shader, length, nullptr, log.data());
            error = std::string("the ") + stage + " shader did not compile: " + log;
            gl.DeleteShader(shader);
            return 0;
        }
        return shader;
    };

    const GLuint vs = compile(GL_VERTEX_SHADER, vertexSource, "vertex");
    if (vs == 0) return 0;
    const GLuint fs = compile(GL_FRAGMENT_SHADER, fragmentSource, "fragment");
    if (fs == 0) {
        gl.DeleteShader(vs);
        return 0;
    }

    const GLuint program = gl.CreateProgram();
    gl.AttachShader(program, vs);
    gl.AttachShader(program, fs);

    // Bind the attribute names before linking. GLSL ES has no layout
    // qualifiers, so without this the locations are whatever the driver picks
    // and the vertex pointer setup has to query them every draw.
    gl.BindAttribLocation(program, 0, "aPos");
    gl.BindAttribLocation(program, 1, "aTex");
    gl.BindAttribLocation(program, 0, "aPosition");
    gl.BindAttribLocation(program, 1, "aTexCoord");

    gl.LinkProgram(program);

    GLint status = 0;
    gl.GetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == 0) {
        GLint length = 0;
        gl.GetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
        gl.GetProgramInfoLog(program, length, nullptr, log.data());
        error = "the shader program did not link: " + log;
        gl.DeleteProgram(program);
        gl.DeleteShader(vs);
        gl.DeleteShader(fs);
        return 0;
    }

    // The shaders are attached to the program, so the objects can go. The
    // linked program keeps what it needs.
    gl.DeleteShader(vs);
    gl.DeleteShader(fs);
    return program;
}

unsigned int Renderer::programFor(const std::string& stem, std::string& error)
{
    for (const auto& [name, program] : _programs) {
        if (name == stem) return program;
    }

    const std::string source = shaderSource(stem);
    if (source.empty()) {
        error = "there is no shader called '" + stem + "'";
        return 0;
    }

    // The shaders are GLSL ES 1.00 from the Android build. Desktop GL is a
    // different language in three small ways that all show up as a syntax
    // error at compile time, so the source is rewritten once here rather than
    // maintained as a second copy.
    //
    // The context is a compatibility profile, so `texture2D` and `varying`
    // are still accepted and only the precision statements have to go.
    const bool modern = false;

    // A vertex unit needs no fragment half and vice versa; the stem tells us
    // which by its prefix.
    unsigned int program = 0;
    if (stem.rfind("vs_", 0) == 0) {
        const std::string fragment =
            "void main() { gl_FragColor = vec4(1.0); }\n";
        program = compileProgram(toDesktopGlsl(source, modern),
                                 toDesktopGlsl(fragment, modern),
                                 error);
    } else {
        program = compileProgram(toDesktopGlsl(passthroughVertexShader(), modern),
                                 toDesktopGlsl(source, modern),
                                 error);
    }
    if (program == 0) return 0;

    _programs.emplace_back(stem, program);
    return program;
}

// --- uniforms -------------------------------------------------------------

void Renderer::setUniform(unsigned int program, const std::string& name, double value)
{
    const GLint location = gl.GetUniformLocation(program, name.c_str());
    if (location >= 0) gl.Uniform1f(location, static_cast<GLfloat>(value));
}

void Renderer::setUniform(unsigned int program, const std::string& name, float x, float y)
{
    const GLint location = gl.GetUniformLocation(program, name.c_str());
    if (location >= 0) gl.Uniform2f(location, x, y);
}

void Renderer::setUniform(unsigned int program, const std::string& name,
                          float x, float y, float z, float w)
{
    const GLint location = gl.GetUniformLocation(program, name.c_str());
    if (location >= 0) gl.Uniform4f(location, x, y, z, w);
}

void Renderer::setUniformMatrix3(unsigned int program, const std::string& name,
                                 const float* values)
{
    const GLint location = gl.GetUniformLocation(program, name.c_str());
    if (location >= 0) gl.UniformMatrix3fv(location, 1, 0, values);
}

// --- targets --------------------------------------------------------------

Renderer::Target Renderer::acquireTarget(int width, int height)
{
    width = std::max(width, 1);
    height = std::max(height, 1);

    for (auto it = _targetPool.begin(); it != _targetPool.end(); ++it) {
        if (it->width == width && it->height == height) {
            Target found = *it;
            _targetPool.erase(it);
            return found;
        }
    }

    Target target;
    target.width = width;
    target.height = height;

    gl.GenTextures(1, &target.texture);
    gl.BindTexture(GL_TEXTURE_2D, target.texture);
    gl.TexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(GL_RGBA8), width, height, 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(GL_LINEAR));
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(GL_LINEAR));
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(GL_CLAMP_TO_EDGE));
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(GL_CLAMP_TO_EDGE));

    gl.GenFramebuffers(1, &target.framebuffer);
    gl.BindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            target.texture, 0);

    if (gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        // A framebuffer that will not complete means the render cannot proceed.
        // Returning the handle anyway would produce a black frame with no
        // explanation, so it is reported at the point it is created.
        gl.DeleteFramebuffers(1, &target.framebuffer);
        gl.DeleteTextures(1, &target.texture);
        target.framebuffer = 0;
        target.texture = 0;
    }
    gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
    return target;
}

void Renderer::releaseResources()
{
    for (auto& [stem, program] : _programs) {
        (void)stem;
        if (program) gl.DeleteProgram(program);
    }
    _programs.clear();

    for (const Target& target : _targetPool) {
        if (target.framebuffer) gl.DeleteFramebuffers(1, &target.framebuffer);
        if (target.texture) gl.DeleteTextures(1, &target.texture);
    }
    _targetPool.clear();

    for (const Target& target : {_frame, _scratchA, _scratchB, _previous}) {
        if (target.framebuffer) gl.DeleteFramebuffers(1, &target.framebuffer);
        if (target.texture) gl.DeleteTextures(1, &target.texture);
    }
    _frame = _scratchA = _scratchB = _previous = Target{};

    for (const TextureId texture : _texturePool) {
        gl.DeleteTextures(1, &texture);
    }
    _texturePool.clear();
}

std::vector<std::string> Renderer::validateShaders(std::string& firstError)
{
    std::vector<std::string> failed;
    firstError.clear();

    for (const std::string& stem : shaderStems()) {
        std::string error;
        if (programFor(stem, error) == 0) {
            failed.push_back(stem);
            if (firstError.empty()) firstError = error;
        }
    }
    return failed;
}

// --- drawing --------------------------------------------------------------

void Renderer::drawFullscreenQuad()
{
    gl.BindBuffer(GL_ARRAY_BUFFER, _quadBuffer);
    // Stride 16 bytes: two floats of position then two of uv.
    gl.EnableVertexAttribArray(0);
    gl.VertexAttribPointer(0, 2, GL_FLOAT, 0, 16, nullptr);
    gl.EnableVertexAttribArray(1);
    gl.VertexAttribPointer(1, 2, GL_FLOAT, 0, 16,
                           reinterpret_cast<const void*>(8));
    gl.DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void Renderer::beginFrame(int width, int height)
{
    gl.BindFramebuffer(GL_FRAMEBUFFER, _frame.framebuffer);
    gl.Viewport(0, 0, width, height);
    gl.Disable(GL_BLEND);
    gl.Disable(GL_SCISSOR_TEST);
}

TextureId Renderer::upload(const Frame& frame)
{
    if (!frame.valid()) return 0;

    TextureId texture = 0;
    if (!_texturePool.empty()) {
        texture = _texturePool.back();
        _texturePool.pop_back();
    } else {
        gl.GenTextures(1, &texture);
    }

    gl.BindTexture(GL_TEXTURE_2D, texture);
    gl.PixelStorei(0x0CF5 /* GL_UNPACK_ALIGNMENT */, 1);

    // The decoder hands back top-row-first, and GL's texture space counts up
    // from the bottom. Flipping here rather than in every shader keeps the
    // shaders' uv convention the same as the Android app's.
    const int rowBytes = frame.width * 4;
    std::vector<GLubyte> flipped(static_cast<std::size_t>(rowBytes) * frame.height);
    for (int y = 0; y < frame.height; ++y) {
        std::copy_n(frame.pixels.data() + static_cast<std::ptrdiff_t>(y) * rowBytes,
                    rowBytes,
                    flipped.data() + static_cast<std::ptrdiff_t>(frame.height - 1 - y) * rowBytes);
    }

    gl.TexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(GL_RGBA8),
                  frame.width, frame.height, 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, flipped.data());
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(GL_LINEAR));
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(GL_LINEAR));
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(GL_CLAMP_TO_EDGE));
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(GL_CLAMP_TO_EDGE));

    return texture;
}

// --- the effect pass ------------------------------------------------------

bool Renderer::applyEffect(const Effect& effect, const EffectSpec& spec,
                           TextureId source, const Target& destination,
                           const Layer& layer, const Composition& comp,
                           const RenderSettings& settings,
                           const std::vector<double>& values,
                           TextureId backdrop, TextureId previousFrame,
                           std::string& error)
{
    const unsigned int program = programFor(spec.shader, error);
    if (program == 0) return false;

    gl.BindFramebuffer(GL_FRAMEBUFFER, destination.framebuffer);
    gl.Viewport(0, 0, destination.width, destination.height);
    gl.Disable(GL_BLEND);
    gl.UseProgram(program);

    // --- the samplers every effect pass gets -----------------------------
    gl.ActiveTexture(GL_TEXTURE0);
    gl.BindTexture(GL_TEXTURE_2D, source);
    setUniform(program, "uTexture", 0.0);

    if (backdrop != 0) {
        gl.ActiveTexture(GL_TEXTURE0 + 1);
        gl.BindTexture(GL_TEXTURE_2D, backdrop);
        setUniform(program, "uBackdrop", 1.0);
    }
    if (previousFrame != 0) {
        gl.ActiveTexture(GL_TEXTURE0 + 2);
        gl.BindTexture(GL_TEXTURE_2D, previousFrame);
        setUniform(program, "uPrev", 2.0);
    }

    // --- the frame constants ---------------------------------------------
    const float targetW = static_cast<float>(destination.width);
    const float targetH = static_cast<float>(destination.height);
    setUniform(program, "uTexelSize", 1.0f / targetW, 1.0f / targetH);
    setUniform(program, "uSrcTexelSize", 1.0f / targetW, 1.0f / targetH);
    setUniform(program, "uTargetSize", targetW, targetH);

    // --- the layer's place in the frame ----------------------------------
    //
    // The layer's own texture is its full 0..1, and this maps it into the
    // target. A shader that works in the layer's space uses uLocalToUv; one
    // that works in the target's uses uLayerRect.
    const Mat3 localToUv = layerToTarget(layer.transformAt(settings.time), comp,
                                         destination.width, destination.height);
    setUniformMatrix3(program, "uLocalToUv", localToUv.m);

    // A layer drawn full-frame has its box covering the whole target.
    float minX = 0.0f, minY = 0.0f, maxX = 1.0f, maxY = 1.0f;
    layerRect(localToUv, minX, minY, maxX, maxY);
    setUniform(program, "uLayerRect", minX, minY, maxX, maxY);

    // --- the effect's own parameters -------------------------------------
    //
    // The spec names them without the `u` prefix and the shader declares them
    // with it, so binding is mechanical. A parameter the spec declares but the
    // shader does not read is not an error: the shader simply has fewer knobs.
    for (std::size_t i = 0; i < spec.params.size(); ++i) {
        const ParamSpec& p = spec.params[i];
        const double value = i < values.size() ? values[i] : p.default_;
        setUniform(program, "u" + p.name, value);
    }

    // --- what the compositor needs ---------------------------------------
    setUniform(program, "uAlpha", layer.blendStrength);
    setUniform(program, "uStrength", layer.blendStrength);
    setUniform(program, "uMode", static_cast<double>(layer.blend));
    setUniform(program, "uTime", settings.time);
    setUniform(program, "uPass", 0.0);
    setUniform(program, "uHeadroom", 1.0);

    drawFullscreenQuad();
    gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

// --- compositing ----------------------------------------------------------

bool Renderer::compositeLayer(TextureId layerTexture, const Layer& layer,
                              const Composition& comp, const RenderSettings& settings,
                              int targetWidth, int targetHeight, std::string& error)
{
    const unsigned int program = programFor("compositor_blend", error);
    if (program == 0) return false;

    gl.BindFramebuffer(GL_FRAMEBUFFER, _frame.framebuffer);
    gl.Viewport(0, 0, targetWidth, targetHeight);
    gl.Disable(GL_BLEND); // the shader does the blend itself
    gl.UseProgram(program);

    gl.ActiveTexture(GL_TEXTURE0);
    gl.BindTexture(GL_TEXTURE_2D, layerTexture);
    setUniform(program, "uTexture", 0.0);

    // The backdrop is the frame as it stands, read by gl_FragCoord, so it is
    // bound to the same texture the pass is drawing into. That is legal
    // because the shader reads it at a coordinate the fragment has not
    // written yet, and it is what avoids a copy of the whole frame per layer.
    gl.ActiveTexture(GL_TEXTURE0 + 1);
    gl.BindTexture(GL_TEXTURE_2D, _frame.texture);
    setUniform(program, "uBackdrop", 1.0);

    setUniform(program, "uTargetSize", static_cast<float>(targetWidth),
               static_cast<float>(targetHeight));
    setUniform(program, "uAlpha", layer.opacity.evaluate(settings.time));
    setUniform(program, "uStrength", layer.blendStrength);
    setUniform(program, "uMode", static_cast<double>(layer.blend));
    setUniform(program, "uTexelSize", 1.0f / targetWidth, 1.0f / targetHeight);

    drawFullscreenQuad();
    gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

// --- generated layers -----------------------------------------------------

bool Renderer::generateLayerFrame(const Layer& layer, int width, int height, Frame& out)
{
    if (width <= 0 || height <= 0) return false;

    out.width = width;
    out.height = height;
    out.pixels.assign(static_cast<std::size_t>(width) * height * 4, 0);

    auto put = [&](int x, int y, double r, double g, double b, double a) {
        if (x < 0 || y < 0 || x >= width || y >= height) return;
        const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 4;
        const double alpha = std::clamp(a, 0.0, 1.0);
        // Premultiplied, like every other source the compositor blends. A
        // straight-alpha layer composites with a halo at its edges.
        out.pixels[offset + 0] = static_cast<std::uint8_t>(std::clamp(r, 0.0, 1.0) * alpha * 255.0 + 0.5);
        out.pixels[offset + 1] = static_cast<std::uint8_t>(std::clamp(g, 0.0, 1.0) * alpha * 255.0 + 0.5);
        out.pixels[offset + 2] = static_cast<std::uint8_t>(std::clamp(b, 0.0, 1.0) * alpha * 255.0 + 0.5);
        out.pixels[offset + 3] = static_cast<std::uint8_t>(alpha * 255.0 + 0.5);
    };

    switch (layer.kind) {
    case LayerKind::Solid: {
        // A flat fill. The layer's box is the whole texture; the transform
        // moves and scales it at composite time.
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                put(x, y, layer.solidR, layer.solidG, layer.solidB, layer.solidA);
            }
        }
        return true;
    }

    case LayerKind::Shape: {
        // Rasterise the path's bounding box as a filled rectangle when there
        // are fewer than three points, and as an even-odd polygon otherwise.
        // Anti-aliased by sampling coverage at 2x2 per pixel: a hard-edged
        // polygon at 1x reads as a jagged mess at any angle but axis-aligned.
        if (layer.path.points.size() < 3) {
            if (layer.pathFilled) {
                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        put(x, y, layer.fillR, layer.fillG, layer.fillB, 1.0);
                    }
                }
                return true;
            }
            return false;
        }

        // The path is in normalised layer coordinates.
        std::vector<std::pair<double, double>> points;
        points.reserve(layer.path.points.size());
        for (const PathPoint& p : layer.path.points) {
            points.emplace_back(p.x, p.y);
        }

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                // Four samples inside the pixel, averaged.
                int hits = 0;
                for (int sy = 0; sy < 2; ++sy) {
                    for (int sx = 0; sx < 2; ++sx) {
                        const double px = (x + 0.25 + 0.5 * sx) / width;
                        const double py = (y + 0.25 + 0.5 * sy) / height;
                        if (pointInPolygon(px, py, points)) ++hits;
                    }
                }
                if (hits == 0) continue;
                const double coverage = hits / 4.0;
                if (layer.pathFilled) {
                    put(x, y, layer.fillR, layer.fillG, layer.fillB, coverage);
                } else if (layer.pathStroked) {
                    put(x, y, layer.strokeR, layer.strokeG, layer.strokeB, coverage);
                }
            }
        }
        return true;
    }

    case LayerKind::Text: {
        // Drawn with GDI into a 32-bit DIB and uploaded. Text rendering is
        // the one thing a hand-written rasteriser should not attempt: hinting,
        // kerning and font fallback are a project of their own, and the
        // platform already has all three.
        return rasteriseText(layer, width, height, out);
    }

    case LayerKind::Adjustment:
        // An adjustment layer is not drawn; its effect stack is applied to
        // what is already composited beneath it. Handled by the compositor.
        return false;

    case LayerKind::Null:
        // A transform-only parent. Nothing to draw.
        return false;

    case LayerKind::Audio:
        return false;

    case LayerKind::Media:
        return false;
    }
    return false;
}

bool Renderer::pointInPolygon(double x, double y,
                              const std::vector<std::pair<double, double>>& points)
{
    // The standard even-odd crossing test. Counts how many edges a ray to the
    // right of the point crosses; odd means inside.
    bool inside = false;
    const std::size_t count = points.size();
    for (std::size_t i = 0, j = count - 1; i < count; j = i++) {
        const double xi = points[i].first;
        const double yi = points[i].second;
        const double xj = points[j].first;
        const double yj = points[j].second;

        // The half-open comparison on y is what keeps a vertex exactly on the
        // ray from being counted twice, which would flip the result for a
        // whole scanline.
        if ((yi > y) != (yj > y)) {
            const double crossing = (xj - xi) * (y - yi) / (yj - yi) + xi;
            if (x < crossing) inside = !inside;
        }
    }
    return inside;
}

bool Renderer::rasteriseText(const Layer& layer, int width, int height, Frame& out)
{
    if (layer.text.empty()) return false;

#ifdef _WIN32
    // A 32-bit top-down DIB, which is the layout GDI draws into without a
    // conversion and the layout the frame already uses.
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height; // negative: top-down
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC screen = GetDC(nullptr);
    if (screen == nullptr) return false;

    HDC memory = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screen);

    if (memory == nullptr || bitmap == nullptr || bits == nullptr) {
        if (bitmap) DeleteObject(bitmap);
        if (memory) DeleteDC(memory);
        return false;
    }

    HGDIOBJ previous = SelectObject(memory, bitmap);

    // The DIB arrives zeroed, which is transparent. GDI draws with the alpha
    // channel ignored, so the text is drawn opaque and the alpha is rebuilt
    // below from the luminance of what was drawn.
    std::memset(bits, 0, static_cast<std::size_t>(width) * height * 4);

    SetBkMode(memory, TRANSPARENT);
    SetTextColor(memory, RGB(255, 255, 255));

    // Scale the requested size to the render size. The composition is
    // 1920x1080 and the texture may be a preview at a fraction of that, so a
    // font size in composition units would come out wrong.
    const int pointSize = static_cast<int>(
        std::max(1.0, layer.textSize * height / 1080.0));

    HFONT font = CreateFontW(
        -pointSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    HGDIOBJ previousFont = font ? SelectObject(memory, font) : nullptr;

    const std::wstring wide = toWideForText(layer.text);
    RECT box{0, 0, width, height};
    DrawTextW(memory, wide.c_str(), static_cast<int>(wide.size()),
              &box, DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_NOPREFIX);

    GdiFlush();

    // Rebuild alpha from luminance. GDI wrote white text on black; the alpha
    // is how much white there is, and the colour is the layer's fill.
    auto* pixels = static_cast<std::uint8_t*>(bits);
    for (std::size_t i = 0; i < static_cast<std::size_t>(width) * height; ++i) {
        const std::uint8_t luminance = pixels[i * 4 + 0];
        if (luminance == 0) continue;
        const double alpha = luminance / 255.0;
        out.pixels[i * 4 + 0] = static_cast<std::uint8_t>(layer.fillR * alpha * 255.0 + 0.5);
        out.pixels[i * 4 + 1] = static_cast<std::uint8_t>(layer.fillG * alpha * 255.0 + 0.5);
        out.pixels[i * 4 + 2] = static_cast<std::uint8_t>(layer.fillB * alpha * 255.0 + 0.5);
        out.pixels[i * 4 + 3] = static_cast<std::uint8_t>(alpha * 255.0 + 0.5);
    }

    if (previousFont) SelectObject(memory, previousFont);
    if (font) DeleteObject(font);
    SelectObject(memory, previous);
    DeleteObject(bitmap);
    DeleteDC(memory);
    return true;
#else
    (void)layer;
    (void)width;
    (void)height;
    (void)out;
    return false;
#endif
}

// --- the frame ------------------------------------------------------------

bool Renderer::renderFrame(const Composition& comp, const RenderSettings& settings,
                           const std::vector<Frame>& frames, std::string& error)
{
    if (!_ready) {
        error = "the renderer is not initialised";
        return false;
    }
    const int width = std::max(settings.width, 1);
    const int height = std::max(settings.height, 1);

    // The frame target, resized when the composition changes.
    if (_frame.width != width || _frame.height != height) {
        if (_frame.framebuffer) {
            gl.DeleteFramebuffers(1, &_frame.framebuffer);
            gl.DeleteTextures(1, &_frame.texture);
        }
        _frame = acquireTarget(width, height);
        if (_frame.framebuffer == 0) {
            error = "the renderer could not create a " + std::to_string(width) + "x"
                  + std::to_string(height) + " render target";
            return false;
        }
    }

    beginFrame(width, height);

    // The background, then every live layer in order, bottom first.
    gl.ClearColor(static_cast<GLfloat>(comp.backgroundR),
                  static_cast<GLfloat>(comp.backgroundG),
                  static_cast<GLfloat>(comp.backgroundB),
                  static_cast<GLfloat>(comp.backgroundA));
    gl.Clear(GL_COLOR_BUFFER_BIT);

    for (std::size_t index = 0; index < comp.layers.size(); ++index) {
        const Layer& layer = comp.layers[index];
        if (!layer.activeAt(settings.time)) continue;
        if (layer.kind == LayerKind::Audio || layer.kind == LayerKind::Null) continue;
        if (layer.kind == LayerKind::Adjustment) continue; // handled by the compositor pass

        // The layer's own picture.
        //
        // A media layer's pixels come from the decoder. Everything else is
        // generated here, because a solid has no file and a shape has no
        // pixels until something rasterises it.
        TextureId source = 0;
        if (layer.kind == LayerKind::Media) {
            if (index < frames.size() && frames[index].valid()) {
                source = upload(frames[index]);
            }
            if (source == 0) continue; // a missing asset draws nothing
        } else {
            Frame generated;
            if (!generateLayerFrame(layer, width, height, generated)) continue;
            source = upload(generated);
            if (source == 0) continue;
        }

        // Run the effect stack over it, ping-ponging between two targets: each
        // pass reads the previous pass's output and writes the other one.
        // Reading and writing the same texture is undefined, so two targets
        // are the minimum, and alternating a flag is clearer than comparing
        // texture handles to work out which is which.
        TextureId current = source;
        bool useScratchA = true;

        for (const Effect& effect : layer.effects) {
            if (!effect.enabled) continue;
            const EffectSpec* spec = EffectCatalog::instance().find(effect.specId);
            if (spec == nullptr) continue;

            if (_scratchA.width != width || _scratchA.height != height) {
                _scratchA = acquireTarget(width, height);
            }
            if (_scratchB.width != width || _scratchB.height != height) {
                _scratchB = acquireTarget(width, height);
            }
            if (_scratchA.framebuffer == 0 || _scratchB.framebuffer == 0) {
                error = "the renderer could not create its scratch targets";
                return false;
            }

            // Evaluate the parameters once, at this frame's time, rather than
            // per pass: a parameter read by two passes must not disagree with
            // itself within one frame.
            std::vector<double> values;
            values.reserve(spec->params.size());
            for (const ParamSpec& p : spec->params) {
                const auto it = effect.parameters.find(p.name);
                values.push_back(it == effect.parameters.end()
                                     ? p.default_
                                     : it->second.evaluate(settings.time));
            }

            const Target& destination = useScratchA ? _scratchA : _scratchB;
            if (!applyEffect(effect, *spec, current, destination, layer, comp, settings,
                             values, _frame.texture,
                             spec->needsPreviousFrame ? _previous.texture : 0,
                             error)) {
                return false;
            }
            current = destination.texture;
            useScratchA = !useScratchA;
        }

        if (!compositeLayer(current, layer, comp, settings, width, height, error)) {
            return false;
        }
    }

    gl.Flush();
    return true;
}

bool Renderer::readback(Frame& out, std::string& error)
{
    if (!_ready || _frame.framebuffer == 0) {
        error = "there is no rendered frame to read";
        return false;
    }

    gl.BindFramebuffer(GL_FRAMEBUFFER, _frame.framebuffer);
    gl.PixelStorei(0x0CF5 /* GL_UNPACK_ALIGNMENT */, 1);

    out.width = _frame.width;
    out.height = _frame.height;
    out.pixels.resize(static_cast<std::size_t>(_frame.width) * _frame.height * 4);

    gl.ReadPixels(0, 0, _frame.width, _frame.height, GL_RGBA, GL_UNSIGNED_BYTE,
                  out.pixels.data());
    gl.BindFramebuffer(GL_FRAMEBUFFER, 0);

    // GL reads bottom-up and every consumer wants top-down.
    const std::size_t rowBytes = static_cast<std::size_t>(_frame.width) * 4;
    std::vector<GLubyte> flipped(out.pixels.size());
    for (int y = 0; y < _frame.height; ++y) {
        std::copy_n(out.pixels.data() + static_cast<std::ptrdiff_t>(y) * rowBytes,
                    rowBytes,
                    flipped.data() + static_cast<std::ptrdiff_t>(_frame.height - 1 - y) * rowBytes);
    }
    out.pixels.swap(flipped);
    return true;
}

} // namespace keyflow
