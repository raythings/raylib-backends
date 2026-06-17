# raylib-backends

Attach Vulkan or Metal rendering backends to a raylib build that you own.

`raylib-backends` is not a raylib fork. It does not contain the full raylib
source tree. You provide an upstream raylib checkout, build a normal `raylib`
target in your project, then call the CMake helper in this package to attach one
backend.

## Supported Backends

- `VULKAN`: `rlvk`, used by Linux and Android consumers
- `METAL`: `rlmt`, used by macOS consumers
- `WEBGPU`: not implemented

## Requirements

- CMake 3.20 or newer
- An upstream raylib checkout available somewhere on disk
- A compiler/toolchain for your target platform
- Vulkan SDK or platform Vulkan libraries for `VULKAN`
- Xcode command line tools for `METAL`
- Android Gradle Plugin, NDK, and Vulkan-capable device or emulator for Android

## Directory Layout

- `cmake/RaylibBackend.cmake`: CMake helper functions for consumers
- `backends/rlvk`: Vulkan backend source and headers
- `backends/rlvk/platforms`: Android SurfaceView platform glue for Vulkan
- `backends/rlmt`: Metal backend source and headers
- `third_party/spirv_reflect`: SPIR-V reflection dependency used by Vulkan
- `examples`: desktop and Android cube examples
- `patches`: notes for generated overlays; this is not a patch queue

## Basic CMake Usage

Build raylib from your own checkout, then attach a backend:

```cmake
set(RAYLIB_DIR "/absolute/path/to/raylib")
set(RAYLIB_BACKENDS_DIR "/absolute/path/to/raylib-backends")

include("${RAYLIB_BACKENDS_DIR}/cmake/RaylibBackend.cmake")

add_library(raylib STATIC
    "${RAYLIB_DIR}/src/rcore.c"
    "${RAYLIB_DIR}/src/rshapes.c"
    "${RAYLIB_DIR}/src/rtextures.c"
    "${RAYLIB_DIR}/src/rtext.c"
    "${RAYLIB_DIR}/src/rmodels.c"
    "${RAYLIB_DIR}/src/raudio.c"
)

target_include_directories(raylib PUBLIC
    "${RAYLIB_DIR}/src"
    "${RAYLIB_DIR}/src/platforms"
)

raylib_backend_attach(
    TARGET raylib
    BACKEND VULKAN
    RAYLIB_BACKENDS_DIR "${RAYLIB_BACKENDS_DIR}"
)
```

For macOS Metal, change the backend:

```cmake
raylib_backend_attach(
    TARGET raylib
    BACKEND METAL
    RAYLIB_BACKENDS_DIR "${RAYLIB_BACKENDS_DIR}"
)
```

`raylib_backend_attach()` only changes the existing CMake target. It adds
include directories, backend sources, compile definitions, and system link
libraries. It does not edit files in your raylib checkout.

## Build Desktop Examples

The desktop examples build a cube app against your raylib checkout.

```bash
cmake -S /absolute/path/to/raylib-backends \
  -B /tmp/raylib-backends-build \
  -DRAYLIB_DIR=/absolute/path/to/raylib

cmake --build /tmp/raylib-backends-build
```

On macOS this builds the Metal cube example. On Linux this builds the Vulkan
cube example.

## Build The Android Example

The Android example is a small `Activity` + `SurfaceView` app that renders a
cube with the Vulkan backend.

```bash
cd /absolute/path/to/raylib-backends/examples/android

gradle :app:installDebug \
  -PRAYLIB_DIR=/absolute/path/to/raylib \
  -PRAYLIB_BACKENDS_DIR=/absolute/path/to/raylib-backends \
  -PRAYGPU_DIR=/absolute/path/to/raygpu
```

If you prefer using an existing Gradle wrapper:

```bash
/path/to/gradlew -p /absolute/path/to/raylib-backends/examples/android \
  :app:installDebug \
  -PRAYLIB_DIR=/absolute/path/to/raylib \
  -PRAYLIB_BACKENDS_DIR=/absolute/path/to/raylib-backends \
  -PRAYGPU_DIR=/absolute/path/to/raygpu
```

## Android SurfaceView Integration

Android SurfaceView support lives in `backends/rlvk/platforms`. Consumers that
need `PLATFORM_ANDROID_SURFACE` should generate an overlay source in the build
directory:

```cmake
raylib_backend_prepare_android_surface_overlay(
    OUT_VAR RAYLIB_RCORE_SRC
    RAYLIB_DIR "${RAYLIB_DIR}"
    RAYLIB_BACKENDS_DIR "${RAYLIB_BACKENDS_DIR}"
)

add_library(raylib STATIC
    "${RAYLIB_RCORE_SRC}"
    "${RAYLIB_DIR}/src/rshapes.c"
    "${RAYLIB_DIR}/src/rtextures.c"
    "${RAYLIB_DIR}/src/rtext.c"
    "${RAYLIB_DIR}/src/rmodels.c"
    "${RAYLIB_DIR}/src/raudio.c"
)
```

The generated file is written under the CMake build directory. Your upstream
raylib checkout remains unchanged.

## Metal Overlay

The Metal backend needs a backend-aware `rcore.c` overlay for desktop GLFW
integration. Generate it before creating the raylib target:

```cmake
raylib_backend_prepare_rcore_overlay(
    OUT_VAR RAYLIB_RCORE_SRC
    RAYLIB_DIR "${RAYLIB_DIR}"
    RAYLIB_BACKENDS_DIR "${RAYLIB_BACKENDS_DIR}"
)
```

Use `${RAYLIB_RCORE_SRC}` instead of `${RAYLIB_DIR}/src/rcore.c` in your
`raylib` target.

## Compatibility Contract

- Your project owns the upstream raylib checkout.
- This package owns backend implementation files and build helpers.
- Backend selection is done with compile definitions and CMake target changes.
- Any required raylib entrypoint changes are generated into the build tree.
- No workflow should require committing edits inside `raylib/src`.

This keeps raylib updates local and reviewable: update your raylib checkout,
reconfigure the consumer build, and inspect any overlay-generation failures as
normal build integration issues rather than fork conflicts.

## Troubleshooting

- `raylib_backend_attach: target does not exist`: create your `raylib` target
  before calling `raylib_backend_attach()`.
- `already has backend attached`: each raylib target can have one backend.
- Missing Vulkan symbols: install the Vulkan SDK or link the platform Vulkan
  library for your target.
- Android cannot find `rcore_android_surface.h`: make sure
  `raylib_backend_attach(TARGET raylib BACKEND VULKAN ...)` is called and that
  consumers include backend headers through the raylib target.
