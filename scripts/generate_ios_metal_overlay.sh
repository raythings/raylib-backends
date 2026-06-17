#!/bin/sh
set -eu

if [ "$#" -ne 3 ]; then
  echo "usage: $0 <raylib-root> <output-file> <raylib-backends-root>" >&2
  exit 1
fi

RAYLIB_DIR="$1"
OUT_FILE="$2"
BACKENDS_DIR="$3"

mkdir -p "$(dirname "$OUT_FILE")"

python3 - "$RAYLIB_DIR/src/rcore.c" "$OUT_FILE" "$BACKENDS_DIR" <<'PY'
import pathlib
import sys

src = pathlib.Path(sys.argv[1]).read_text()
out = pathlib.Path(sys.argv[2])
backends = sys.argv[3]
raylib_src = str(pathlib.Path(sys.argv[1]).parent)

src = src.replace(
    '#include "config.h"                 // Defines module configuration flags',
    f'#include "{raylib_src}/config.h"                 // Defines module configuration flags'
)

src = src.replace(
    '#define RLGL_IMPLEMENTATION\n#include "rlgl.h"',
    f'''#if defined(RAYLIB_USE_RLMT)\n    #include "{backends}/backends/rlmt/rlmt.h"\n#else\n    #define RLGL_IMPLEMENTATION\n    #include "rlgl.h"\n#endif'''
)
if 'rlBeginFrame();' not in src:
    src = src.replace(
        'CORE.Time.previous = CORE.Time.current;\n\n    rlLoadIdentity();',
        'CORE.Time.previous = CORE.Time.current;\n\n#if defined(RAYLIB_USE_RLVK) || defined(RAYLIB_USE_RLMT)\n    rlBeginFrame();\n#endif\n\n    rlLoadIdentity();',
    )
else:
    src = src.replace(
        '#if defined(RAYLIB_USE_RLVK)\n    rlBeginFrame();\n#endif',
        '#if defined(RAYLIB_USE_RLVK) || defined(RAYLIB_USE_RLMT)\n    rlBeginFrame();\n#endif',
    )

if 'rlEndFrame();' not in src:
    src = src.replace(
        '#if SUPPORT_AUTOMATION_EVENTS\n    if (automationEventRecording) RecordAutomationEvent();    // Event recording\n#endif\n\n#if !SUPPORT_CUSTOM_FRAME_CONTROL',
        '#if SUPPORT_AUTOMATION_EVENTS\n    if (automationEventRecording) RecordAutomationEvent();    // Event recording\n#endif\n\n#if defined(RAYLIB_USE_RLVK) || defined(RAYLIB_USE_RLMT)\n    rlEndFrame();\n#endif\n\n#if !SUPPORT_CUSTOM_FRAME_CONTROL',
    )
else:
    src = src.replace(
        '#if defined(RAYLIB_USE_RLVK)\n    rlEndFrame();\n#endif',
        '#if defined(RAYLIB_USE_RLVK) || defined(RAYLIB_USE_RLMT)\n    rlEndFrame();\n#endif',
    )
src = src.replace(
    '#include "platforms/rcore_desktop_glfw.c"',
    f'#include "{backends}/backends/rlmt/platforms/rcore_ios_metal.c"'
)
src = src.replace(
    '// NOTE: PLATFORM_DESKTOP defaults to GLFW backend\n#if defined(PLATFORM_DESKTOP)\n    #define PLATFORM_DESKTOP_GLFW\n#endif',
    '// NOTE: iOS uses the external Metal overlay backend\n#if defined(PLATFORM_IOS_METAL)\n    #define PLATFORM_DESKTOP_GLFW\n#endif'
)
src = src.replace(
    '#elif defined(PLATFORM_ANDROID)\n    #include "platforms/rcore_android.c"',
    f'#elif defined(PLATFORM_IOS_METAL)\n    #include "{backends}/backends/rlmt/platforms/rcore_ios_metal.c"\n#elif defined(PLATFORM_ANDROID)\n    #include "platforms/rcore_android.c"'
)
src = src.replace(
    '#elif defined(PLATFORM_ANDROID)\n    TRACELOG(LOG_INFO, "Platform backend: ANDROID");',
    '#elif defined(PLATFORM_IOS_METAL)\n    TRACELOG(LOG_INFO, "Platform backend: IOS (Metal external layer)");\n#elif defined(PLATFORM_ANDROID)\n    TRACELOG(LOG_INFO, "Platform backend: ANDROID");'
)
out.write_text(src)
PY
