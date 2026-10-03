# ---------------------------------------------------------------------------
# Regenerate src/gl/ShaderTable.cpp from src/gl/shaders/*.glsl.
#
# The shaders are compiled into the executable rather than read from disk, so
# there is no directory of .glsl files to go missing, be edited, or belong to a
# different version of the app. Every one of those failure modes shows up as a
# black frame with no explanation.
#
# Run this after adding or editing a shader. The generated file says so at the
# top; a hand edit there survives until the next build and then disappears,
# which is the worst of both.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts/embed-shaders.ps1
#   powershell -ExecutionPolicy Bypass -File scripts/embed-shaders.ps1 -Check
#
# -Check regenerates into memory and fails when the file on disk differs. CI
# runs it that way, so a shader edited without re-running this script is a
# build failure rather than a shader that silently does not ship.
# ---------------------------------------------------------------------------

[CmdletBinding()]
param(
    # Fail instead of writing when the generated file would change.
    [switch] $Check
)

$ErrorActionPreference = 'Stop'

$repoRoot  = Split-Path -Parent $PSScriptRoot
$shaderDir = Join-Path $repoRoot 'src\gl\shaders'
$outFile   = Join-Path $repoRoot 'src\gl\ShaderTable.cpp'

if (-not (Test-Path $shaderDir)) {
    throw "no shader directory at $shaderDir"
}

$shaders = Get-ChildItem -LiteralPath $shaderDir -Filter '*.glsl' -File | Sort-Object Name
if ($shaders.Count -eq 0) {
    throw "no .glsl files in $shaderDir"
}

$sb = [System.Text.StringBuilder]::new()

[void]$sb.AppendLine('// ---------------------------------------------------------------------------')
[void]$sb.AppendLine('// The shader table.')
[void]$sb.AppendLine('//')
[void]$sb.AppendLine('// GENERATED from the recovered shader set. Do not edit by hand: edit the')
[void]$sb.AppendLine('// .glsl files under src/gl/shaders and re-run scripts/embed-shaders.ps1, which')
[void]$sb.AppendLine('// regenerates this file. A hand edit here survives until the next build and')
[void]$sb.AppendLine('// then disappears, which is the worst of both.')
[void]$sb.AppendLine('//')
[void]$sb.AppendLine('// The shaders are GLSL ES 1.00, recovered from the Android build and adapted')
[void]$sb.AppendLine('// for a desktop context by gl/GlslCompat.cpp at compile time. They are')
[void]$sb.AppendLine('// compiled into the executable rather than read from disk so the app cannot')
[void]$sb.AppendLine('// run against a shader set that does not match its own catalogue.')
[void]$sb.AppendLine('//')
[void]$sb.AppendLine('// Long units are emitted as several adjacent raw string literals. MSVC caps a')
[void]$sb.AppendLine('// single literal at 16380 bytes; adjacent literals are concatenated at compile')
[void]$sb.AppendLine('// time and produce the same string.')
[void]$sb.AppendLine('// ---------------------------------------------------------------------------')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('#include "gl/ShaderTable.h"')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('#include <array>')
[void]$sb.AppendLine('#include <string_view>')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('namespace keyflow {')
[void]$sb.AppendLine('namespace {')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('struct Entry')
[void]$sb.AppendLine('{')
[void]$sb.AppendLine('    std::string_view stem;')
[void]$sb.AppendLine('    std::string_view source;')
[void]$sb.AppendLine('};')
[void]$sb.AppendLine('')
[void]$sb.AppendLine("constexpr std::array<Entry, $($shaders.Count)> kShaders = { {")

