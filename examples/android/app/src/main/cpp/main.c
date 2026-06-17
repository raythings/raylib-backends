#include "raylib.h"
#include <stdint.h>
#include "platforms/rcore_android_surface.h"
#include "cube_scene.h"

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>
#include <pthread.h>
#include <stdatomic.h>

static pthread_t render_thread;
static atomic_bool render_running = false;
static atomic_bool render_started = false;
static int surface_width = 1280;
static int surface_height = 720;

static void draw_cube(void) {
    BeginDrawing();
    ClearBackground((Color){ 14, 16, 22, 255 });
    RaylibBackendsDrawCubeScene();
    DrawText("raylib-backends android cube smoke test", 24, 24, 24, RAYWHITE);
    EndDrawing();
}

static void *render_main(void *unused) {
    (void)unused;
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(surface_width, surface_height, "raylib-backends android cube smoke test");
    SetTargetFPS(60);

    while (atomic_load(&render_running) && !WindowShouldClose()) {
        draw_cube();
    }

    CloseWindow();
    atomic_store(&render_started, false);
    return NULL;
}

JNIEXPORT void JNICALL
Java_com_raylib_backends_example_MainActivity_nativeSurfaceCreated(JNIEnv *env, jclass clazz, jobject surface) {
    (void)clazz;
    if (atomic_load(&render_running)) return;

    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    if (!window) return;

    RcoreAndroidSurface_SetWindow(window);
    atomic_store(&render_running, true);
    if (pthread_create(&render_thread, NULL, render_main, NULL) == 0) {
        atomic_store(&render_started, true);
    } else {
        atomic_store(&render_running, false);
        ANativeWindow_release(window);
    }
}

JNIEXPORT void JNICALL
Java_com_raylib_backends_example_MainActivity_nativeSurfaceChanged(JNIEnv *env, jclass clazz, jint width, jint height) {
    (void)env;
    (void)clazz;
    surface_width = width > 0 ? width : surface_width;
    surface_height = height > 0 ? height : surface_height;
}

JNIEXPORT void JNICALL
Java_com_raylib_backends_example_MainActivity_nativeSurfaceDestroyed(JNIEnv *env, jclass clazz) {
    (void)env;
    (void)clazz;
    if (!atomic_load(&render_running)) return;

    atomic_store(&render_running, false);
    if (atomic_load(&render_started)) {
        pthread_join(render_thread, NULL);
    }
}
