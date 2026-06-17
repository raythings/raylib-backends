#include "raylib.h"

#include "cube_scene.h"

#ifdef __ANDROID__
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "raylib-backends", __VA_ARGS__)
#endif

void android_main(void) {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "raylib-backends android cube smoke test");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground((Color){ 14, 16, 22, 255 });
        RaylibBackendsDrawCubeScene();
        DrawText("raylib-backends android cube smoke test", 24, 24, 24, RAYWHITE);
        EndDrawing();
    }

    CloseWindow();
}
