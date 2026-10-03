# The shaders

63 GLSL ES 1.00 shaders, recovered from the Android build's `classes.dex`.

## Where they came from

The Android app compiled its shaders into the DEX as string constants. They
survive decompilation intact because a shader is data, not code: R8 obfuscates
class and method names, but a string constant it cannot reach into is left
exactly as it was written.

So the shaders are not a reimplementation. They are the original source,
recovered by walking the DEX string table and picking out the entries that
contain a shader entry point. The comments survived with them, which is why the
recovered files explain their own reasoning — the bit about `pow(0.0, e)` being
undefined in GLSL ES, the note about sin-based hashes losing their high
frequencies at reduced precision.

## What was recovered

| Category | Shaders |
|---|---|
| Colour | brightness/contrast, exposure/gamma, hue/saturation, HSL, levels, LUT, invert, fill, opacity |
| Keying | chroma key with spill suppression |
| Blur | box, bokeh/iris, masked, separable, pyramid down/up, 13-tap downsample |
| Glow | bloom with tint and chromatic aberration, highlight recovery, highlight suppress |
| Stylise | CMYK halftone, pixelate, scanlines, fractal noise, ASCII, sharpen |
| Generate | audio spectrum, 4-colour gradient, linear/radial gradient, circle, rectangle, ellipse, radial repeat, light flare |
| Motion | wiggle, ripple, swirl, transform, directional motion blur |
| Transition | rectangular, linear and radial wipe |
| Distort | chromatic aberration, grain, outline, edge detect, stroke, drop shadow |
| Composite | the compositor with 21 blend modes, backdrop blend, adjustment layer, mask apply |

Plus the vertex units: a quad, an MVP/ST transform, a passthrough, and a
particle system.

## Three things had to be done to them

**Desktop GLSL.** The shaders are GLSL ES 1.00 and desktop GL is a different
language in three small ways — precision statements, Android-only extensions,
and the sampler read's name. `GlslCompat.cpp` rewrites the source once per
program at startup rather than keeping a second copy of 63 files.

**Two templates resolved.** The Android build generated some shaders per
variant, substituting a compile-time constant. On a desktop, where a program is
compiled once and reused, a uniform is the same work:

- `blur_simple` had an `AXIS` placeholder for the separable blur's direction.
  It became `blur_simple_x` and `blur_simple_y` — two programs that differ by a
  constant vector are clearer as two programs than as one with a branch.
- `blur_pyramid_up` had `LEVEL_INDEX`, which is per-pass state, so it became a
  uniform.

**One shader converted.** `opacity` used `samplerExternalOES`, Android's camera
texture type. The maths is the same for any sampler, so only the type changed.
The shader that is genuinely Android-only — the camera path's external-texture
copy — is not in the table, because a desktop driver cannot resolve the type it
needs.

## What did not survive

Two units were split by the shrinker into a declaration head and a body tail,
with the join hoisted into the DEX's shared string pool:

- **`matte_choke`** was recovered. The head ends mid-expression at
  `vec2 d = o * uTexelSize * ` and the tail continues `;` then uses `d`. The
  loop is 8 iterations at a π/4 stride with a growing radius, which is an
  8-point ring at increasing distance — alpha erosion, and the join is forced
  by the loop's own constants.

- **`soften_feather`** was not. Its head ends at
  `vTex + o * uTexelSize * ` and the join is gone. Both candidates — a
  horizontal axis and a vertical one — are consistent with what survives, so
  shipping either would be a guess. A "Soften" effect is a conspicuous gap in
  the Blur category, and `EffectCatalog.cpp` says why rather than leaving it to
  be rediscovered.

Two more units are incomplete for the same reason and are not in the table:
a chromatic-aberration variant and a `blur_masked` companion. The recovered
copies are still in the APK's DEX if a future version wants to try again with
more context.

## Working on them

The `.glsl` files under `src/gl/shaders/` are the source of truth.
`scripts/embed-shaders.ps1` compiles them into `ShaderTable.cpp`, which is what
the executable actually reads.

```
scripts/embed-shaders.ps1          # regenerate the table
scripts/embed-shaders.ps1 -Check   # fail if the table is stale, for CI
```

Then:

```
build/dev/bin/keyflow.exe --self-test
```

which compiles every shader against the machine's driver and reports the ones
that fail. A shader that does not compile is an effect that renders nothing
with no error anywhere, which is why this check exists as a separate step
rather than being left to whoever notices the effect is not working.

## Conventions

The recovered shaders share a set of conventions worth keeping:

- **Premultiplied alpha.** `uTexture` arrives premultiplied. A shader doing
  colour maths un-multiplies first (`src.rgb / max(src.a, 1e-4)`) and
  re-multiplies at the end. A shader that skips this composites with a halo.
- **`uLayerRect`** is the layer's box in the target's uv, `(minX, minY, maxX,
  maxY)`. Taps that would read outside it clamp to the edge, so a blur does not
  smear the border across the frame.
- **`uTexelSize`** is `1/width, 1/height` of the *target*. A radius in "1080p
  pixels" is divided by it, which is how a blur stays the same size relative to
  the picture at any render resolution.
- **Aspect correction.** A blur's horizontal axis divides by the aspect so the
  kernel stays round rather than stretching on a wide composition.
- **`uHeadroom`** carries values above 1.0 through the chain for glow, which is
  what lets a highlight survive to be bloomed rather than clipping first.
