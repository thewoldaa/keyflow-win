// ---------------------------------------------------------------------------
// Desktop GLSL compatibility.
//
// The shaders are GLSL ES 1.00, written for Android. Desktop OpenGL is not
// the same language, and the differences are small enough to hide until
// runtime, which is the worst way to find them:
//
//   * GLSL ES requires a precision statement; desktop GLSL 1.10 does not
//     accept one at all, and a fragment shader that opens with
//     `precision mediump float;` is a syntax error on a desktop driver.
//   * GLSL ES spells the texture sampler read `texture2D`; desktop GLSL 3.30
//     and later renamed it `texture`. Compatibility profiles still take the
//     old name, which is why only the precision needs handling here — but the
//     rename is one driver away, so it is handled too.
//   * `#extension GL_OES_EGL_image_external` is an Android camera extension
//     with no desktop meaning. The driver is not required to accept a
//     directive it does not know, and Mesa rejects unknown extensions
//     outright.
//
// Rewriting the source is the alternative to maintaining two copies of sixty
// shaders. The transforms are mechanical and applied once per program at
// compile time, so the cost is a string pass at startup and nothing per frame.
// ---------------------------------------------------------------------------

#pragma once

#include <string>

namespace keyflow {

/// Rewrite GLSL ES 1.00 source for a desktop OpenGL context.
///
/// Strips precision statements, resolves the GL_FRAGMENT_PRECISION_HIGH
/// conditional to its high-precision branch, drops Android-only extensions,
/// and rewrites the sampler read to its modern name when the context is new
/// enough to need it. `modern` selects the sampler spelling: pass false for a
/// compatibility profile, which still accepts `texture2D`.
std::string toDesktopGlsl(const std::string& source, bool modern);

/// True when the source contains an Android-only extension that has no
/// desktop equivalent. Used by the diagnostics to say why a shader was
/// skipped rather than leaving it as a silent no-op.
bool usesAndroidOnlyExtension(const std::string& source);

} // namespace keyflow
