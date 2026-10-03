# Wave 1: renderer

## Goal

The OpenGL compositor, end to end: layer textures, the effect stack with
ping-pong targets, the compositor's blend modes, the generated layers (solids,
shapes, text) and the frame readback. Substantially done — 63 of 63 shaders
compile and a solid layer renders to a PNG. What remains is the parts that need
a second layer kind to exercise: text rasterisation at a real font size, shape
paths with curves, and the adjustment-layer pass.

## Territory

- src/gl/**
- src/media/**

## Deliverable

- `keyflow.exe --self-test` renders a composition containing one of every
  layer kind and exits 0.
- Every shader in the table compiles against the machine's driver.
- An adjustment layer applies its effect stack to the layers beneath it.
- A media layer decodes, uploads and composites at the right size and place.
- Text renders through the platform's font engine, correctly scaled from
  composition units to the render size.

## Notes

The renderer is where the recovered shaders stop being text and start being
pixels, so this is the wave where a mistake in the catalogue surfaces. The
`--self-test` mode exists for exactly that: it compiles every shader and
renders a frame, and its exit code is the answer.

Frame generation for non-media layers lives here rather than in `core` because
it produces pixels, and pixels need a size.
