include_guard(GLOBAL)

function(raylib_backend_apply_external_rlgl_overlay CONTENTS_VAR RAYLIB_BACKENDS_DIR)
    set(_contents "${${CONTENTS_VAR}}")

    string(REPLACE
        "#define RLGL_IMPLEMENTATION\n#include \"rlgl.h\""
        "#if defined(RAYLIB_USE_RLVK)\n    #include \"${RAYLIB_BACKENDS_DIR}/backends/rlvk/rlvk.h\"\n#elif defined(RAYLIB_USE_RLMT)\n    #include \"${RAYLIB_BACKENDS_DIR}/backends/rlmt/rlmt.h\"\n#else\n    #define RLGL_IMPLEMENTATION\n    #include \"rlgl.h\"\n#endif"
        _contents
        "${_contents}")

    string(FIND "${_contents}" "rlBeginFrame();" _has_begin_frame)
    if(_has_begin_frame EQUAL -1)
        string(REPLACE
            "CORE.Time.previous = CORE.Time.current;\n\n    rlLoadIdentity();"
            "CORE.Time.previous = CORE.Time.current;\n\n#if defined(RAYLIB_USE_RLVK) || defined(RAYLIB_USE_RLMT)\n    rlBeginFrame();\n#endif\n\n    rlLoadIdentity();"
            _contents
            "${_contents}")
    else()
        string(REPLACE
            "#if defined(RAYLIB_USE_RLVK)\n    rlBeginFrame();\n#endif"
            "#if defined(RAYLIB_USE_RLVK) || defined(RAYLIB_USE_RLMT)\n    rlBeginFrame();\n#endif"
            _contents
            "${_contents}")
    endif()

    string(FIND "${_contents}" "rlEndFrame();" _has_end_frame)
    if(_has_end_frame EQUAL -1)
        string(REPLACE
            "#if SUPPORT_AUTOMATION_EVENTS\n    if (automationEventRecording) RecordAutomationEvent();    // Event recording\n#endif\n\n#if !SUPPORT_CUSTOM_FRAME_CONTROL"
            "#if SUPPORT_AUTOMATION_EVENTS\n    if (automationEventRecording) RecordAutomationEvent();    // Event recording\n#endif\n\n#if defined(RAYLIB_USE_RLVK) || defined(RAYLIB_USE_RLMT)\n    rlEndFrame();\n#endif\n\n#if !SUPPORT_CUSTOM_FRAME_CONTROL"
            _contents
            "${_contents}")
    else()
        string(REPLACE
            "#if defined(RAYLIB_USE_RLVK)\n    rlEndFrame();\n#endif"
            "#if defined(RAYLIB_USE_RLVK) || defined(RAYLIB_USE_RLMT)\n    rlEndFrame();\n#endif"
            _contents
            "${_contents}")
    endif()

    set(${CONTENTS_VAR} "${_contents}" PARENT_SCOPE)
endfunction()