foreach ($shader in $shaders) {
    $stem = [System.IO.Path]::GetFileNameWithoutExtension($shader.Name)
    $source = [System.IO.File]::ReadAllText($shader.FullName)

    # Normalise to \n: a file saved with CRLF would otherwise embed carriage
    # returns, which some drivers reject inside a shader.
    $source = $source -replace "`r`n", "`n"
    if (-not $source.EndsWith("`n")) { $source += "`n" }

    # A raw-string delimiter that does not occur in the source.
    $delimiter = 'KF'
    while ($source.Contains($delimiter)) { $delimiter += '_' }

    # MSVC caps a single string literal at 16380 bytes and silently truncates
    # anything longer, which produces a shader that compiles to something other
    # than what is on disk. Split on line boundaries and emit adjacent
    # literals: the compiler concatenates them, and splitting at a newline
    # cannot cut a token in half.
    $limit = 7000
    $chunks = @()
    $current = [System.Text.StringBuilder]::new()
    foreach ($line in ($source -split "`n")) {
        $piece = $line + "`n"
        if (($current.Length + $piece.Length) -gt $limit -and $current.Length -gt 0) {
            $chunks += $current.ToString()
            $current = [System.Text.StringBuilder]::new()
        }
        [void]$current.Append($piece)
    }
    if ($current.Length -gt 0) { $chunks += $current.ToString() }

    [void]$sb.AppendLine("    { `"$stem`",")
    foreach ($chunk in $chunks) {
        [void]$sb.Append("R`"$delimiter(")
        [void]$sb.Append($chunk)
        [void]$sb.AppendLine(")$delimiter`"")
    }
    [void]$sb.AppendLine("    },")
}

[void]$sb.AppendLine('} };')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('// The vertex shader every full-screen effect pass uses. It is also in the')
[void]$sb.AppendLine('// table above; this reference is what the renderer actually binds.')
[void]$sb.AppendLine('constexpr std::string_view kPassthroughVertex = R"KF(')
[void]$sb.AppendLine('attribute vec2 aPos;')
[void]$sb.AppendLine('attribute vec2 aTex;')
[void]$sb.AppendLine('varying vec2 vTex;')
[void]$sb.AppendLine('void main() {')
[void]$sb.AppendLine('    gl_Position = vec4(aPos * 2.0 - 1.0, 0.0, 1.0);')
[void]$sb.AppendLine('    vTex = aTex;')
[void]$sb.AppendLine('}')
[void]$sb.AppendLine(')KF";')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('} // namespace')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('std::string_view shaderSourceView(const std::string& stem)')
[void]$sb.AppendLine('{')
[void]$sb.AppendLine('    for (const Entry& entry : kShaders) {')
[void]$sb.AppendLine('        if (entry.stem == stem) return entry.source;')
[void]$sb.AppendLine('    }')
[void]$sb.AppendLine('    return {};')
[void]$sb.AppendLine('}')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('std::vector<std::string> shaderStems()')
[void]$sb.AppendLine('{')
[void]$sb.AppendLine('    std::vector<std::string> stems;')
[void]$sb.AppendLine('    stems.reserve(kShaders.size());')
[void]$sb.AppendLine('    for (const Entry& entry : kShaders) {')
[void]$sb.AppendLine('        stems.emplace_back(entry.stem);')
[void]$sb.AppendLine('    }')
[void]$sb.AppendLine('    return stems;')
[void]$sb.AppendLine('}')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('std::string shaderSource(const std::string& stem)')
[void]$sb.AppendLine('{')
[void]$sb.AppendLine('    return std::string(shaderSourceView(stem));')
[void]$sb.AppendLine('}')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('std::string passthroughVertexShader()')
[void]$sb.AppendLine('{')
[void]$sb.AppendLine('    return std::string(kPassthroughVertex);')
[void]$sb.AppendLine('}')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('} // namespace keyflow')

$generated = $sb.ToString()

if ($Check) {
    if (-not (Test-Path $outFile)) {
        Write-Error "ShaderTable.cpp does not exist. Run scripts/embed-shaders.ps1."
        exit 1
    }
    $current = [System.IO.File]::ReadAllText($outFile)
    if ($current -ne $generated) {
        Write-Error @"
src/gl/ShaderTable.cpp is out of date.

A .glsl file was added or edited without regenerating the table, so the
build would ship the previous shader. Run:

    powershell -ExecutionPolicy Bypass -File scripts/embed-shaders.ps1
"@
        exit 1
    }
    Write-Host "ShaderTable.cpp is up to date ($($shaders.Count) shaders)."
    exit 0
}

# Write only when it changed, so a run that changes nothing does not touch the
# file and force a rebuild of everything that includes it.
if ((Test-Path $outFile) -and ([System.IO.File]::ReadAllText($outFile) -eq $generated)) {
    Write-Host "ShaderTable.cpp is already up to date ($($shaders.Count) shaders)."
    exit 0
}

[System.IO.File]::WriteAllText($outFile, $generated, [System.Text.UTF8Encoding]::new($false))
Write-Host "Wrote src/gl/ShaderTable.cpp with $($shaders.Count) shaders."
