# Context

Where this project is, what has been verified, and the traps that cost time
the first time round. Written to be read cold by someone — or something —
picking this up with no memory of how it got here.

Last updated at commit `1c57516`, on `dev`.

---

## 1. What this is

A native Windows port of the Android app **Keyflow** (`com.angkin.keyflow`),
a GL-shader video editor. The APK is at:

```
C:\1234\undow my app to win app\Keyflow.apk
```

The port is not a reimplementation. The Android build compiled its GLSL into
`classes.dex` as string constants, and a string constant is data — R8
obfuscates class and method names but cannot reach into a literal. So the
shader source survives decompilation intact, comments included. 63 units came
across that way, along with the effect catalogue, the document model, the
project format and the expression language.

The shell around them is new: Win32 window, WebView2, an HTML interface, an
OpenGL compositor, and ffmpeg as a subprocess.

**Repository:** https://github.com/thewoldaa/keyflow-win (public)
**Default branch:** `main`. **Work happens on:** `dev`.
**Local path:** `C:\1234\keyflow-win`

---

## 2. Current state

Everything below was verified at the commit above, not assumed.

| | |
|---|---|
| Unit tests | **123 cases, 0 failed** (`ctest`, no GPU, any platform) |
| Shader self-test | **63 of 63 compile**, exit 0 (`keyflow.exe --self-test`) |
| Blend modes | **Normal, Multiply and Screen correct** (`--self-test`) |
| Effects in the catalogue | **49**, across 10 categories |
| Shaders | **63** `.glsl` files |
| Files tracked | 128 |
| Commits | 8 |
| Fresh clone | builds, tests, self-tests and launches — verified |
| Wave harness | start → build in worktree → test → finish — verified |

The app launches with the full interface: effect browser grouped by category,
viewer, transport, timeline, inspector, and a layout report that measures clean
(no collapsed band, nothing past the bottom edge).

### Source sizes

```
src/core       3438 lines   model, format, catalogue, expressions, shader table
src/gl         5862 lines   the compositor, the GLSL compatibility pass
src/app        2652 lines   the window, the message bridge
src/media      1089 lines   ffmpeg subprocess, PNG encoder
src/ui/assets  2455 lines   ui.html — the whole interface, one file
```

---

## 3. The effects that came across

49 in the catalogue. The categories are the browser's grouping, in
`EffectCatalog.cpp`:

- **Colour (9)** — Brightness & Contrast, Exposure & Gamma, Hue & Saturation,
  HSL Adjust, Levels, Colour LUT, Invert, Fill Colour, Opacity
- **Keying (1)** — Chroma Key
- **Blur (5)** — Box Blur, Bokeh Blur, Blur (separable, two passes), Masked
  Blur, Matte Choke
- **Glow (3)** — Glow, Highlight Recovery, Highlight Suppress
- **Stylise (6)** — Halftone (CMYK), Pixelate, Scanlines, Fractal Noise, ASCII,
  Sharpen
- **Generate (8)** — Audio Spectrum, 4-Colour Gradient, Gradient, Circle,
  Rectangle, Ellipse, Radial Repeat, Light Flare
- **Motion (5)** — Wiggle, Ripple, Swirl, Transform, Directional Motion Blur
- **Transition (3)** — Rectangular Wipe, Linear Wipe, Radial Wipe
- **Distort (6)** — Chromatic Aberration, Grain, Outline, Edge Detect, Stroke,
  Drop Shadow
- **Composite (3)** — Blend With Backdrop, Adjustment Layer, Apply Mask

The compositor shader carries 21 blend modes (Porter-Duff plus the Photoshop
set) and is driven by a `uMode` uniform.

---

## 4. Build, run, test

