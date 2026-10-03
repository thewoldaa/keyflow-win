// ---------------------------------------------------------------------------
// The shader table.
//
// The GLSL is compiled into the executable. The alternative — shipping .glsl
// files next to keyflow.exe — is a directory that can go missing, be edited,
// or belong to a different version of the app, and every one of those shows up
// as a black frame with no explanation.
//
// ShaderTable.cpp is generated; this header is not.
// ---------------------------------------------------------------------------

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace keyflow {

/// The GLSL source for a shader stem, or an empty view when absent.
std::string_view shaderSourceView(const std::string& stem);

/// The GLSL source for a shader stem as an owned string.
std::string shaderSource(const std::string& stem);

/// Every stem in the table, sorted. Used by the tests that compile the whole
/// set and by the diagnostic that reports what is missing.
std::vector<std::string> shaderStems();

/// The vertex shader every full-screen effect pass uses.
std::string passthroughVertexShader();

} // namespace keyflow
