cmake_minimum_required(VERSION 3.22)
project(raylib_backends_android_example LANGUAGES C CXX)

if(NOT RAYLIB_DIR)
    message(FATAL_ERROR "RAYLIB_DIR is required")
endif()
if(NOT RAYLIB_BACKENDS_DIR)
    message(FATAL_ERROR "RAYLIB_BACKENDS_DIR is required")
endif()
if(NOT RAYGPU_DIR)
    message(FATAL_ERROR "RAYGPU_DIR is required")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include("${RAYLIB_BACKENDS_DIR}/cmake/RaylibBackend.cmake")

set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${RAYGPU_DIR}/amalgamation/glslang" "${CMAKE_BINARY_DIR}/glslang" EXCLUDE_FROM_ALL)

raylib_backend_prepare_android_surface_overlay(
    OUT_VAR RAYLIB_RCORE_SRC
    RAYLIB_DIR "${RAYLIB_DIR}"
    RAYLIB_BACKENDS_DIR "${RAYLIB_BACKENDS_DIR}"
)

set(RAYLIB_SRC
    ${RAYLIB_RCORE_SRC}
    ${RAYLIB_DIR}/src/rshapes.c
    ${RAYLIB_DIR}/src/rtextures.c
    ${RAYLIB_DIR}/src/rtext.c
    ${RAYLIB_DIR}/src/rmodels.c
    ${RAYLIB_DIR}/src/raudio.c
)

add_library(raylib STATIC ${RAYLIB_SRC})
target_include_directories(raylib PUBLIC
    ${RAYLIB_DIR}/src
    ${RAYLIB_DIR}/src/platforms
    ${ANDROID_NDK}/sources/android/native_app_glue
    ${RAYGPU_DIR}/amalgamation/glslang/glslang/Include
    ${RAYGPU_DIR}/amalgamation/glslang/glslang/Public
)
target_compile_definitions(raylib PUBLIC PLATFORM_ANDROID_SURFACE RAYLIB_USE_RLVK RLVK_ENABLE_GLSLANG=1)
target_link_libraries(raylib PUBLIC android log EGL GLESv3 vulkan)
raylib_backend_attach(TARGET raylib BACKEND VULKAN RAYLIB_BACKENDS_DIR ${RAYLIB_BACKENDS_DIR})
target_link_libraries(raylib PUBLIC glslang glslang-default-resource-limits)

add_library(raylib_backends_android_example SHARED
    main.c
    "${RAYLIB_BACKENDS_DIR}/examples/cube_scene.c"
)
target_include_directories(raylib_backends_android_example PRIVATE "${RAYLIB_BACKENDS_DIR}/examples")
target_link_libraries(raylib_backends_android_example PRIVATE raylib android log)