function(raylib_backend_prepare_rcore_overlay)
    set(options)
    set(oneValueArgs OUT_VAR RAYLIB_DIR RAYLIB_BACKENDS_DIR)
    set(multiValueArgs)
    cmake_parse_arguments(RBPO "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT RBPO_OUT_VAR)
        message(FATAL_ERROR "raylib_backend_prepare_rcore_overlay: OUT_VAR is required")
    endif()
    if(NOT RBPO_RAYLIB_DIR)
        message(FATAL_ERROR "raylib_backend_prepare_rcore_overlay: RAYLIB_DIR is required")
    endif()
    if(NOT RBPO_RAYLIB_BACKENDS_DIR)
        set(RBPO_RAYLIB_BACKENDS_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
    endif()

    set(_overlay_dir "${CMAKE_CURRENT_BINARY_DIR}/raylib_backend_overlay")
    file(MAKE_DIRECTORY "${_overlay_dir}")
    set(_overlay_src "${_overlay_dir}/rcore_overlay.c")

    file(READ "${RBPO_RAYLIB_DIR}/src/rcore.c" _rcore_contents)
    raylib_backend_apply_external_rlgl_overlay(_rcore_contents "${RBPO_RAYLIB_BACKENDS_DIR}")
    string(REPLACE
        "#include \"platforms/rcore_desktop_glfw.c\""
        "#include \"${RBPO_RAYLIB_BACKENDS_DIR}/backends/rlmt/platforms/rcore_desktop_glfw.c\""
        _rcore_contents
        "${_rcore_contents}")
    file(WRITE "${_overlay_src}" "${_rcore_contents}")

    set(${RBPO_OUT_VAR} "${_overlay_src}" PARENT_SCOPE)
endfunction()

function(raylib_backend_prepare_android_surface_overlay)
    set(options)
    set(oneValueArgs OUT_VAR RAYLIB_DIR RAYLIB_BACKENDS_DIR)
    set(multiValueArgs)
    cmake_parse_arguments(RBASO "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT RBASO_OUT_VAR)
        message(FATAL_ERROR "raylib_backend_prepare_android_surface_overlay: OUT_VAR is required")
    endif()
    if(NOT RBASO_RAYLIB_DIR)
        message(FATAL_ERROR "raylib_backend_prepare_android_surface_overlay: RAYLIB_DIR is required")
    endif()
    if(NOT RBASO_RAYLIB_BACKENDS_DIR)
        set(RBASO_RAYLIB_BACKENDS_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
    endif()

    set(_overlay_dir "${CMAKE_CURRENT_BINARY_DIR}/raylib_backend_overlay")
    file(MAKE_DIRECTORY "${_overlay_dir}")
    set(_overlay_src "${_overlay_dir}/rcore_android_surface_overlay.c")

    file(READ "${RBASO_RAYLIB_DIR}/src/rcore.c" _rcore_contents)
    raylib_backend_apply_external_rlgl_overlay(_rcore_contents "${RBASO_RAYLIB_BACKENDS_DIR}")
    string(FIND "${_rcore_contents}" "PLATFORM_ANDROID_SURFACE" _has_android_surface)
    string(REPLACE
        "#if defined(PLATFORM_ANDROID_SURFACE)\n    // Render into an externally-provided ANativeWindow (SurfaceView),\n    // decoupled from native_app_glue / the Activity lifecycle.\n    #include \"platforms/rcore_android_surface.c\""
        "#if defined(PLATFORM_ANDROID_SURFACE)\n    #include \"${RBASO_RAYLIB_BACKENDS_DIR}/backends/rlvk/platforms/rcore_android_surface.c\""
        _rcore_contents
        "${_rcore_contents}")
    if(_has_android_surface EQUAL -1)
        string(REPLACE
            "#elif defined(PLATFORM_ANDROID)\n    #include \"platforms/rcore_android.c\""
            "#elif defined(PLATFORM_ANDROID_SURFACE)\n    #include \"${RBASO_RAYLIB_BACKENDS_DIR}/backends/rlvk/platforms/rcore_android_surface.c\"\n#elif defined(PLATFORM_ANDROID)\n    #include \"platforms/rcore_android.c\""
            _rcore_contents
            "${_rcore_contents}")
    endif()
    string(REPLACE
        "#elif defined(PLATFORM_ANDROID)\n    TRACELOG(LOG_INFO, \"Platform backend: ANDROID\");"
        "#elif defined(PLATFORM_ANDROID_SURFACE)\n    TRACELOG(LOG_INFO, \"Platform backend: ANDROID (external surface)\");\n#elif defined(PLATFORM_ANDROID)\n    TRACELOG(LOG_INFO, \"Platform backend: ANDROID\");"
        _rcore_contents
        "${_rcore_contents}")
    string(REPLACE
        "#if defined(PLATFORM_ANDROID)\n    switch (logType)"
        "#if defined(PLATFORM_ANDROID) || defined(PLATFORM_ANDROID_SURFACE)\n    switch (logType)"
        _rcore_contents
        "${_rcore_contents}")
    file(WRITE "${_overlay_src}" "${_rcore_contents}")

    set(${RBASO_OUT_VAR} "${_overlay_src}" PARENT_SCOPE)
endfunction()

function(raylib_backend_prepare_ios_metal_overlay)
    set(options)
    set(oneValueArgs OUT_VAR RAYLIB_DIR RAYLIB_BACKENDS_DIR)
    set(multiValueArgs)
    cmake_parse_arguments(RBIMO "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT RBIMO_OUT_VAR)
        message(FATAL_ERROR "raylib_backend_prepare_ios_metal_overlay: OUT_VAR is required")
    endif()
    if(NOT RBIMO_RAYLIB_DIR)
        message(FATAL_ERROR "raylib_backend_prepare_ios_metal_overlay: RAYLIB_DIR is required")
    endif()
    if(NOT RBIMO_RAYLIB_BACKENDS_DIR)
        set(RBIMO_RAYLIB_BACKENDS_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
    endif()

    set(_overlay_dir "${CMAKE_CURRENT_BINARY_DIR}/raylib_backend_overlay")
    file(MAKE_DIRECTORY "${_overlay_dir}")
    set(_overlay_src "${_overlay_dir}/rcore_ios_metal_overlay.c")

    file(READ "${RBIMO_RAYLIB_DIR}/src/rcore.c" _rcore_contents)
    raylib_backend_apply_external_rlgl_overlay(_rcore_contents "${RBIMO_RAYLIB_BACKENDS_DIR}")
    string(REPLACE
        "#elif defined(PLATFORM_ANDROID)\n    #include \"platforms/rcore_android.c\""
        "#elif defined(PLATFORM_IOS_METAL)\n    #include \"${RBIMO_RAYLIB_BACKENDS_DIR}/backends/rlmt/platforms/rcore_ios_metal.c\"\n#elif defined(PLATFORM_ANDROID)\n    #include \"platforms/rcore_android.c\""
        _rcore_contents
        "${_rcore_contents}")
    string(REPLACE
        "#elif defined(PLATFORM_ANDROID)\n    TRACELOG(LOG_INFO, \"Platform backend: ANDROID\");"
        "#elif defined(PLATFORM_IOS_METAL)\n    TRACELOG(LOG_INFO, \"Platform backend: IOS (Metal external layer)\");\n#elif defined(PLATFORM_ANDROID)\n    TRACELOG(LOG_INFO, \"Platform backend: ANDROID\");"
        _rcore_contents
        "${_rcore_contents}")
    file(WRITE "${_overlay_src}" "${_rcore_contents}")

    set(${RBIMO_OUT_VAR} "${_overlay_src}" PARENT_SCOPE)
endfunction()

function(raylib_backend_attach)
    set(options)
    set(oneValueArgs TARGET BACKEND RAYLIB_BACKENDS_DIR)
    set(multiValueArgs)
    cmake_parse_arguments(RBA "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT RBA_TARGET)
        message(FATAL_ERROR "raylib_backend_attach: TARGET is required")
    endif()
    if(NOT TARGET ${RBA_TARGET})
        message(FATAL_ERROR "raylib_backend_attach: target '${RBA_TARGET}' does not exist")
    endif()
    if(NOT RBA_BACKEND)
        message(FATAL_ERROR "raylib_backend_attach: BACKEND is required")
    endif()
    if(NOT RBA_RAYLIB_BACKENDS_DIR)
        set(RBA_RAYLIB_BACKENDS_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
    endif()

    get_target_property(_attached_backend ${RBA_TARGET} RAYLIB_BACKEND_NAME)
    if(_attached_backend AND NOT _attached_backend STREQUAL "NOTFOUND")
        message(FATAL_ERROR
            "raylib_backend_attach: target '${RBA_TARGET}' already has backend '${_attached_backend}' attached")
    endif()

    string(TOUPPER "${RBA_BACKEND}" _backend)
    set(_root "${RBA_RAYLIB_BACKENDS_DIR}")

    if(_backend STREQUAL "VULKAN")
        target_compile_definitions(${RBA_TARGET} PUBLIC RAYLIB_USE_RLVK RAYLIB_RLGL_EXTENDED GRAPHICS_API_OPENGL_33)
        target_include_directories(${RBA_TARGET} BEFORE PUBLIC
            "${_root}/backends/rlvk"
            "${_root}/backends/rlvk/platforms"
            "${_root}/third_party/spirv_reflect"
        )
        target_sources(${RBA_TARGET} PRIVATE
            "${_root}/backends/rlvk/glad_vulkan_impl.c"
            "${_root}/backends/rlvk/rlvk_reflect.c"
            "${_root}/third_party/spirv_reflect/spirv_reflect.c"
        )
        target_link_libraries(${RBA_TARGET} PUBLIC vulkan)
        set_target_properties(${RBA_TARGET} PROPERTIES RAYLIB_BACKEND_NAME "VULKAN")
    elseif(_backend STREQUAL "METAL")
        if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
            target_compile_definitions(${RBA_TARGET} PUBLIC RAYLIB_USE_RLMT RAYLIB_RLGL_EXTENDED GRAPHICS_API_OPENGL_33 PLATFORM_IOS_METAL)
        else()
            target_compile_definitions(${RBA_TARGET} PUBLIC RAYLIB_USE_RLMT RAYLIB_RLGL_EXTENDED GRAPHICS_API_OPENGL_33 PLATFORM_DESKTOP)
        endif()
        target_include_directories(${RBA_TARGET} BEFORE PUBLIC "${_root}/backends/rlmt")
        target_sources(${RBA_TARGET} PRIVATE "${_root}/backends/rlmt/rlmt.mm")
        if(APPLE)
            if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
                target_link_libraries(${RBA_TARGET} PUBLIC "-framework Metal" "-framework QuartzCore" "-framework UIKit" "-framework Foundation" "-framework CoreVideo")
            else()
                target_link_libraries(${RBA_TARGET} PUBLIC "-framework Metal" "-framework QuartzCore" "-framework Cocoa" "-framework CoreVideo")
            endif()
        endif()
        set_target_properties(${RBA_TARGET} PROPERTIES RAYLIB_BACKEND_NAME "METAL")
    else()
        message(FATAL_ERROR "raylib_backend_attach: unsupported BACKEND '${RBA_BACKEND}'")
    endif()
endfunction()
