# Wave 0: core

## Goal

The document model, the project format, the effect catalogue, the expression
evaluator and the shader table — everything that is testable without a window,
a GPU or a page. This is the wave everything else is built on, and it is
already substantially done: the port recovered the Android app's model, its
project format, its catalogue and its 63 shaders, and the tests pass.

## Territory

- src/core/**
- src/gl/ShaderTable.cpp
- src/gl/ShaderTable.h
- src/gl/shaders/**
- tests/JsonValueTests.cpp
- tests/ModelTests.cpp
- tests/ProjectIOTests.cpp
- tests/CatalogTests.cpp
- tests/ExpressionTests.cpp
- tests/TestFramework.cpp
- tests/TestFramework.h
- scripts/embed-shaders.ps1

## Deliverable

- `keyflow_core` builds on any platform, with no Windows header in it.
- `keyflow_tests` passes: model, keyframes, expressions, project round-trip,
  catalogue integrity, shader table integrity.
- Every shader in `src/gl/shaders/` is embedded in `ShaderTable.cpp` by
  `scripts/embed-shaders.ps1`, and the table matches the files.

## Notes

The catalogue is the load-bearing piece: the page's effect browser, the
inspector's controls and the renderer's uniform binding all read it, so a
parameter that is wrong here is wrong in three places at once. The tests in
`CatalogTests.cpp` exist for that reason.

Adding an effect is: write the `.glsl`, add an entry to `EffectCatalog.cpp`,
re-run `scripts/embed-shaders.ps1`. No other file changes.
