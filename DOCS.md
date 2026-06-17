# Consumer Guide

This guide explains how to integrate `raylib-backends` into an application that
already builds raylib.

## 1. Keep Raylib Separate

Put raylib and `raylib-backends` in separate directories:

```text
deps/
  raylib/
  raylib-backends/
```

Do not copy `raylib/src` into `raylib-backends`. Do not apply checked-in patches
to `raylib/src`. The consumer project decides which raylib revision to use.

## 2. Include The Helper

```cmake
set(RAYLIB_DIR "${CMAKE_SOURCE_DIR}/deps/raylib")
set(RAYLIB_BACKENDS_DIR "${CMAKE_SOURCE_DIR}/deps/raylib-backends")

include("${RAYLIB_BACKENDS_DIR}/cmake/RaylibBackend.cmake")
```

## 3. Build A Raylib Target

For a plain Vulkan build:

```cmake
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
```

For Metal or Android SurfaceView, generate the `rcore` source first and use the
generated path in the target.

Metal:

```cmake
raylib_backend_prepare_rcore_overlay(
    OUT_VAR RAYLIB_RCORE_SRC
    RAYLIB_DIR "${RAYLIB_DIR}"
    RAYLIB_BACKENDS_DIR "${RAYLIB_BACKENDS_DIR}"
)
```

Android SurfaceView:

```cmake
raylib_backend_prepare_android_surface_overlay(
    OUT_VAR RAYLIB_RCORE_SRC
    RAYLIB_DIR "${RAYLIB_DIR}"
    RAYLIB_BACKENDS_DIR "${RAYLIB_BACKENDS_DIR}"
)
```

Then use:

```cmake
add_library(raylib STATIC
    "${RAYLIB_RCORE_SRC}"
    "${RAYLIB_DIR}/src/rshapes.c"
    "${RAYLIB_DIR}/src/rtextures.c"
    "${RAYLIB_DIR}/src/rtext.c"
    "${RAYLIB_DIR}/src/rmodels.c"
    "${RAYLIB_DIR}/src/raudio.c"
)
```

## 4. Attach One Backend

```cmake
raylib_backend_attach(
    TARGET raylib
    BACKEND VULKAN
    RAYLIB_BACKENDS_DIR "${RAYLIB_BACKENDS_DIR}"
)
```

or:

```cmake
raylib_backend_attach(
    TARGET raylib
    BACKEND METAL
    RAYLIB_BACKENDS_DIR "${RAYLIB_BACKENDS_DIR}"
)
```

The helper rejects conflicting backend selections on the same target.

## 5. Link Your App

```cmake
add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE raylib)
```

Link other platform libraries required by your application as usual.

## Android Notes

`PLATFORM_ANDROID_SURFACE` is for apps that own the Android `Activity` and pass
an externally-owned `ANativeWindow` into raylib. The host calls the functions in
`backends/rlvk/platforms/rcore_android_surface.h` to provide the window, input,
surface lifecycle, and multi-surface control.

Use the Android example as the reference layout:

```text
examples/android/
  app/src/main/
  app/src/main/cpp/
```

Required Gradle properties:

- `RAYLIB_DIR`: upstream raylib checkout
- `RAYLIB_BACKENDS_DIR`: this package
- `RAYGPU_DIR`: glslang source used by the Vulkan backend

## Desktop Examples

Build from the package root:

```bash
cmake -S . -B build -DRAYLIB_DIR=/absolute/path/to/raylib
cmake --build build
```

The desktop example source is `examples/clear_cube.c`. The CMake example
selects Metal on macOS and Vulkan on Linux.

## What The Helper Changes

`raylib_backend_attach()` adds target properties only:

- backend include directories
- backend source files
- backend compile definitions
- backend link libraries

Overlay helpers create generated `.c` files under the CMake build directory.
They do not write to your raylib checkout.

## When Updating Raylib

1. Update the consumer-owned raylib checkout.
2. Reconfigure your build from a clean CMake build directory.
3. Build the desktop or Android cube example for the backend you use.
4. If overlay generation fails, update the helper pattern in
   `cmake/RaylibBackend.cmake` instead of patching `raylib/src`.

## Current Limits

- Vulkan and Metal are the active backends.
- WebGPU is intentionally not implemented.
- The examples are smoke tests, not full raylib feature coverage.
