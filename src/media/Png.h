// ---------------------------------------------------------------------------
// PNG encoding and decoding.
//
// The preview sends frames to the page as data URLs and the renderer writes a
// PNG per frame for ffmpeg to encode. Both need PNG, and both need it without
// a third-party library: zlib is the only dependency and it is in every
// Windows SDK.
//
// Written out rather than pulled in because the alternative is a dependency
// that has to be vendored, licensed and built for every architecture, in
// exchange for about two hundred lines.
// ---------------------------------------------------------------------------

#pragma once

#include <string>

#include "media/Media.h"

namespace keyflow {

/// Encode a frame as a PNG file.
///
/// The frame is RGBA, top row first. Returns false and fills `error` on
/// failure.
bool writePng(const Frame& frame, const std::string& path, std::string& error);

/// Encode a frame as a PNG and return it base64-encoded in a data URL.
///
/// The page draws the result onto a canvas. A data URL rather than a file
/// because the page has no file system access, and rather than a raw pixel
/// buffer because getting a hundred megabytes of pixels through
/// ExecuteScript would be slower than the encode.
std::string encodePngDataUrl(const Frame& frame);

/// Encode a byte buffer as base64.
std::string base64Encode(const std::string& bytes);

/// Encode a frame as a PNG into a byte buffer. Used by both entry points.
bool encodePng(const Frame& frame, std::string& out, std::string& error);

} // namespace keyflow
