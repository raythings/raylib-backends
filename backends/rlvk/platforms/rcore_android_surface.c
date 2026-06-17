/**********************************************************************************************
*
*   rcore_<platform> template - Functions to manage window, graphics device and inputs
*
*   PLATFORM: <PLATFORM>
*       - TODO: Define the target platform for the core
*
*   LIMITATIONS:
*       - Limitation 01
*       - Limitation 02
*
*   POSSIBLE IMPROVEMENTS:
*       - Improvement 01
*       - Improvement 02
*
*   ADDITIONAL NOTES:
*       - TRACELOG() function is located in raylib [utils] module
*
*   CONFIGURATION:
*       #define RCORE_PLATFORM_CUSTOM_FLAG
*           Custom flag for rcore on target platform -not used-
*
*   DEPENDENCIES:
*       - <platform-specific SDK dependency>
*       - gestures: Gestures system for touch-ready devices (or simulated from mouse inputs)
*
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2013-2026 Ramon Santamaria (@raysan5) and contributors
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/

// PLATFORM_ANDROID_SURFACE: render raylib into an externally-provided
// ANativeWindow (from a SurfaceView's Surface), WITHOUT owning the Activity or
// using native_app_glue. The host (Rayact JNI layer) creates the engine, sets
// the window via RcoreAndroidSurface_SetWindow() before InitWindow(), pumps
// touch via RcoreAndroidSurface_PushTouch(), and drives frames itself. This is
// what lets the process-level JS engine coordinate multiple surfaces/screens.
#include <KHR/khrplatform.h>            // Required first: defines KHRONOS_APIENTRY used by EGL/GLES headers
#include <EGL/egl.h>                    // Native platform windowing system interface
#include <GLES2/gl2.h>                  // GLES base (also ensures khrplatform ordering)
#include <android/native_window.h>
#include <android/log.h>
#include <time.h>
#include <string.h>

#include "rcore_android_surface.h"

// Sized to survive a janky first frame (Vulkan pipeline warm-up can stall
// ~1s while a 120Hz touch stream queues up). Overflow drops the newest
// event — including a gesture-ending UP — leaving input stuck "pressed".
#define RCORE_AS_TOUCH_QUEUE 1024

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
#define RCORE_AS_MAX_WINDOWS 16

typedef struct {
    int id;
    ANativeWindow *window;
#if !defined(RAYLIB_USE_RLVK)
    EGLSurface surface;
#endif
    int width, height;
} WindowSlot;

typedef struct {
    EGLDisplay device;              // EGL display (one per process). EGL_NO_DISPLAY when RLVK is in use.
    EGLContext context;             // EGL context (shared by all windows)
    EGLConfig config;               // EGL framebuffer config
    float density;                  // Display density (dpi/160), 1.0 if unknown
    const char *internalDataPath;   // Storage base path (from JNI)

    // EGL/ANativeWindow surfaces — index 0 is the boot surface (created at
    // InitWindow time). Additional ones are added via CreateWindow for
    // navigation screens. All share the EGL context above.
    WindowSlot windows[RCORE_AS_MAX_WINDOWS];
    int windowCount;
    int currentWindow;              // index of the currently bound surface, -1 if none

    // Single-producer (UI thread) / single-consumer (render thread) touch queue.
    struct { int action; int id; float x; float y; } touchQueue[RCORE_AS_TOUCH_QUEUE];
    volatile int touchHead;
    volatile int touchTail;
} PlatformData;

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
extern CoreData CORE;                   // Global CORE state context

static PlatformData platform = { 0 };   // Platform specific data
static int g_nextWindowId = 1;          // 0 reserved for "no window"

//----------------------------------------------------------------------------------
// External host API (declared in rcore_android_surface.h, called from JNI)
//----------------------------------------------------------------------------------
void RcoreAndroidSurface_SetWindow(void *nativeWindow) { platform.windows[0].window = (ANativeWindow *)nativeWindow; }
void RcoreAndroidSurface_SetDensity(float density) { platform.density = density; }
void RcoreAndroidSurface_SetDataPath(const char *path) { platform.internalDataPath = path; }
int  RcoreAndroidSurface_HasWindow(void) { return platform.windows[0].window != NULL; }

// True while queued touch events are waiting to be drained. The host's frame
// scheduler must keep pumping frames while this holds: PollInputEvents stops
// each frame's drain at the first DOWN/UP edge, so a buffered gesture needs
// several frames to replay even after the finger has lifted.
int RcoreAndroidSurface_HasPendingTouch(void)
{
    return platform.touchTail != platform.touchHead;
}

void RcoreAndroidSurface_PushTouch(int action, int id, float x, float y)
{
    int next = (platform.touchHead + 1) % RCORE_AS_TOUCH_QUEUE;
    if (next == platform.touchTail) return;   // queue full — drop oldest-pressure event
    platform.touchQueue[platform.touchHead].action = action;
    platform.touchQueue[platform.touchHead].id = id;
    platform.touchQueue[platform.touchHead].x = x;
    platform.touchQueue[platform.touchHead].y = y;
    platform.touchHead = next;
}

//----------------------------------------------------------------------------------
// Module Internal Functions Declaration
//----------------------------------------------------------------------------------
int InitPlatform(void);          // Initialize platform (graphics, inputs and more)
bool InitGraphicsDevice(void);   // Initialize graphics device
#if defined(RAYLIB_USE_RLVK)
void rlvkSetNativeWindow(void *nativeWindow);
bool rlvkRecreateSurface(void);
#endif

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
// NOTE: Functions declaration is provided by raylib.h

//----------------------------------------------------------------------------------
// Module Functions Definition: Window and Graphics Device
//----------------------------------------------------------------------------------

// Check if application should close
bool WindowShouldClose(void)
{
    if (CORE.Window.ready) return CORE.Window.shouldClose;
    else return true;
}

// Toggle fullscreen mode
void ToggleFullscreen(void)
{
    TRACELOG(LOG_WARNING, "ToggleFullscreen() not available on target platform");
}

// Toggle borderless windowed mode
void ToggleBorderlessWindowed(void)
{
    TRACELOG(LOG_WARNING, "ToggleBorderlessWindowed() not available on target platform");
}

// Set window state: maximized, if resizable
void MaximizeWindow(void)
{
    TRACELOG(LOG_WARNING, "MaximizeWindow() not available on target platform");
}

// Set window state: minimized
void MinimizeWindow(void)
{
    TRACELOG(LOG_WARNING, "MinimizeWindow() not available on target platform");
}

// Restore window from being minimized/maximized
void RestoreWindow(void)
{
    TRACELOG(LOG_WARNING, "RestoreWindow() not available on target platform");
}

// Set window configuration state using flags
void SetWindowState(unsigned int flags)
{
    TRACELOG(LOG_WARNING, "SetWindowState() not available on target platform");
}

// Clear window configuration state flags
void ClearWindowState(unsigned int flags)
{
    TRACELOG(LOG_WARNING, "ClearWindowState() not available on target platform");
}

// Set icon for window
void SetWindowIcon(Image image)
{
    TRACELOG(LOG_WARNING, "SetWindowIcon() not available on target platform");
}

// Set icon for window
void SetWindowIcons(Image *images, int count)
{
    TRACELOG(LOG_WARNING, "SetWindowIcons() not available on target platform");
}

// Set title for window
void SetWindowTitle(const char *title)
{
    CORE.Window.title = title;
}

// Set window position on screen (windowed mode)
void SetWindowPosition(int x, int y)
{
    TRACELOG(LOG_WARNING, "SetWindowPosition() not available on target platform");
}

// Set monitor for the current window
void SetWindowMonitor(int monitor)
{
    TRACELOG(LOG_WARNING, "SetWindowMonitor() not available on target platform");
}

// Set window minimum dimensions (FLAG_WINDOW_RESIZABLE)
void SetWindowMinSize(int width, int height)
{
    CORE.Window.screenMin.width = width;
    CORE.Window.screenMin.height = height;
}

// Set window maximum dimensions (FLAG_WINDOW_RESIZABLE)
void SetWindowMaxSize(int width, int height)
{
    CORE.Window.screenMax.width = width;
    CORE.Window.screenMax.height = height;
}

// Set window dimensions
void SetWindowSize(int width, int height)
{
    TRACELOG(LOG_WARNING, "SetWindowSize() not available on target platform");
}

// Set window opacity, value opacity is between 0.0 and 1.0
void SetWindowOpacity(float opacity)
{
    TRACELOG(LOG_WARNING, "SetWindowOpacity() not available on target platform");
}

// Set window focused
void SetWindowFocused(void)
{
    TRACELOG(LOG_WARNING, "SetWindowFocused() not available on target platform");
}

// Get native window handle
void *GetWindowHandle(void)
{
    TRACELOG(LOG_WARNING, "GetWindowHandle() not implemented on target platform");
    return NULL;
}

// Get number of monitors
int GetMonitorCount(void)
{
    TRACELOG(LOG_WARNING, "GetMonitorCount() not implemented on target platform");
    return 1;
}

// Get current monitor where window is placed
int GetCurrentMonitor(void)
{
    TRACELOG(LOG_WARNING, "GetCurrentMonitor() not implemented on target platform");
    return 0;
}

// Get selected monitor position
Vector2 GetMonitorPosition(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorPosition() not implemented on target platform");
    return (Vector2){ 0, 0 };
}

// Get selected monitor width (currently used by monitor)
int GetMonitorWidth(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorWidth() not implemented on target platform");
    return 0;
}

// Get selected monitor height (currently used by monitor)
int GetMonitorHeight(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorHeight() not implemented on target platform");
    return 0;
}

// Get selected monitor physical width in millimetres
int GetMonitorPhysicalWidth(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorPhysicalWidth() not implemented on target platform");
    return 0;
}

// Get selected monitor physical height in millimetres
int GetMonitorPhysicalHeight(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorPhysicalHeight() not implemented on target platform");
    return 0;
}

// Get selected monitor refresh rate
int GetMonitorRefreshRate(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorRefreshRate() not implemented on target platform");
    return 0;
}

// Get the human-readable, UTF-8 encoded name of the selected monitor
const char *GetMonitorName(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorName() not implemented on target platform");
    return "";
}

// Get window position XY on monitor
Vector2 GetWindowPosition(void)
{
    TRACELOG(LOG_WARNING, "GetWindowPosition() not implemented on target platform");
    return (Vector2){ 0, 0 };
}

// Get window scale DPI factor for current monitor
Vector2 GetWindowScaleDPI(void)
{
    float d = (platform.density > 0.0f) ? platform.density : 1.0f;
    return (Vector2){ d, d };
}

// Set clipboard text content
void SetClipboardText(const char *text)
{
    TRACELOG(LOG_WARNING, "SetClipboardText() not implemented on target platform");
}

// Get clipboard text content
// NOTE: returned string is allocated and freed by GLFW
const char *GetClipboardText(void)
{
    TRACELOG(LOG_WARNING, "GetClipboardText() not implemented on target platform");
    return NULL;
}

// Get clipboard image
Image GetClipboardImage(void)
{
    Image image = { 0 };

    TRACELOG(LOG_WARNING, "GetClipboardImage() not implemented on target platform");

    return image;
}

// Show mouse cursor
void ShowCursor(void)
{
    CORE.Input.Mouse.cursorHidden = false;
}

// Hide mouse cursor
void HideCursor(void)
{
    CORE.Input.Mouse.cursorHidden = true;
}

// Enable cursor (unlock cursor)
void EnableCursor(void)
{
    // Set cursor position in the middle
    SetMousePosition(CORE.Window.screen.width/2, CORE.Window.screen.height/2);

    CORE.Input.Mouse.cursorHidden = false;
}

// Disable cursor (lock cursor)
void DisableCursor(void)
{
    // Set cursor position in the middle
    SetMousePosition(CORE.Window.screen.width/2, CORE.Window.screen.height/2);

    CORE.Input.Mouse.cursorHidden = true;
}

// Swap back buffer with front buffer (screen drawing)
void SwapScreenBuffer(void)
{
#if defined(RAYLIB_USE_RLVK)
    return;
#else
    int idx = platform.currentWindow;
    if (idx < 0 || idx >= RCORE_AS_MAX_WINDOWS) return;
    if (platform.windows[idx].surface != EGL_NO_SURFACE) {
        eglSwapBuffers(platform.device, platform.windows[idx].surface);
    }
#endif
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Misc
//----------------------------------------------------------------------------------

// Get elapsed time measure in seconds since InitTimer()
double GetTime(void)
{
    double time = 0.0;

    struct timespec ts = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &ts);
    unsigned long long nanoSeconds = (unsigned long long)ts.tv_sec*1000000000LLU + (unsigned long long)ts.tv_nsec;

    time = (double)(nanoSeconds - CORE.Time.base)*1e-9; // Elapsed time since InitTimer()

    return time;
}

// Open URL with default system browser (if available)
// NOTE: This function is only safe to use if you control the URL given.
// A user could craft a malicious string performing another action.
// Only call this function yourself not with user input or make sure to check the string yourself.
// Ref: https://github.com/raysan5/raylib/issues/686
void OpenURL(const char *url)
{
    // Security check to (partially) avoid malicious code on target platform
    if (strchr(url, '\'') != NULL) TRACELOG(LOG_WARNING, "SYSTEM: Provided URL could be potentially malicious, avoid [\'] character");
    else
    {
        // TODO: Load url using default browser
    }
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Inputs
//----------------------------------------------------------------------------------

// Set internal gamepad mappings
int SetGamepadMappings(const char *mappings)
{
    TRACELOG(LOG_WARNING, "SetGamepadMappings() not implemented on target platform");
    return 0;
}

// Set gamepad vibration
void SetGamepadVibration(int gamepad, float leftMotor, float rightMotor, float duration)
{
    TRACELOG(LOG_WARNING, "SetGamepadVibration() not implemented on target platform");
}

// Set mouse position XY
void SetMousePosition(int x, int y)
{
    CORE.Input.Mouse.currentPosition = (Vector2){ (float)x, (float)y };
    CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;
}

// Set mouse cursor
void SetMouseCursor(int cursor)
{
    (void)cursor;
}

// Get physical key name.
const char *GetKeyName(int key)
{
    TRACELOG(LOG_WARNING, "GetKeyName() not implemented on target platform");
    return "";
}

// Register all input events
void PollInputEvents(void)
{
#if SUPPORT_GESTURES_SYSTEM
    // NOTE: Gestures update must be called every frame to reset gestures correctly
    // because ProcessGestureEvent() is called on an event, not every frame
    UpdateGestures();
#endif

    // Reset keys/chars pressed registered
    CORE.Input.Keyboard.keyPressedQueueCount = 0;
    CORE.Input.Keyboard.charPressedQueueCount = 0;

    // Reset key repeats
    for (int i = 0; i < MAX_KEYBOARD_KEYS; i++) CORE.Input.Keyboard.keyRepeatInFrame[i] = 0;

    // Reset last gamepad button/axis registered state
    CORE.Input.Gamepad.lastButtonPressed = 0; // GAMEPAD_BUTTON_UNKNOWN
    //CORE.Input.Gamepad.axisCount = 0;

    // Register previous touch states
    for (int i = 0; i < MAX_TOUCH_POINTS; i++) CORE.Input.Touch.previousTouchState[i] = CORE.Input.Touch.currentTouchState[i];

    // Register previous mouse states (touch is mirrored onto the mouse below, so
    // IsMouseButtonPressed/Released — used by raym3 hit-testing — work correctly)
    CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;
    CORE.Input.Mouse.previousWheelMove = CORE.Input.Mouse.currentWheelMove;
    CORE.Input.Mouse.currentWheelMove = (Vector2){ 0.0f, 0.0f };
    for (int i = 0; i < MAX_MOUSE_BUTTONS; i++) CORE.Input.Mouse.previousButtonState[i] = CORE.Input.Mouse.currentButtonState[i];

    // Reset touch positions
    // TODO: It resets on target platform the mouse position and not filled again until a move-event,
    // so, if mouse is not moved it returns a (0, 0) position... this behaviour should be reviewed!
    //for (int i = 0; i < MAX_TOUCH_POINTS; i++) CORE.Input.Touch.position[i] = (Vector2){ 0, 0 };

    // Register previous keys states
    // NOTE: Android supports up to 260 keys
    for (int i = 0; i < 260; i++)
    {
        CORE.Input.Keyboard.previousKeyState[i] = CORE.Input.Keyboard.currentKeyState[i];
        CORE.Input.Keyboard.keyRepeatInFrame[i] = 0;
    }

    // Drain the touch queue (filled by the JNI UI thread). Actions:
    //   RCORE_AS_TOUCH_DOWN=0, UP=1, MOVE=2.
    //
    // Each frame's drain ends at the first DOWN/UP edge: after a stalled
    // frame the whole gesture may be queued, and draining it all at once
    // teleports the pointer to the gesture's end before the press is ever
    // hit-tested — a 700px swipe then reads as a tap at its end point.
    // Stopping at the edge keeps press/release positions exact; the host
    // keeps scheduling frames while RcoreAndroidSurface_HasPendingTouch().
    while (platform.touchTail != platform.touchHead)
    {
        int action = platform.touchQueue[platform.touchTail].action;
        int id     = platform.touchQueue[platform.touchTail].id;
        float x    = platform.touchQueue[platform.touchTail].x;
        float y    = platform.touchQueue[platform.touchTail].y;
        platform.touchTail = (platform.touchTail + 1) % RCORE_AS_TOUCH_QUEUE;

        if (id < 0 || id >= MAX_TOUCH_POINTS) continue;

        // Diagnostic: trace every primary-pointer (id 0) touch event so we
        // can verify the platform layer is delivering events end-to-end via
        //   adb logcat -d | grep "INPUT_TOUCH"
        // Touch id 0 is also mirrored onto the mouse below, so this is the
        // canonical signal that the whole input pipeline is alive.
        if (id == 0)
        {
            const char *act = "?";
            if      (action == RCORE_AS_TOUCH_DOWN) act = "DOWN";
            else if (action == RCORE_AS_TOUCH_UP)   act = "UP";
            else if (action == RCORE_AS_TOUCH_MOVE) act = "MOVE";
            TRACELOG(LOG_INFO, "INPUT_TOUCH id=%d action=%s pos=(%.1f, %.1f)", id, act, x, y);
        }

        CORE.Input.Touch.position[id] = (Vector2){ x, y };
        if (action == RCORE_AS_TOUCH_DOWN)      CORE.Input.Touch.currentTouchState[id] = 1;
        else if (action == RCORE_AS_TOUCH_UP)   CORE.Input.Touch.currentTouchState[id] = 0;

        // Mirror the primary pointer (id 0) onto the mouse so existing mouse-based
        // hit-testing (raym3) works unchanged.
        if (id == 0)
        {
            CORE.Input.Mouse.currentPosition = (Vector2){ x, y };
            CORE.Input.Touch.position[0] = (Vector2){ x, y };
            if (action == RCORE_AS_TOUCH_DOWN) {
                CORE.Input.Mouse.currentButtonState[MOUSE_BUTTON_LEFT] = 1;
                // Force previous=0 so the diff gives pressed=1 for this frame.
                // PollInputEvents already copied previous=current above; we
                // override to ensure the transition is detectable even when
                // DOWN+UP land in the same poll cycle.
                CORE.Input.Mouse.previousButtonState[MOUSE_BUTTON_LEFT] = 0;
            } else if (action == RCORE_AS_TOUCH_UP) {
                CORE.Input.Mouse.currentButtonState[MOUSE_BUTTON_LEFT] = 0;
                // Force previous=1 so the diff gives released=1 for this frame.
                CORE.Input.Mouse.previousButtonState[MOUSE_BUTTON_LEFT] = 1;
            }
            // Mirror mouse transitions too — this catches them even when the
            // raym3 host doesn't query IsMouseButtonPressed/Released.
            if (action == RCORE_AS_TOUCH_DOWN)
                TRACELOG(LOG_INFO, "INPUT_MOUSE_BUTTON_DOWN button=LEFT pos=(%.1f, %.1f)", x, y);
            else if (action == RCORE_AS_TOUCH_UP)
                TRACELOG(LOG_INFO, "INPUT_MOUSE_BUTTON_UP button=LEFT pos=(%.1f, %.1f)", x, y);
        }

        // Edge events end this frame's batch (see comment above the loop).
        if ((action == RCORE_AS_TOUCH_DOWN) || (action == RCORE_AS_TOUCH_UP)) break;
    }

    // Recompute active touch point count
    CORE.Input.Touch.pointCount = 0;
    for (int i = 0; i < MAX_TOUCH_POINTS; i++)
    {
        if (CORE.Input.Touch.currentTouchState[i] == 1)
        {
            CORE.Input.Touch.pointId[CORE.Input.Touch.pointCount] = i;
            CORE.Input.Touch.pointCount++;
        }
    }
}

//----------------------------------------------------------------------------------
// Module Internal Functions Definition
//----------------------------------------------------------------------------------

// One-time EGL init: display, config, context. Idempotent.
static bool InitEGL(void)
{
    if (platform.device != EGL_NO_DISPLAY) return true;

    EGLint samples = 0;
    EGLint sampleBuffer = 0;
    if (FLAG_IS_SET(CORE.Window.flags, FLAG_MSAA_4X_HINT)) { samples = 4; sampleBuffer = 1; }

    const EGLint framebufferAttribs[] = {
        EGL_RENDERABLE_TYPE, (rlGetVersion() == RL_OPENGL_ES_30)? EGL_OPENGL_ES3_BIT : EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,        // raym3 uses GL stencil for clipping — required or content is clipped away
        EGL_SAMPLE_BUFFERS, sampleBuffer,
        EGL_SAMPLES, samples,
        EGL_NONE
    };
    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, (rlGetVersion() == RL_OPENGL_ES_30)? 3 : 2,
        EGL_NONE
    };
    EGLint numConfigs = 0;

    platform.device = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (platform.device == EGL_NO_DISPLAY) { TRACELOG(LOG_FATAL, "DISPLAY: Failed to get EGL display"); return false; }
    if (eglInitialize(platform.device, NULL, NULL) == EGL_FALSE) { TRACELOG(LOG_FATAL, "DISPLAY: Failed to initialize EGL"); return false; }

    eglChooseConfig(platform.device, framebufferAttribs, &platform.config, 1, &numConfigs);
    eglBindAPI(EGL_OPENGL_ES_API);

    platform.context = eglCreateContext(platform.device, platform.config, EGL_NO_CONTEXT, contextAttribs);
    if (platform.context == EGL_NO_CONTEXT) { TRACELOG(LOG_FATAL, "DISPLAY: Failed to create EGL context"); return false; }

    rlLoadExtensions((void *)eglGetProcAddress);
    return true;
}

// Allocate a free slot in platform.windows[] for the given ANativeWindow and
// create an EGL surface for it. Returns the slot index, or -1 on failure.
static int AllocWindowSlot(ANativeWindow *window)
{
    if (window == NULL) return -1;
    // Slot 0 is reserved for the boot surface (set via RcoreAndroidSurface_SetWindow).
    // For new surfaces, find the first free slot starting at 1.
    for (int i = 1; i < RCORE_AS_MAX_WINDOWS; i++) {
        if (platform.windows[i].window == NULL) {
            platform.windows[i].id = g_nextWindowId++;
            platform.windows[i].window = window;
            platform.windows[i].width = ANativeWindow_getWidth(window);
            platform.windows[i].height = ANativeWindow_getHeight(window);
#if !defined(RAYLIB_USE_RLVK)
            platform.windows[i].surface = EGL_NO_SURFACE;
#endif
            if (platform.windowCount <= i) platform.windowCount = i + 1;
            return i;
        }
    }
    return -1;
}

// Initialize graphics device: EGL display/context/surface on the external window
bool InitGraphicsDevice(void)
{
    if (platform.windows[0].window == NULL)
    {
        TRACELOG(LOG_FATAL, "DISPLAY: No native window set — call RcoreAndroidSurface_SetWindow() before InitWindow()");
        return false;
    }

#if defined(RAYLIB_USE_RLVK)
    int wWidth = ANativeWindow_getWidth(platform.windows[0].window);
    int wHeight = ANativeWindow_getHeight(platform.windows[0].window);
    platform.windows[0].width = wWidth;
    platform.windows[0].height = wHeight;
    platform.windows[0].id = 1;
    platform.windowCount = 1;
    platform.currentWindow = 0;
    g_nextWindowId = 2;

    if (CORE.Window.screen.width == 0) CORE.Window.screen.width = wWidth;
    if (CORE.Window.screen.height == 0) CORE.Window.screen.height = wHeight;
    CORE.Window.display.width = wWidth;
    CORE.Window.display.height = wHeight;
    CORE.Window.render.width = CORE.Window.screen.width;
    CORE.Window.render.height = CORE.Window.screen.height;
    CORE.Window.currentFbo.width = CORE.Window.render.width;
    CORE.Window.currentFbo.height = CORE.Window.render.height;

    rlvkSetNativeWindow(platform.windows[0].window);
    CORE.Window.ready = true;
    return true;
#else
    if (!InitEGL()) return false;

    EGLint displayFormat = 0;
    eglGetConfigAttrib(platform.device, platform.config, EGL_NATIVE_VISUAL_ID, &displayFormat);

    // Edge-to-edge: render at the full window (surface) size — the Activity is
    // laid out behind the system bars, so the surface already spans the screen.
    int wWidth = ANativeWindow_getWidth(platform.windows[0].window);
    int wHeight = ANativeWindow_getHeight(platform.windows[0].window);
    if (CORE.Window.screen.width == 0) CORE.Window.screen.width = wWidth;
    if (CORE.Window.screen.height == 0) CORE.Window.screen.height = wHeight;
    CORE.Window.display.width = wWidth;
    CORE.Window.display.height = wHeight;

    ANativeWindow_setBuffersGeometry(platform.windows[0].window, wWidth, wHeight, displayFormat);
    platform.windows[0].id = 1;
    platform.windows[0].width = wWidth;
    platform.windows[0].height = wHeight;
    platform.windowCount = 1;
    g_nextWindowId = 2;
#if !defined(RAYLIB_USE_RLVK)
    platform.windows[0].surface = eglCreateWindowSurface(platform.device, platform.config, platform.windows[0].window, NULL);
    if (platform.windows[0].surface == EGL_NO_SURFACE) { TRACELOG(LOG_FATAL, "DISPLAY: Failed to create EGL window surface"); return false; }
#endif

    platform.currentWindow = 0;
    if (eglMakeCurrent(platform.device, platform.windows[0].surface, platform.windows[0].surface, platform.context) == EGL_FALSE)
    {
        TRACELOG(LOG_FATAL, "DISPLAY: Failed to attach EGL context to surface");
        return false;
    }

    CORE.Window.render.width = CORE.Window.screen.width;
    CORE.Window.render.height = CORE.Window.screen.height;
    CORE.Window.currentFbo.width = CORE.Window.render.width;
    CORE.Window.currentFbo.height = CORE.Window.render.height;

    CORE.Window.ready = true;
    return true;
#endif
}

// Initialize platform: graphics, inputs and more
int InitPlatform(void)
{
    FLAG_SET(CORE.Window.flags, FLAG_FULLSCREEN_MODE);
    // HighDPI: so raylib's BeginScissorMode multiplies the scissor rect by
    // GetWindowScaleDPI() (density) → logical(dp) scissor becomes pixels, matching
    // the dp content-scale applied to geometry. Without it, scissor stays in dp
    // units while the framebuffer is in px → clip lands off-screen.
    FLAG_SET(CORE.Window.flags, FLAG_WINDOW_HIGHDPI);

    if (!InitGraphicsDevice())
    {
        TRACELOG(LOG_FATAL, "PLATFORM: Failed to initialize graphics device");
        return -1;
    }

    TRACELOG(LOG_INFO, "DISPLAY: Device initialized successfully");
    TRACELOG(LOG_INFO, "    > Display size: %i x %i", CORE.Window.display.width, CORE.Window.display.height);
    TRACELOG(LOG_INFO, "    > Screen size:  %i x %i", CORE.Window.screen.width, CORE.Window.screen.height);
    TRACELOG(LOG_INFO, "    > Render size:  %i x %i", CORE.Window.render.width, CORE.Window.render.height);

    InitTimer();

    CORE.Storage.basePath = (platform.internalDataPath != NULL) ? platform.internalDataPath : GetWorkingDirectory();

    for (int i = 0; i < MAX_TOUCH_POINTS; i++) CORE.Input.Touch.position[i] = (Vector2){ 0, 0 };

    TRACELOG(LOG_INFO, "PLATFORM: ANDROID_SURFACE: Initialized successfully");
    return 0;
}

// Close platform
void ClosePlatform(void)
{
#if defined(RAYLIB_USE_RLVK)
    for (int i = 0; i < RCORE_AS_MAX_WINDOWS; i++) {
        if (platform.windows[i].window != NULL) {
            ANativeWindow_release(platform.windows[i].window);
            platform.windows[i].window = NULL;
        }
    }
#else
    if (platform.device != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(platform.device, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        for (int i = 0; i < RCORE_AS_MAX_WINDOWS; i++) {
            if (platform.windows[i].surface != EGL_NO_SURFACE) {
                eglDestroySurface(platform.device, platform.windows[i].surface);
                platform.windows[i].surface = EGL_NO_SURFACE;
            }
            if (platform.windows[i].window != NULL) {
                ANativeWindow_release(platform.windows[i].window);
                platform.windows[i].window = NULL;
            }
        }
        if (platform.context != EGL_NO_CONTEXT) { eglDestroyContext(platform.device, platform.context); platform.context = EGL_NO_CONTEXT; }
        eglTerminate(platform.device);
        platform.device = EGL_NO_DISPLAY;
    }
    platform.windowCount = 0;
    platform.currentWindow = -1;
#endif
}

// ─── multi-surface API ───────────────────────────────────────────────────

int RcoreAndroidSurface_CreateWindow(void *nativeWindow)
{
    if (nativeWindow == NULL) return 0;
#if defined(RAYLIB_USE_RLVK)
    int slot = AllocWindowSlot((ANativeWindow *)nativeWindow);
    if (slot < 0) return 0;
    return platform.windows[slot].id;
#else
    if (!InitEGL()) return 0;
    int slot = AllocWindowSlot((ANativeWindow *)nativeWindow);
    if (slot < 0) return 0;
    EGLint displayFormat = 0;
    eglGetConfigAttrib(platform.device, platform.config, EGL_NATIVE_VISUAL_ID, &displayFormat);
    ANativeWindow_setBuffersGeometry(platform.windows[slot].window,
        platform.windows[slot].width, platform.windows[slot].height, displayFormat);
    platform.windows[slot].surface = eglCreateWindowSurface(platform.device, platform.config,
        platform.windows[slot].window, NULL);
    if (platform.windows[slot].surface == EGL_NO_SURFACE) {
        TRACELOG(LOG_WARNING, "DISPLAY: Failed to create EGL surface for additional window");
        // Roll back slot allocation.
        ANativeWindow_release(platform.windows[slot].window);
        platform.windows[slot].window = NULL;
        platform.windows[slot].id = 0;
        return 0;
    }
    return platform.windows[slot].id;
#endif
}

int RcoreAndroidSurface_BindWindow(int id)
{
    if (id <= 0 || id >= g_nextWindowId) return 0;
    int slot = -1;
    for (int i = 0; i < RCORE_AS_MAX_WINDOWS; i++) {
        if (platform.windows[i].id == id) { slot = i; break; }
    }
    if (slot < 0 || platform.windows[slot].window == NULL) return 0;
#if !defined(RAYLIB_USE_RLVK)
    if (platform.windows[slot].surface != EGL_NO_SURFACE) {
        if (eglMakeCurrent(platform.device, platform.windows[slot].surface,
                            platform.windows[slot].surface, platform.context) == EGL_FALSE) {
            return 0;
        }
    }
#endif
    platform.currentWindow = slot;
    // Update raylib's global window state to reflect the new surface size.
    int w = platform.windows[slot].width;
    int h = platform.windows[slot].height;
    CORE.Window.screen.width = w;
    CORE.Window.screen.height = h;
    CORE.Window.display.width = w;
    CORE.Window.display.height = h;
    CORE.Window.render.width = w;
    CORE.Window.render.height = h;
    CORE.Window.currentFbo.width = w;
    CORE.Window.currentFbo.height = h;
#if defined(RAYLIB_USE_RLVK)
    rlvkSetNativeWindow(platform.windows[slot].window);
#endif
    // Keep the rlgl viewport + projection in sync with the bound window; each
    // surface can have a different size.
    SetupViewport(w, h);
    return 1;
}

int RcoreAndroidSurface_ResizeWindow(int id, int width, int height)
{
    if (id <= 0 || width <= 0 || height <= 0) return 0;
    int slot = -1;
    for (int i = 0; i < RCORE_AS_MAX_WINDOWS; i++) {
        if (platform.windows[i].id == id) { slot = i; break; }
    }
    if (slot < 0 || platform.windows[slot].window == NULL) return 0;
    if (platform.windows[slot].width == width &&
        platform.windows[slot].height == height) {
        return 1;
    }

    platform.windows[slot].width = width;
    platform.windows[slot].height = height;
    if (platform.currentWindow == slot) {
        CORE.Window.screen.width = width;
        CORE.Window.screen.height = height;
        CORE.Window.display.width = width;
        CORE.Window.display.height = height;
        CORE.Window.render.width = width;
        CORE.Window.render.height = height;
        CORE.Window.currentFbo.width = width;
        CORE.Window.currentFbo.height = height;
    }
#if defined(RAYLIB_USE_RLVK)
    rlvkSetNativeWindow(platform.windows[slot].window);
    if (!rlvkRecreateSurface()) return 0;
    // Refresh the rlgl viewport + ortho projection; otherwise draws stay
    // clipped/scaled to the previous surface size (rotation symptom).
    if (platform.currentWindow == slot) SetupViewport(width, height);
#else
    if (platform.windows[slot].surface != EGL_NO_SURFACE) {
        eglMakeCurrent(platform.device, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(platform.device, platform.windows[slot].surface);
        platform.windows[slot].surface = EGL_NO_SURFACE;
        EGLint displayFormat = 0;
        eglGetConfigAttrib(platform.device, platform.config, EGL_NATIVE_VISUAL_ID, &displayFormat);
        ANativeWindow_setBuffersGeometry(platform.windows[slot].window,
            width, height, displayFormat);
        platform.windows[slot].surface = eglCreateWindowSurface(platform.device, platform.config,
            platform.windows[slot].window, NULL);
        if (platform.windows[slot].surface == EGL_NO_SURFACE) return 0;
        if (platform.currentWindow == slot) {
            eglMakeCurrent(platform.device, platform.windows[slot].surface,
                           platform.windows[slot].surface, platform.context);
        }
    }
#endif
    return 1;
}

void RcoreAndroidSurface_SwapWindow(void)
{
    SwapScreenBuffer();
}

// Rebind a new ANativeWindow to the boot surface (slot 0) after Android
// destroyed the previous one while the process stayed alive (background →
// resume). With RLVK this recreates only the VkSurface + swapchain — the
// Vulkan device and every texture survive. Returns the window id (1) on
// success, 0 on failure. GLES path: not supported (caller falls back to a
// full InitWindow re-init).
int RcoreAndroidSurface_ResumeWindow(void *nativeWindow)
{
#if defined(RAYLIB_USE_RLVK)
    if (nativeWindow == NULL) return 0;
    ANativeWindow *win = (ANativeWindow *)nativeWindow;
    int wWidth = ANativeWindow_getWidth(win);
    int wHeight = ANativeWindow_getHeight(win);

    platform.windows[0].window = win;
    platform.windows[0].width = wWidth;
    platform.windows[0].height = wHeight;
    platform.windows[0].id = 1;
    if (platform.windowCount < 1) platform.windowCount = 1;
    platform.currentWindow = 0;
    if (g_nextWindowId < 2) g_nextWindowId = 2;

    CORE.Window.screen.width = wWidth;
    CORE.Window.screen.height = wHeight;
    CORE.Window.display.width = wWidth;
    CORE.Window.display.height = wHeight;
    CORE.Window.render.width = wWidth;
    CORE.Window.render.height = wHeight;
    CORE.Window.currentFbo.width = wWidth;
    CORE.Window.currentFbo.height = wHeight;

    rlvkSetNativeWindow(win);
    if (!rlvkRecreateSurface()) return 0;
    CORE.Window.ready = true;
    return platform.windows[0].id;
#else
    (void)nativeWindow;
    return 0;
#endif
}

void RcoreAndroidSurface_DestroyWindow(int id)
{
    if (id <= 0 || id >= g_nextWindowId) return;
    for (int i = 0; i < RCORE_AS_MAX_WINDOWS; i++) {
        if (platform.windows[i].id == id) {
#if !defined(RAYLIB_USE_RLVK)
            if (platform.windows[i].surface != EGL_NO_SURFACE) {
                eglDestroySurface(platform.device, platform.windows[i].surface);
                platform.windows[i].surface = EGL_NO_SURFACE;
            }
#endif
            if (platform.windows[i].window != NULL) {
                ANativeWindow_release(platform.windows[i].window);
                platform.windows[i].window = NULL;
            }
            platform.windows[i].id = 0;
            platform.windows[i].width = 0;
            platform.windows[i].height = 0;
            if (platform.currentWindow == i) platform.currentWindow = -1;
            return;
        }
    }
}

int RcoreAndroidSurface_GetCurrentId(void)
{
    int idx = platform.currentWindow;
    if (idx < 0 || idx >= RCORE_AS_MAX_WINDOWS) return 0;
    if (platform.windows[idx].window == NULL) return 0;
    return platform.windows[idx].id;
}

int RcoreAndroidSurface_GetWindowCount(void)
{
    int n = 0;
    for (int i = 0; i < RCORE_AS_MAX_WINDOWS; i++) {
        if (platform.windows[i].window != NULL) n++;
    }
    return n;
}

void RcoreAndroidSurface_GetWindowSize(int id, int *w, int *h)
{
    if (w) *w = 0;
    if (h) *h = 0;
    if (id <= 0) return;
    for (int i = 0; i < RCORE_AS_MAX_WINDOWS; i++) {
        if (platform.windows[i].id == id) {
            if (w) *w = platform.windows[i].width;
            if (h) *h = platform.windows[i].height;
            return;
        }
    }
}

// Monotonic clock in nanoseconds — used by the JNI layer to debounce per-frame
// work when multiple render threads each call nativeRenderFrame.
int64_t RcoreAndroidSurface_NowNanos(void)
{
    struct timespec ts = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}

// EOF
