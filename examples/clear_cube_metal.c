#include "raylib.h"

static void draw_demo_cube(void) {
    Camera3D camera = { 0 };
    camera.position = (Vector3){ 4.0f, 3.0f, 4.0f };
    camera.target = (Vector3){ 0.0f, 0.75f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    BeginMode3D(camera);
    DrawCube((Vector3){ 0.0f, 0.75f, 0.0f }, 1.5f, 1.5f, 1.5f, (Color){ 105, 164, 255, 255 });
    DrawCubeWires((Vector3){ 0.0f, 0.75f, 0.0f }, 1.5f, 1.5f, 1.5f, RAYWHITE);
    DrawGrid(12, 1.0f);
    EndMode3D();
}

int main(void) {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(960, 540, "raylib-backends cube smoke test");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground((Color){ 18, 18, 24, 255 });
        draw_demo_cube();
        DrawText("raylib-backends cube smoke test", 24, 24, 24, RAYWHITE);
        DrawText("backend attachment is working", 24, 60, 20, LIGHTGRAY);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
