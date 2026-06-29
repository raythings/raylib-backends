cmake_minimum_required(VERSION 3.20)

if(NOT RAYLIB_DIR)
    message(FATAL_ERROR "raylib-backends examples require RAYLIB_DIR to point at an upstream raylib checkout")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/RaylibBackend.cmake")

# GLFW is only needed by the native (Vulkan/Metal) desktop examples; the WebGPU/web
# example runs against the browser canvas and never links GLFW.
if(NOT EMSCRIPTEN)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(${RAYLIB_DIR}/src/external/glfw ${CMAKE_CURRENT_BINARY_DIR}/glfw EXCLUDE_FROM_ALL)
endif()

function(raylib_backends_add_smoke_example target_name backend)
    set(_raylib_rcore "${RAYLIB_DIR}/src/rcore.c")
    if((APPLE AND "${backend}" STREQUAL "METAL") OR "${backend}" STREQUAL "WEBGPU")
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

    # The standalone WebGPU example uses a blocking while(!WindowShouldClose()) loop and
    # -sASYNCIFY; SwapScreenBuffer must yield to the browser. (Engine hosts drive their
    # own rAF loop and omit this — see backends/rlwg/platforms/rcore_web.c.)
    if("${backend}" STREQUAL "WEBGPU")
        target_compile_definitions(${target_name}_raylib PRIVATE RLWG_ASYNCIFY_YIELD)
    endif()

    add_executable(${target_name} clear_cube.c cube_scene.c)
    if("${backend}" STREQUAL "WEBGPU")
        target_link_libraries(${target_name} PRIVATE ${target_name}_raylib)
        set_target_properties(${target_name} PROPERTIES
            SUFFIX ".html"
            LINK_FLAGS "--shell-file ${CMAKE_CURRENT_LIST_DIR}/shell.html -sASYNCIFY -sALLOW_MEMORY_GROWTH=1 --use-port=emdawnwebgpu")
    else()
        target_link_libraries(${target_name} PRIVATE ${target_name}_raylib glfw)
    endif()
endfunction()

if(EMSCRIPTEN)
    raylib_backends_add_smoke_example(clear_cube_webgpu WEBGPU)
elseif(APPLE)
    raylib_backends_add_smoke_example(clear_cube_metal METAL)
elseif(UNIX)
    raylib_backends_add_smoke_example(clear_cube_vulkan VULKAN)
endif()
