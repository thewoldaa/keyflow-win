// ---------------------------------------------------------------------------
// Media: probing, decoding and encoding, via ffmpeg.
//
// The app shells out to ffmpeg.exe rather than linking libav. That is a
// deliberate trade:
//
//   - The build has no third-party link step. libav needs its own configure,
//     its own runtime, and its DLLs beside the executable; a fresh clone
//     builds with a compiler and nothing else.
//   - A decoder crash cannot take the editor with it. ffmpeg is a separate
//     process, and a malformed file kills that process, not the app.
//   - The user almost certainly already has ffmpeg, and if not, one path in
//     settings fixes it.
//
// The cost is a process spawn per decode, which matters for scrubbing. The
// frame cache below is what makes that acceptable: a decode is done once per
// frame and then held.
//
// Everything here is platform-neutral — it is process spawning and file
// parsing, not Win32 — so it builds and is tested on any platform.
// ---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace keyflow {

/// What ffprobe found in a file.
struct MediaInfo
{
    bool ok = false;

    /// Why the probe failed, when it did.
    std::string error;

    int width = 0;
    int height = 0;

    /// Seconds. Zero for a still image.
    double duration = 0.0;

    /// The container's frame rate, as a rational. Zero over one when unknown.
    int fpsNumerator = 0;
    int fpsDenominator = 1;

    /// True when the file has an audio stream.
    bool hasAudio = false;

    /// True when the file is a still image rather than a video.
    bool still = false;

    /// The video codec's short name, for the media panel.
    std::string videoCodec;

    /// The number of frames, when the container states it.
    int frameCount = 0;
};

/// A decoded frame, in RGBA8, top row first.
///
/// Top row first rather than GL's bottom-up: every consumer either uploads it
/// to a texture (where the flip is one line) or writes it to a file (where
/// top-down is correct), and the format a decoder naturally produces is not
/// consistent between codecs.
struct Frame
{
    int width = 0;
    int height = 0;

    /// width * height * 4 bytes, RGBA, 8 bits per channel.
    std::vector<std::uint8_t> pixels;

    /// Seconds from the start of the source.
    double time = 0.0;

    bool valid() const
    {
        return width > 0 && height > 0
            && pixels.size() == static_cast<std::size_t>(width) * height * 4;
    }
};

/// Where ffmpeg lives.
///
/// Resolved once at startup: an explicit path from settings, then beside the
/// executable, then on PATH. A missing ffmpeg is reported when the user first
/// tries to import, not at launch — the editor's model and project files work
/// without it, and refusing to start would be wrong.
struct FFmpegPaths
{
    std::string ffmpeg;
    std::string ffprobe;

    bool valid() const { return !ffmpeg.empty() && !ffprobe.empty(); }
};

/// Find ffmpeg and ffprobe.
///
/// `explicitDirectory` is checked first when non-empty. Then the directory
/// holding the running executable. Then PATH.
FFmpegPaths locateFFmpeg(const std::string& explicitDirectory = {});

/// Probe a file. Never throws; a failure is reported in the result.
MediaInfo probeMedia(const FFmpegPaths& tools, const std::string& path);

/// Decode one frame at `time` seconds into `out`.
///
/// Returns false and fills `out.error` on failure.
bool decodeFrameAt(const FFmpegPaths& tools, const std::string& path,
                   double time, int targetWidth, int targetHeight,
                   Frame& out, std::string& error);

/// Decode a frame to a temporary file rather than to memory.
///
/// Used by the thumbnail pipeline, where the result is written straight to
/// disk and never touched again.
bool extractFrameToPng(const FFmpegPaths& tools, const std::string& path,
                       double time, int width, int height,
                       const std::string& outputPath, std::string& error);

/// Settings for a render.
struct EncodeSettings
{
    std::string outputPath;

    /// Container and codec. "libx264" is the default because it is present in
    /// every ffmpeg build; the hardware encoders are offered when the probe
    /// below says the machine has one.
    std::string videoCodec = "libx264";
    std::string audioCodec = "aac";

    /// Quality. For x264 this is CRF, where lower is better; for the hardware
    /// encoders it is a bitrate in kbps.
    int quality = 18;
    bool useBitrate = false;
    int bitrateKbps = 12000;

    /// The output frame rate.
    int fpsNumerator = 30;
    int fpsDenominator = 1;

    /// Pixel format. yuv420p is the only one every player accepts.
    std::string pixelFormat = "yuv420p";

    /// True to encode at a size other than the composition's.
    bool resize = false;
    int width = 0;
    int height = 0;
};

/// Which hardware encoders this machine's ffmpeg build offers.
///
/// Probed once and cached. A hardware encoder that the build lists but the
/// driver refuses still fails at encode time, so this is a hint for the UI
/// rather than a promise.
struct EncoderSupport
{
    bool nvenc = false;
    bool qsv = false;
    bool amf = false;
};

EncoderSupport probeEncoders(const FFmpegPaths& tools);

/// Build the ffmpeg argument list for an encode.
///
/// Separated from the run so the tests can check the arguments without an
/// ffmpeg present. The arguments are the part that is easy to get wrong and
/// the part that is worth pinning.
std::vector<std::string> buildEncodeArguments(const EncodeSettings& settings,
                                              const std::string& inputPattern);

/// Build the argument list that decodes a source into a raw RGBA stream on
/// stdout, one frame at a time.
std::vector<std::string> buildDecodeArguments(const std::string& path,
                                              double time, int width, int height);

/// Run a process and capture its stdout.
///
/// Returns the exit code, or -1 when the process could not be started.
int runProcessCapture(const std::string& executable,
                      const std::vector<std::string>& arguments,
                      std::string& stdoutText,
                      std::string& error);

/// Run a process and let it write its own stdout, streaming each line to
/// `onLine`. Used by the renderer, which wants ffmpeg's progress lines.
int runProcessStreaming(const std::string& executable,
                        const std::vector<std::string>& arguments,
                        const std::function<void(const std::string&)>& onLine,
                        std::string& error);

} // namespace keyflow
