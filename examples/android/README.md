# Android Example Project

This is a dedicated Android example project for `raylib-backends`.

It is intentionally small and expects a consumer-owned upstream `raylib`
checkout.

## What it does

- builds a small Android `Activity` with a `SurfaceView`
- builds a local `raylib` target from the consumer checkout
- attaches the Vulkan backend with `raylib_backend_attach()`
- renders a simple cube smoke test

## Required paths

- `RAYLIB_DIR`: upstream `raylib` checkout
- `RAYLIB_BACKENDS_DIR`: path to `raylib-backends`
- `RAYGPU_DIR`: path to the glslang source used by `rlvk`

## Example build

```bash
cd raylib-backends/examples/android
gradle :app:installDebug \
  -PRAYLIB_DIR=/absolute/path/to/raylib \
  -PRAYLIB_BACKENDS_DIR=/absolute/path/to/raylib-backends \
  -PRAYGPU_DIR=/absolute/path/to/raygpu
```

If this project does not have a local Gradle wrapper yet, run the same command
through any compatible wrapper with `-p /path/to/raylib-backends/examples/android`.

The example is minimal by design. It is meant to prove that a consumer can
build and launch an Android app while keeping upstream `raylib` outside this
package.