```powershell
cd C:\1234\keyflow-win

scripts\fetch-deps.ps1              # WebView2 SDK, once per checkout/worktree
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Executable lands at `build\dev\bin\keyflow.exe`.

**Diagnostics the app carries:**

```powershell
keyflow.exe --self-test        # compiles all 63 shaders, renders a frame, exit code
keyflow.exe --layout-report    # page measures its own bands, writes layout_report.json
```

`--self-test` writes `self_test.log` beside itself and returns 0 on success.
It answers two questions the unit tests cannot, because they have no GL
context: do the shaders compile, and do the blend modes actually blend. The
second one reads pixels — a uniform that never reached the shader and a blend
that never ran look identical from the outside.

**ffmpeg** is a runtime dependency, not a link-time one. The app looks for it
beside `keyflow.exe`, then on `PATH`. Without it the editor still runs; only
import and export need it.

---

## 5. The traps

This is the section worth reading. Each of these cost real time and would cost
it again.

### The message accessor, not the JSON parse

The page posts a JSON **string**. `get_WebMessageAsJson` wraps it in another
layer of quoting, so the bridge receives a JSON string where it expects an
object and rejects every message — including `ready`. The window stays empty
with no error anywhere.

Use `TryGetWebMessageAsString`. See `src/app/main.cpp`, the
`WebMessageReceived` handler.

### The host-to-page hook has to exist

The host cannot use `postMessage` — that is the page-to-host direction. It
calls `window.__keyflowReceive(...)` by name. If the page does not define it,
every message the host sends is dropped **silently**. The page must define it
unconditionally and before it sends `ready`.

### GLSL ES is not desktop GLSL

Three differences, all of which surface as a compile error at runtime:

- `precision mediump float;` is required in GLSL ES and **invalid** in desktop
  GLSL 1.10.
- `GL_OES_EGL_image_external` is Android-only; Mesa rejects unknown extensions
  outright.
- GLSL 3.30 renamed the sampler read to `texture`.

`src/gl/GlslCompat.cpp` rewrites the source once per program at startup. The
alternative is a second copy of 63 shaders.

### MSVC caps a string literal at 16380 bytes

It truncates silently. `scripts/embed-shaders.ps1` splits anything long into
adjacent literals on line boundaries.

### `.gitignore` and the bare `lib/` rule

A bare `lib/` matches at **any depth**, so it also excluded
`scripts/harness/lib/` — the harness's shared functions. Every `wave-*.ps1`
dot-sources that file, so in a fresh clone they ran with none of their
functions defined. The symptom was a message about a property that does not
exist, from a script that had sourced nothing.

All build-output rules are now anchored: `/lib/`, not `lib/`.

### PowerShell unwraps single-element collections

A function returning a one-element list returns the element itself. A caller
asking for `.Count` on a string throws under `Set-StrictMode`. Any harness
function returning a collection must wrap it: `return ,@($items)`.

### rc.exe does not track its includes

`app.rc` includes `ui.html`, but the build system does not know that. Editing
the interface alone relinked the executable without recompiling the resource,
so the app kept showing the previous page. `src/CMakeLists.txt` declares the
dependency with `OBJECT_DEPENDS`.

### Sampler uniforms are integers

`sampler2D` is an integer uniform naming a texture unit. Setting one with
`glUniform1f` is an error the driver reports to `glGetError` and then ignores,
so the sampler keeps its default of **0** — which is also where `uTexture` was
bound.

The effect: `uBackdrop` read the *layer* instead of the frame, so the
compositor blended every layer against itself. Green over green is green in
every blend mode, which made all 21 of them render as a plain source-over that
happened to look correct for Normal.

Use `setUniformInt` for samplers. It is the same class of mistake as the next
one, and both read as a shader problem rather than a uniform-type problem.

### `blendColor` had no branch for Normal

The recovered shader dispatches `uMode` starting at 1. Mode 0 — Normal — fell
through to the final `return`, which is **Luminosity**. A layer set to Normal
composited its brightness from itself and its colour from the backdrop, giving
`255,105,105` for green over red rather than green.

This one is worth noting because it is a genuine gap in the recovered source,
not a port bug. A `if (uMode == 0) return s;` was added.

### The compositor read the frame it was drawing into

`compositeLayer` bound `_frame.texture` as `uBackdrop` while drawing into the
framebuffer that texture was attached to. Reading a texture attached to the
bound framebuffer is undefined, and the driver returned a zero alpha — so the
shader's blend weight `uStrength * dst.a` was zero and every mode composited as
source-over.

Fixed by rendering the composite into a scratch target and copying it back. One
extra full-screen pass per layer, which is what a correct composite costs when
the backdrop cannot be read in place.

**All three bugs produce the same symptom.** That is the lesson: "every blend
mode renders as source-over" had three independent causes, and reading the
shader would have found none of them. `Renderer::checkBlendModes` reads pixels
and is the regression test for all three.

### Grid and flex items will not shrink below their content

The viewer holds a canvas with an intrinsic 1280x720 size. A grid row defaults
to `min-height: auto`, which refuses to shrink below that, so the row grew and
pushed the timeline and status bar off the bottom of the window. The fix is
`grid-template-rows: minmax(0, 1fr)` plus `min-height: 0` on the items.

The layout report exists to catch this class of bug without guessing at CSS.

### Screenshots can lie about layout

`MoveWindow` in a capture script resizes the window *after* the page has
measured itself. A screenshot taken that way shows a clipped interface on a
layout that is actually correct. Trust `layout_report.json` over a screenshot.

---

## 6. What is not done

Stated plainly rather than left to be discovered.

- **No audio.** Import detects an audio stream and the timeline shows it, but
  nothing decodes or mixes it. The encoder is configured for it.
- **No 3D compositor.** The camera uniforms are in the model and the particle
  vertex shader is recovered, but the renderer does not project a layer through
  a camera.
- **No multi-pass effects.** The catalogue has a `passes` field and a
  `passShaders` list, and the separable Blur uses two passes. The Glow shader
  is written for a pyramid but runs as one pass — the catalogue says `passes =
  1` because that is what the renderer actually does.
- **No GPU video decode.** ffmpeg decodes to system memory; the frame is
  uploaded.
- **No frame cache.** A media layer decodes once per layer per frame, which is
  the current cost of scrubbing.
- **No audio waveform on the timeline.**

Next three, in order: the frame cache, audio, the multi-pass pipeline.

### One effect is deliberately missing

`soften_feather` is not in the catalogue. R8 split the shader into a
declaration head and a body tail and hoisted the join into the DEX's shared
string pool. The head ends at `vTex + o * uTexelSize * ` and what the loop
multiplies by is gone. Both candidates — a horizontal axis and a vertical one —
are consistent with what survives, so shipping either would be a guess.
`EffectCatalog.cpp` says so where the effect would be.

A "Soften" entry is a conspicuous gap in the Blur category. Closing it means
either recovering the fragment from the DEX with more context, or writing the
effect fresh.

---

## 7. Where the recovery artifacts are

**Not in the repository.** These live in a scratch workspace:

```
C:\1234\apk_work\
  extracted\            the unzipped APK
  dex_strings.txt       all 31221 DEX string constants, one per line
  allglsl\              every GLSL-ish string by DEX index
  shader_manifest.json  stem -> source, the input to the embed script
  finalize.py           assembles the shader set, including the chunked joins
  *.py                  the one-shot patch scripts used during the build-out
