#include "raylib.h"

#include "cube_scene.h"

int main(void) {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(960, 540, "raylib-backends cube smoke test");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground((Color){ 18, 18, 24, 255 });
        RaylibBackendsDrawCubeScene();
        DrawText("raylib-backends cube smoke test", 24, 24, 24, RAYWHITE);
        DrawText("backend attachment is working", 24, 60, 20, LIGHTGRAY);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
