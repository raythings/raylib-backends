/**********************************************************************************************
*
*   rcore_web (rlwg) - raylib PLATFORM_WEB layer for the WebGPU backend
*
*   Replaces raylib's stock platforms/rcore_web.c (which creates a WebGL context via
*   GLFW). Here the browser canvas is sized and the rlwg backend acquires its own
*   WebGPU instance/adapter/device/surface inside rlglInit() - no GL context is created.
*
*   Driven by the rcore overlay: the CMake overlay rewrites
*       #include "platforms/rcore_web.c"
*   to point at this file when the WEBGPU backend is attached.
*
*   Frame pacing: with -sASYNCIFY the user's `while (!WindowShouldClose())` loop
*   cooperates with the browser by yielding once per frame in SwapScreenBuffer().
*
*   LICENSE: zlib/libpng (matches raylib)
*
**********************************************************************************************/

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <time.h>

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct {
    char canvasSelector[64];
} PlatformData;

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
extern CoreData CORE;                   // Global CORE state context

static PlatformData platform = { 0 };   // Platform specific data

// rlwg backend hooks (declared in rlwg.h, which is included via the rlgl overlay)
void rlwgResize(int width, int height);

//----------------------------------------------------------------------------------
// Module Internal Functions Declaration
//----------------------------------------------------------------------------------
int InitPlatform(void);

//----------------------------------------------------------------------------------
// Window and Graphics Device
//----------------------------------------------------------------------------------
bool WindowShouldClose(void)
{
    if (CORE.Window.ready) return CORE.Window.shouldClose;
    else return true;
}

void ToggleFullscreen(void) { TRACELOG(LOG_WARNING, "ToggleFullscreen() not available on web (rlwg)"); }
void ToggleBorderlessWindowed(void) { TRACELOG(LOG_WARNING, "ToggleBorderlessWindowed() not available on web (rlwg)"); }
void MaximizeWindow(void) { TRACELOG(LOG_WARNING, "MaximizeWindow() not available on web (rlwg)"); }
void MinimizeWindow(void) { TRACELOG(LOG_WARNING, "MinimizeWindow() not available on web (rlwg)"); }
void RestoreWindow(void) { TRACELOG(LOG_WARNING, "RestoreWindow() not available on web (rlwg)"); }
void SetWindowState(unsigned int flags) { (void)flags; TRACELOG(LOG_WARNING, "SetWindowState() not available on web (rlwg)"); }
void ClearWindowState(unsigned int flags) { (void)flags; TRACELOG(LOG_WARNING, "ClearWindowState() not available on web (rlwg)"); }
void SetWindowIcon(Image image) { (void)image; }
void SetWindowIcons(Image *images, int count) { (void)images; (void)count; }
void SetWindowTitle(const char *title) { CORE.Window.title = title; emscripten_set_window_title(title); }
void SetWindowPosition(int x, int y) { (void)x; (void)y; }
void SetWindowMonitor(int monitor) { (void)monitor; }
void SetWindowMinSize(int width, int height) { CORE.Window.screenMin.width = width; CORE.Window.screenMin.height = height; }
void SetWindowMaxSize(int width, int height) { CORE.Window.screenMax.width = width; CORE.Window.screenMax.height = height; }

void SetWindowSize(int width, int height)
{
    CORE.Window.screen.width = width;
    CORE.Window.screen.height = height;
    CORE.Window.render.width = width;
    CORE.Window.render.height = height;
    CORE.Window.currentFbo.width = width;
    CORE.Window.currentFbo.height = height;
    emscripten_set_canvas_element_size(platform.canvasSelector, width, height);
    rlwgResize(width, height);
}

void SetWindowOpacity(float opacity) { (void)opacity; }
void SetWindowFocused(void) {}
void *GetWindowHandle(void) { return NULL; }

int GetMonitorCount(void) { return 1; }
int GetCurrentMonitor(void) { return 0; }
Vector2 GetMonitorPosition(int monitor) { (void)monitor; return (Vector2){ 0, 0 }; }
int GetMonitorWidth(int monitor) { (void)monitor; return CORE.Window.screen.width; }
int GetMonitorHeight(int monitor) { (void)monitor; return CORE.Window.screen.height; }
int GetMonitorPhysicalWidth(int monitor) { (void)monitor; return 0; }
int GetMonitorPhysicalHeight(int monitor) { (void)monitor; return 0; }
int GetMonitorRefreshRate(int monitor) { (void)monitor; return 60; }
const char *GetMonitorName(int monitor) { (void)monitor; return "Browser canvas"; }
Vector2 GetWindowPosition(void) { return (Vector2){ 0, 0 }; }
Vector2 GetWindowScaleDPI(void) { return (Vector2){ 1.0f, 1.0f }; }

void SetClipboardText(const char *text) { (void)text; }
const char *GetClipboardText(void) { return NULL; }
Image GetClipboardImage(void) { Image image = { 0 }; return image; }

void ShowCursor(void) { CORE.Input.Mouse.cursorHidden = false; }
void HideCursor(void) { CORE.Input.Mouse.cursorHidden = true; }
void EnableCursor(void) { CORE.Input.Mouse.cursorHidden = false; }
void DisableCursor(void) { CORE.Input.Mouse.cursorHidden = true; }

