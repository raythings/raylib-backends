cmake_minimum_required(VERSION 3.20)
project(raylib_backends_ios_metal LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "13.0")

if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    message(FATAL_ERROR "Configure this example with -DCMAKE_SYSTEM_NAME=iOS")
endif()

if(NOT RAYLIB_DIR)
    message(FATAL_ERROR "RAYLIB_DIR must point at an upstream raylib checkout")
endif()

set(RAYLIB_BACKENDS_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "raylib-backends root")

include("${RAYLIB_BACKENDS_DIR}/cmake/RaylibBackend.cmake")

raylib_backend_prepare_ios_metal_overlay(
    OUT_VAR RAYLIB_RCORE_SRC
    RAYLIB_DIR "${RAYLIB_DIR}"
    RAYLIB_BACKENDS_DIR "${RAYLIB_BACKENDS_DIR}"
)

add_library(raylib_ios_metal STATIC)
target_sources(raylib_ios_metal PRIVATE
    "${RAYLIB_RCORE_SRC}"
    "${RAYLIB_DIR}/src/rshapes.c"
    "${RAYLIB_DIR}/src/rtextures.c"
    "${RAYLIB_DIR}/src/rtext.c"
    "${RAYLIB_DIR}/src/rmodels.c"
)
target_include_directories(raylib_ios_metal PUBLIC "${RAYLIB_DIR}/src")
set_target_properties(raylib_ios_metal PROPERTIES
    XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "13.0"
)
raylib_backend_attach(
    TARGET raylib_ios_metal
    BACKEND METAL
    RAYLIB_BACKENDS_DIR "${RAYLIB_BACKENDS_DIR}"
)

add_executable(raylib_backends_ios_metal MACOSX_BUNDLE main.mm)
target_include_directories(raylib_backends_ios_metal PRIVATE "${CMAKE_CURRENT_LIST_DIR}/..")
target_sources(raylib_backends_ios_metal PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../cube_scene.c")
target_link_libraries(raylib_backends_ios_metal PRIVATE raylib_ios_metal)
set_target_properties(raylib_backends_ios_metal PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_LIST_DIR}/Info.plist"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.raylib.backends.iosmetal"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "13.0"
)