```

If that directory is lost, the shaders are still in `src/gl/shaders/` and the
repository is unaffected — the workspace was only needed to produce them. It
would be needed again to recover anything further from the APK.

**The APK itself** is at `C:\1234\undow my app to win app\Keyflow.apk`.

---

## 8. How work is organised

Parallel work uses the wave harness, adapted from the `storynode` repository.
One task, one branch, one worktree, one build directory, and a declared file
territory per task.

```powershell
scripts\harness\wave-new.ps1 -Wave 1 -Task renderer   # start
scripts\harness\wave-list.ps1                         # what is running
scripts\harness\wave-done.ps1 -Task renderer          # test and finish
scripts\harness\wave-clean.ps1 -Task renderer         # abandon
```

A task declares the files it owns in `tasks\wave-<n>\<task>.md`. The harness
refuses to start a task whose territory overlaps another in the same wave —
checked before the worktree exists, because a conflict found then costs
nothing.

Both a PowerShell and a Bash implementation exist and are kept behaviourally
identical: a task may be started from one shell and finished from the other.

### The wave plan

| Wave | Task | Scope |
|---|---|---|
| 0 | core | model, format, catalogue, expressions, shader table — **done** |
| 1 | renderer | compositor, generated layers, text, adjustment layers |
| 2 | interface | timeline interaction, keyframe editing, expression validation |
| 3 | pipeline | frame cache, render queue, audio |

Wave 0 is substantially complete. Waves 1–3 are each part-done — the renderer
composites and generates layers, the interface is complete and reports clean,
the pipeline imports and exports. What remains in each is listed in its task
file.

---

## 9. Conventions worth keeping

- **Premultiplied alpha throughout.** The blend maths and the chroma key's
  un-multiply both assume it. A straight-alpha layer composites with a halo.
- **One table, four consumers.** `EffectCatalog.cpp` drives the effect browser,
  the inspector's controls, the renderer's uniform binding and project files.
  Four lists would drift; the symptom of drift is a control that moves
  something the renderer does not read.
- **The page carries no list of effects**, no ranges, no labels. All of it
  arrives in the catalogue message. Adding an effect in `core` makes it appear
  in the interface with no change to `src/ui/`.
- **The page never edits its own copy of the document.** Every change is a
  message to the host and a fresh snapshot back. One copy, and it is the
  host's.
- **Comments say why, not what.** The recovered shaders do this and it is worth
  matching — the note about `pow(0.0, e)` being undefined in GLSL ES is the
  reason a stray texel no longer appears mid-frame, and nothing in the code
  would tell you that.

---

## 10. Reading order

For someone new to the codebase, in this order:

1. `README.md` — what it is and how to build it
2. `docs/ARCHITECTURE.md` — the three interfaces and why ffmpeg is a subprocess
3. `docs/SHADERS.md` — where the shaders came from and what was done to them
4. `src/core/EffectCatalog.cpp` — the table everything else reads
5. `src/app/Bridge.h` — the protocol, with the reasoning in the header
6. `src/gl/Renderer.cpp` — the pipeline, from layer texture to composite
7. `src/ui/assets/ui.html` — the interface, one file, no build step
