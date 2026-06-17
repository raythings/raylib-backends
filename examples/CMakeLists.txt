cmake_minimum_required(VERSION 3.20)

if(NOT RAYLIB_DIR)
    message(FATAL_ERROR "raylib-backends examples require RAYLIB_DIR to point at an upstream raylib checkout")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/RaylibBackend.cmake")

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${RAYLIB_DIR}/src/external/glfw ${CMAKE_CURRENT_BINARY_DIR}/glfw EXCLUDE_FROM_ALL)

function(raylib_backends_add_smoke_example target_name backend)
    set(_raylib_rcore "${RAYLIB_DIR}/src/rcore.c")
    if(APPLE AND "${backend}" STREQUAL "METAL")
        raylib_backend_prepare_rcore_overlay(
            OUT_VAR _raylib_rcore
            RAYLIB_DIR "${RAYLIB_DIR}"
            RAYLIB_BACKENDS_DIR "${CMAKE_CURRENT_LIST_DIR}/.."
        )
    endif()

    add_library(${target_name}_raylib STATIC)
    target_sources(${target_name}_raylib PRIVATE
        "${_raylib_rcore}"
        ${RAYLIB_DIR}/src/rshapes.c
        ${RAYLIB_DIR}/src/rtextures.c
        ${RAYLIB_DIR}/src/rtext.c
        ${RAYLIB_DIR}/src/rmodels.c
        ${RAYLIB_DIR}/src/raudio.c
    )
    target_include_directories(${target_name}_raylib PUBLIC
        ${RAYLIB_DIR}/src
        ${RAYLIB_DIR}/src/platforms
        ${RAYLIB_DIR}/src/external/glfw/include
    )

    raylib_backend_attach(
        TARGET ${target_name}_raylib
        BACKEND ${backend}
        RAYLIB_BACKENDS_DIR ${CMAKE_CURRENT_LIST_DIR}/..
    )

    add_executable(${target_name} clear_cube.c cube_scene.c)
    target_link_libraries(${target_name} PRIVATE ${target_name}_raylib glfw)
endfunction()

if(APPLE)
    raylib_backends_add_smoke_example(clear_cube_metal METAL)
endif()

if(UNIX AND NOT APPLE)
    raylib_backends_add_smoke_example(clear_cube_vulkan VULKAN)
endif()
