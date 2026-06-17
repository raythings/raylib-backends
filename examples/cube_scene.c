#include "raylib.h"

#include "cube_scene.h"

void RaylibBackendsDrawCubeScene(void)
{
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
