#ifndef RCORE_ANDROID_SURFACE_H
#define RCORE_ANDROID_SURFACE_H

#include <stdint.h>

// Host API for the PLATFORM_ANDROID_SURFACE raylib backend. Consumers call
// these to feed raylib an externally-owned ANativeWindow + touch events,
// instead of raylib owning the Activity via native_app_glue.
//
// Multi-surface: after the first RcoreAndroidSurface_CreateWindow() /
// InitWindow() boot, the host can call RcoreAndroidSurface_CreateWindow()
// again to allocate additional EGL surfaces (one per navigation screen).
// All surfaces share a single EGL context — switching is just an
// eglMakeCurrent + draw + eglSwapBuffers. The currently-bound surface is
// the one EGL draws into; the host picks the order each frame.

#ifdef __cplusplus
extern "C" {
#endif

// Touch action codes for RcoreAndroidSurface_PushTouch().
#define RCORE_AS_TOUCH_DOWN 0
#define RCORE_AS_TOUCH_UP   1
#define RCORE_AS_TOUCH_MOVE 2

// Set the native window (from ANativeWindow_fromSurface) BEFORE InitWindow().
// Used for the boot/first surface.
void RcoreAndroidSurface_SetWindow(void *nativeWindow);
// Rebind a new ANativeWindow to the boot surface after background/resume.
// RLVK only: recreates VkSurface+swapchain, device and textures survive.
// Returns window id (1) on success, 0 on failure.
int RcoreAndroidSurface_ResumeWindow(void *nativeWindow);
// Optional display density (dpi/160). Defaults to 1.0.
void RcoreAndroidSurface_SetDensity(float density);
// Optional storage base path (Activity internalDataPath).
void RcoreAndroidSurface_SetDataPath(const char *path);
// Whether a window is currently set.
int  RcoreAndroidSurface_HasWindow(void);
// Enqueue a touch event (called from the UI thread).
void RcoreAndroidSurface_PushTouch(int action, int id, float x, float y);
// True while queued touch events await draining — the host must keep
// scheduling frames until this clears (the per-frame drain stops at the
// first DOWN/UP edge, so a buffered gesture replays across frames).
int  RcoreAndroidSurface_HasPendingTouch(void);

// Multi-surface
// Returns a positive surface id on success, 0 on failure. The boot surface
// (created via InitWindow) is also represented; RcoreAndroidSurface_GetCurrentId()
// returns its id. After CreateWindow, call BindWindow(id) to draw into it.
int  RcoreAndroidSurface_CreateWindow(void *nativeWindow);
// Binds an EGL surface as current + updates CORE.Window.{screen,display,
// render,currentFbo} from the new window's size. Returns 0 on success.
int  RcoreAndroidSurface_BindWindow(int id);
// Updates the cached native-window size after SurfaceHolder.surfaceChanged().
// Returns 1 when the window id exists, 0 otherwise.
int  RcoreAndroidSurface_ResizeWindow(int id, int width, int height);
// Swaps the currently bound surface.
void RcoreAndroidSurface_SwapWindow(void);
// Destroys an EGL surface + releases the ANativeWindow. Idempotent.
void RcoreAndroidSurface_DestroyWindow(int id);
// Returns the id of the currently bound surface, or 0 if none.
int  RcoreAndroidSurface_GetCurrentId(void);
// Count of live surfaces.
int  RcoreAndroidSurface_GetWindowCount(void);
// Size of a surface in pixels. Returns 0,0 for unknown id.
void RcoreAndroidSurface_GetWindowSize(int id, int *w, int *h);

// Monotonic clock in nanoseconds (CLOCK_MONOTONIC). Used by the JNI layer to
// debounce per-frame work across multiple render threads.
int64_t RcoreAndroidSurface_NowNanos(void);

#ifdef __cplusplus
}
#endif

#endif // RCORE_ANDROID_SURFACE_H