// On the browser the canvas presents implicitly when the frame's command buffer is
// submitted (rlEndFrame). Here we only yield to the browser event loop so the user's
// blocking render loop cooperates (requires -sASYNCIFY).
void SwapScreenBuffer(void)
{
    emscripten_sleep(0);
}

//----------------------------------------------------------------------------------
// Misc
//----------------------------------------------------------------------------------
double GetTime(void)
{
    struct timespec ts = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &ts);
    unsigned long long nanoSeconds = (unsigned long long)ts.tv_sec*1000000000LLU + (unsigned long long)ts.tv_nsec;
    return (double)(nanoSeconds - CORE.Time.base)*1e-9;
}

void OpenURL(const char *url)
{
    if (strchr(url, '\'') != NULL) TRACELOG(LOG_WARNING, "SYSTEM: Provided URL could be potentially malicious");
    else emscripten_run_script(TextFormat("window.open('%s', '_blank')", url));
}

//----------------------------------------------------------------------------------
// Inputs
//----------------------------------------------------------------------------------
int SetGamepadMappings(const char *mappings) { (void)mappings; return 0; }
void SetGamepadVibration(int gamepad, float leftMotor, float rightMotor, float duration) { (void)gamepad;(void)leftMotor;(void)rightMotor;(void)duration; }
void SetMousePosition(int x, int y)
{
    CORE.Input.Mouse.currentPosition = (Vector2){ (float)x, (float)y };
    CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;
}
void SetMouseCursor(int cursor) { (void)cursor; }
const char *GetKeyName(int key) { (void)key; return ""; }

//----------------------------------------------------------------------------------
// Input event callbacks (mouse/keyboard) - minimal but functional
//----------------------------------------------------------------------------------
static EM_BOOL rlwgMouseMoveCb(int type, const EmscriptenMouseEvent *e, void *ud)
{
    (void)type; (void)ud;
    CORE.Input.Mouse.currentPosition = (Vector2){ (float)e->targetX, (float)e->targetY };
    CORE.Input.Touch.position[0] = CORE.Input.Mouse.currentPosition;
    return EM_TRUE;
}
static EM_BOOL rlwgMouseBtnCb(int type, const EmscriptenMouseEvent *e, void *ud)
{
    (void)ud;
    int btn = (e->button == 2) ? MOUSE_BUTTON_RIGHT : (e->button == 1) ? MOUSE_BUTTON_MIDDLE : MOUSE_BUTTON_LEFT;
    CORE.Input.Mouse.currentButtonState[btn] = (type == EMSCRIPTEN_EVENT_MOUSEDOWN) ? 1 : 0;
    return EM_TRUE;
}

void PollInputEvents(void)
{
#if SUPPORT_GESTURES_SYSTEM
    UpdateGestures();
#endif
    CORE.Input.Keyboard.keyPressedQueueCount = 0;
    CORE.Input.Keyboard.charPressedQueueCount = 0;
    for (int i = 0; i < MAX_KEYBOARD_KEYS; i++) CORE.Input.Keyboard.keyRepeatInFrame[i] = 0;
    CORE.Input.Gamepad.lastButtonPressed = 0;
    for (int i = 0; i < MAX_TOUCH_POINTS; i++) CORE.Input.Touch.previousTouchState[i] = CORE.Input.Touch.currentTouchState[i];
    for (int i = 0; i < 260; i++)
    {
        CORE.Input.Keyboard.previousKeyState[i] = CORE.Input.Keyboard.currentKeyState[i];
        CORE.Input.Keyboard.keyRepeatInFrame[i] = 0;
    }
    // Carry mouse button state forward (event callbacks update currentButtonState)
    for (int i = 0; i < MAX_MOUSE_BUTTONS; i++) CORE.Input.Mouse.previousButtonState[i] = CORE.Input.Mouse.currentButtonState[i];
    CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;
}

//----------------------------------------------------------------------------------
// Platform init / close
//----------------------------------------------------------------------------------
int InitPlatform(void)
{
    strcpy(platform.canvasSelector, "#canvas");

    CORE.Window.flags |= FLAG_WINDOW_HIGHDPI;

    int w = CORE.Window.screen.width, h = CORE.Window.screen.height;
    emscripten_set_canvas_element_size(platform.canvasSelector, w, h);

    CORE.Window.display.width = w;
    CORE.Window.display.height = h;
    CORE.Window.render.width = w;
    CORE.Window.render.height = h;
    CORE.Window.currentFbo.width = w;
    CORE.Window.currentFbo.height = h;

    // Acquire the WebGPU device/surface now (async, ASYNCIFY-blocking) so that
    // rlglInit() called by rcore.c immediately after InitPlatform() returns finds
    // device+surface already set and skips re-acquisition.
    rlwgAcquireDevice(platform.canvasSelector);
    CORE.Window.ready = true;

    // Input callbacks
    emscripten_set_mousemove_callback(platform.canvasSelector, NULL, EM_TRUE, rlwgMouseMoveCb);
    emscripten_set_mousedown_callback(platform.canvasSelector, NULL, EM_TRUE, rlwgMouseBtnCb);
    emscripten_set_mouseup_callback(platform.canvasSelector, NULL, EM_TRUE, rlwgMouseBtnCb);

    InitTimer();
    CORE.Storage.basePath = GetWorkingDirectory();

    TRACELOG(LOG_INFO, "PLATFORM: WEB (rlwg/WebGPU): Initialized successfully");
    return 0;
}

void ClosePlatform(void)
{
    rlglClose();
}

// EOF
