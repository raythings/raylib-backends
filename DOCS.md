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

## WebGPU backend (rlwg)

`rlwg` is a browser WebGPU backend (Emscripten / emdawnwebgpu) that re-implements
the rlgl API and emulates OpenGL 3.3 1:1, exactly like rlvk/rlmt. It is a fresh
implementation on the WebGPU C API (`webgpu/webgpu.h`) - it does **not** use raygpu,
and so does not inherit raygpu's shader caveats.

Files: `backends/rlwg/rlwg.h` (+ `rlwg_impl.h`, `rlwg_impl2.h`), the default shaders
in `backends/rlwg/rlwg_default_wgsl.h`, the wrapper `backends/rlwg/rlgl.h`, and the
web platform `backends/rlwg/platforms/rcore_web.c`.

### 1:1 OpenGL 3.3 shader contract

- Fixed attribute locations: 0 position, 1 texcoord, 2 normal, 3 color, 4 tangent,
  5 texcoord2.
- The genuine rlgl path: raw model-space vertices upload and the shader transforms by
  the `mvp` uniform (no CPU pre-transform), so stock raylib shaders behave as on GL.
- Loose GL uniforms are packed into uniform buffers; the C mirror matches the default
  shader's byte layout so `rlSetUniform`/`rlSetUniformMatrix` write by offset.
- Two unavoidable WebGPU deviations, both invisible to rlgl callers:
  1. clip-space z `[-1,1] -> [0,1]` is remapped in the default vertex shader;
  2. matrices upload column-major (raylib's `Matrix` struct memory order is the
     transpose, so they are reordered like `MatrixToFloat` before upload).

### Custom shaders

Browser WebGPU accepts **WGSL only** (no SPIR-V). Pass WGSL to `LoadShader`/
`rlLoadShaderProgram` (a single source string holding `vs_main` + `fs_main`).
GL 3.3 GLSL must be translated offline (e.g. glslang -> SPIR-V -> Tint -> WGSL); no
shader compiler is bundled into the WASM payload.

### Build & run

```
emcmake cmake -S . -B build-web -DRAYLIB_DIR=/path/to/raylib -DCMAKE_BUILD_TYPE=Release
cmake --build build-web -j
# serve build-web/examples and open clear_cube_webgpu.html in a WebGPU browser
python3 -m http.server --directory build-web/examples
```

Attach to your own raylib target with `raylib_backend_attach(TARGET raylib BACKEND
WEBGPU)` under the Emscripten toolchain. Link flags pulled in: `--use-port=emdawnwebgpu`,
`-sASYNCIFY`, `-sALLOW_MEMORY_GROWTH=1`.

### Current rlwg limits

- Immediate-mode batch path (shapes, text, DrawCube/DrawGrid via batch) is complete.
- Generic mesh VAO draws (`rlDrawVertexArray*`), cubemaps, mipmap generation, pixel
  readback, and compute/SSBO are stubbed pending follow-up.

## Current Limits

- Vulkan, Metal, and WebGPU (browser) are the active backends.
- The examples are smoke tests, not full raylib feature coverage.
