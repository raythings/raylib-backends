cmake_minimum_required(VERSION 3.20)
project(raylib_backends LANGUAGES C CXX)

option(RAYLIB_BACKENDS_BUILD_EXAMPLES "Build backend smoke-test examples" ON)
set(RAYLIB_DIR "" CACHE PATH "Path to an upstream raylib checkout for smoke-test examples")

add_library(raylib_backends INTERFACE)
target_include_directories(raylib_backends INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/rlvk
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/rlmt
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/spirv_reflect
)

if(RAYLIB_BACKENDS_BUILD_EXAMPLES AND EXISTS "${RAYLIB_DIR}/src/raylib.h")
    add_subdirectory(examples)
endif()
