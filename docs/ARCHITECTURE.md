# Architecture

## What this is

Keyflow is a GL-shader video editor. This is its Windows port, recovered from
the Android build's APK and rebuilt as a native Win32 application with an HTML
interface.

The recovered material is the substance: 63 GLSL shaders, the effect
catalogue, the document model, the project format and the expression language.
Everything else is the shell that makes them run on a desktop.

## The shape

```
   keyflow.exe
   |
   +-- Win32 window ......................... src/app/main.cpp
   |     |
   |     +-- WebView2 ....................... src/ui/assets/ui.html
   |     |     the interface, compiled into the executable as a resource
   |     |     and loaded with NavigateToString
   |     |
   |     +-- Bridge ......................... src/app/Bridge.{h,cpp}
   |     |     the message protocol: JSON in, JSON out, no WebView2 types
   |     |
   |     +-- Renderer ....................... src/gl/Renderer.{h,cpp}
   |     |     the OpenGL compositor
   |     |
   |     +-- Media .......................... src/media/Media.{h,cpp}
   |           ffprobe and ffmpeg, as subprocesses
   |
   +-- keyflow_core ......................... src/core/
         Model, ProjectIO, EffectCatalog, Expression, the shader table
```

`keyflow_core` has no Windows header in it and no GL. That is deliberate: the
model, the project format, the catalogue and the expression evaluator are the
parts most likely to be wrong, and being able to test them without a window
means the tests run on any machine and in CI on a Linux runner.

## The three interfaces

### Page to host: JSON messages

Every message is a JSON object with a `type`. The page sends intent ("set this
parameter"); the host applies it to the document and broadcasts a fresh
snapshot.

The page never patches its own copy optimistically. That is the property that
keeps the two sides from drifting into disagreeing about the document: there is
exactly one copy, and it is the host's.

`Bridge` is deliberately free of WebView2 types. It is the dispatcher, not the
transport: the host calls `HandleMessage` with text that arrived from the web
view, and the bridge calls back with text to send. That separation is what lets
the protocol be tested without a window, and `tests/BridgeTests.cpp` does
exactly that with a recording host.

A message that changes nothing — selecting a layer, moving the playhead — does
not mark the document dirty. Marking every message dirty would make a freshly
opened project look modified the moment the user clicked something.

### Host to page: `window.__keyflowReceive`

The page cannot be reached with `postMessage` — that is the page-to-host
direction — so the host calls a named function with a JSON string. Both paths
land in the same handler, so a message behaves the same however it arrived.

The host escapes the JSON as a JavaScript string literal before calling.
Passing it bare would evaluate it as code, which is both wrong and a way for a
document field to become script.

### Renderer: the catalogue

`EffectCatalog` is one table with four consumers: the page's effect browser,
the inspector's controls, the renderer's uniform binding, and project files,
which name effects by their catalogue ids.

One table rather than four. The alternative is two lists that drift, and the
symptom of drift is a control that moves something the renderer does not read.

Binding is mechanical: a parameter named `tolerance` in the catalogue binds to
`uTolerance` in the shader. A parameter the shader does not read is not an
error — the shader simply has fewer knobs than the spec offers.

## The render pipeline

```
   for each layer, bottom to top:
     |
     +-- source texture
     |     media: decoded by ffmpeg, uploaded
     |     solid: a flat fill
     |     shape: the path rasterised with 2x2 coverage
     |     text:  drawn with GDI, alpha rebuilt from luminance
     |
     +-- effect stack
     |     one full-screen pass per effect, ping-ponging between two targets
     |     parameters evaluated once per frame, not once per pass
     |
     +-- composite
           the compositor shader, with the layer's blend mode
```

Premultiplied alpha throughout. That is not a style choice: the blend maths and
the chroma key's un-multiply both assume it, and a layer that arrives
straight-alpha composites with a halo.

### Why the shaders are rewritten at compile time

The shaders are GLSL ES 1.00, written for Android. Desktop GL is a different
language in three small ways that all show up as a syntax error:

- GLSL ES requires a precision statement; desktop GLSL 1.10 does not accept one
  at all.
- `GL_OES_EGL_image_external` is an Android camera extension with no desktop
  meaning, and Mesa rejects an unknown extension outright.
- GLSL 3.30 renamed the sampler read to `texture`.

`GlslCompat.cpp` rewrites the source once per program at startup. The
alternative is maintaining two copies of 63 shaders, which is 63 more places
for them to diverge.

### Why ffmpeg is a subprocess

Not linked. Three reasons:

- The build has no third-party link step. libav needs its own configure, its
  own runtime, and its DLLs beside the executable.
- A decoder crash cannot take the editor with it. ffmpeg is a separate process,
  and a malformed file kills that process.
- The user almost certainly already has it.

The cost is a process spawn per decode, which matters for scrubbing. The frame
cache exists to pay that down.

## Testing

Three layers, each answering a question the others cannot.

**Unit tests** (`tests/`, run by ctest, no window, any platform) — is the model
right? Do keyframes interpolate, does a project round-trip, is every catalogue
entry consistent, is every shader well-formed? 122 cases.

**The GL self-test** (`keyflow.exe --self-test`) — do the shaders compile? This
is the one question the unit tests cannot answer, because they have no GL
context. It compiles all 63 shaders, renders a composition, writes a PNG and
returns an exit code.

**The layout report** (`keyflow.exe --layout-report`) — is the interface
intact? The page measures its own bands and names the ones that collapsed or
fell off the bottom. A layout bug becomes a list of numbers rather than an
afternoon of guessing at CSS.

## The build

```
scripts/fetch-deps.ps1              # WebView2 SDK, once per worktree
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

The WebView2 SDK is fetched rather than committed: it is about 9 MB, it is not
ours to license, and a fresh clone should not carry it. When it is absent the
core library still builds and the tests still run, so a contributor without the
SDK can work on the model and the shaders.

## What the port does not have

Stated plainly rather than left to be discovered:

- **No audio.** Import detects an audio stream and the timeline shows it, but
  nothing decodes or mixes it. The encoder is configured for it.
- **No 3D compositor.** The camera uniforms exist in the model and the particle
  vertex shader is recovered, but the renderer does not yet project a layer
  through a camera.
- **No multi-pass effects.** The catalogue carries a `passes` field and the
  glow shader is written for a pyramid, but the renderer runs one pass per
  effect.
- **No GPU video decode.** ffmpeg decodes to system memory and the frame is
  uploaded. Hardware decode would need a different upload path.

The frame cache, audio, and the multi-pass pipeline are the three things worth
doing next, in that order.
