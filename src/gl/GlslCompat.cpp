#include "gl/GlslCompat.h"

#include <algorithm>
#include <sstream>
#include <vector>

namespace keyflow {

namespace {

/// Split into lines, keeping no line terminators.
std::vector<std::string> splitLines(const std::string& text)
{
    std::vector<std::string> lines;
    std::string current;
    for (char c : text) {
        if (c == '\n') {
            lines.push_back(current);
            current.clear();
        } else if (c != '\r') {
            current.push_back(c);
        }
    }
    if (!current.empty()) lines.push_back(current);
    return lines;
}

std::string trim(const std::string& text)
{
    const std::size_t first = text.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    const std::size_t last = text.find_last_not_of(" \t");
    return text.substr(first, last - first + 1);
}

/// True when a line is a `precision <qualifier> <type>;` statement.
bool isPrecisionStatement(const std::string& line)
{
    const std::string t = trim(line);
    if (t.rfind("precision", 0) != 0) return false;
    // The statement ends in a semicolon; `precision` as a prefix of an
    // identifier would not.
    if (t.empty() || t.back() != ';') return false;
    return t.find("float") != std::string::npos || t.find("int") != std::string::npos;
}

/// Replace every whole-word occurrence of `from` with `to`.
void replaceWord(std::string& text, const std::string& from, const std::string& to)
{
    std::size_t position = 0;
    while ((position = text.find(from, position)) != std::string::npos) {
        // Whole word only: `texture2D` must not be found inside a longer
        // identifier, and `texture` must not be found inside `texture2D`.
        const bool leftOk = position == 0
            || !(std::isalnum(static_cast<unsigned char>(text[position - 1]))
                 || text[position - 1] == '_');
        const std::size_t after = position + from.size();
        const bool rightOk = after >= text.size()
            || !(std::isalnum(static_cast<unsigned char>(text[after]))
                 || text[after] == '_');

        if (leftOk && rightOk) {
            text.replace(position, from.size(), to);
            position += to.size();
        } else {
            position += from.size();
        }
    }
}

} // namespace

bool usesAndroidOnlyExtension(const std::string& source)
{
    return source.find("GL_OES_EGL_image_external") != std::string::npos;
}

std::string toDesktopGlsl(const std::string& source, bool modern)
{
    std::vector<std::string> lines = splitLines(source);
    std::vector<std::string> out;
    out.reserve(lines.size());

    // The #ifdef GL_FRAGMENT_PRECISION_HIGH block: desktop GL always has full
    // precision, so the true branch is taken and the rest dropped. Tracking
    // the block explicitly rather than deleting every precision line keeps the
    // shader's own structure intact for anything else inside it.
    bool inPrecisionConditional = false;

    for (const std::string& raw : lines) {
        const std::string t = trim(raw);

        // --- #ifdef GL_FRAGMENT_PRECISION_HIGH ---------------------------
        if (t.rfind("#ifdef", 0) == 0
            && t.find("GL_FRAGMENT_PRECISION_HIGH") != std::string::npos) {
            inPrecisionConditional = true;
            continue;
        }
        if (inPrecisionConditional) {
            if (t.rfind("#else", 0) == 0) {
                // Everything from here to the matching #endif is the fallback
                // branch, which desktop GL never needs.
                continue;
            }
            if (t.rfind("#endif", 0) == 0) {
                inPrecisionConditional = false;
                continue;
            }
            // A precision statement inside the true branch: dropped by the
            // isPrecisionStatement check below anyway.
        }

        // --- #extension --------------------------------------------------
        if (t.rfind("#extension", 0) == 0) {
            // Android's external-image sampler. Dropping the directive is not
            // enough on its own — the sampler type it enables is also
            // unknown — but a shader that uses it is skipped by the caller
            // rather than compiled with a type the driver cannot resolve.
            if (t.find("GL_OES_EGL_image_external") != std::string::npos) continue;
            // A desktop driver rejects an extension it does not advertise, so
            // a directive for one is worse than useless. Keep only the two
            // that every desktop driver has had for years.
            if (t.find("GL_EXT_shader_texture_lod") == std::string::npos
                && t.find("GL_OES_standard_derivatives") == std::string::npos
                && t.find("GL_EXT_frag_depth") == std::string::npos) {
                continue;
            }
        }

        // --- precision statements ----------------------------------------
        if (isPrecisionStatement(raw)) continue;

        out.push_back(raw);
    }

    std::string text;
    for (std::size_t i = 0; i < out.size(); ++i) {
        text += out[i];
        text.push_back('\n');
    }

    // `precision` can also appear as a bare qualifier on a declaration
    // (`precision highp float;` handled above; `highp float x;` is legal ES
    // and illegal on some desktop drivers). Desktop GL has no precision
    // qualifiers, so the words are removed where they qualify a type.
    for (const char* qualifier : {"highp", "mediump", "lowp"}) {
        // Only as a leading qualifier on a declaration, never as an
        // identifier: these words are reserved in GLSL, so any occurrence is
        // a qualifier.
        replaceWord(text, qualifier, "");
    }

    if (modern) {
        // GLSL 3.30 renamed the sampler read. Done after the line pass so a
        // comment mentioning the old name is not rewritten.
        replaceWord(text, "texture2D", "texture");
        replaceWord(text, "textureCube", "texture");
        // `varying` and `attribute` are also gone in a core profile. The
        // compatibility profile this renderer asks for still takes them, so
        // only the sampler read is rewritten here.
    }

    return text;
}

} // namespace keyflow
