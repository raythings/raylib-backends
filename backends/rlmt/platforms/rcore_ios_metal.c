/**********************************************************************************************
*
*   rcore_ios_metal - Minimal iOS platform shim for raylib-backends + rlmt
*
*   This file is package-owned build glue. It is included from a generated rcore overlay so
*   consumers can keep their upstream raylib checkout untouched.
*
**********************************************************************************************/

#include <time.h>
#include <unistd.h>

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct {
    void *metalLayer;
    int widthPx;
    int heightPx;
    float scale;
} PlatformData;

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
extern CoreData CORE;

static PlatformData platform = { 0 };

//----------------------------------------------------------------------------------
// Module Internal Functions Declaration
//----------------------------------------------------------------------------------
int InitPlatform(void);
bool InitGraphicsDevice(void);

//----------------------------------------------------------------------------------
// Consumer-facing layer bridge
//----------------------------------------------------------------------------------
void RcoreIosMetal_SetLayer(void *metalLayer, int widthPx, int heightPx, float scale)
{
    platform.metalLayer = metalLayer;
    platform.widthPx = widthPx;
    platform.heightPx = heightPx;
    platform.scale = (scale > 0.0f)? scale : 1.0f;
}

void RcoreIosMetal_ResizeLayer(int widthPx, int heightPx, float scale)
{
    platform.widthPx = widthPx;
    platform.heightPx = heightPx;
    platform.scale = (scale > 0.0f)? scale : 1.0f;

    if (CORE.Window.ready)
    {
        CORE.Window.display.width = widthPx;
        CORE.Window.display.height = heightPx;
        CORE.Window.screen.width = (int)((float)widthPx/platform.scale);
        CORE.Window.screen.height = (int)((float)heightPx/platform.scale);
        CORE.Window.render.width = widthPx;
        CORE.Window.render.height = heightPx;
        CORE.Window.currentFbo.width = widthPx;
        CORE.Window.currentFbo.height = heightPx;
        rlmtResizeSurface(widthPx, heightPx);
        SetupViewport(widthPx, heightPx);
    }
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Window and Graphics Device
//----------------------------------------------------------------------------------
bool WindowShouldClose(void)
{
    if (CORE.Window.ready) return CORE.Window.shouldClose;
    return true;
}

void ToggleFullscreen(void) { }
void ToggleBorderlessWindowed(void) { }
void MaximizeWindow(void) { }
void MinimizeWindow(void) { }
void RestoreWindow(void) { }
void SetWindowState(unsigned int flags) { CORE.Window.flags |= flags; }
void ClearWindowState(unsigned int flags) { CORE.Window.flags &= ~flags; }
void SetWindowIcon(Image image) { }
void SetWindowIcons(Image *images, int count) { }
void SetWindowTitle(const char *title) { CORE.Window.title = title; }
void SetWindowPosition(int x, int y) { }
void SetWindowMonitor(int monitor) { }

void SetWindowMinSize(int width, int height)
{
    CORE.Window.screenMin.width = width;
    CORE.Window.screenMin.height = height;
}

void SetWindowMaxSize(int width, int height)
{
    CORE.Window.screenMax.width = width;
    CORE.Window.screenMax.height = height;
}

void SetWindowSize(int width, int height)
{
    RcoreIosMetal_ResizeLayer((int)((float)width*platform.scale), (int)((float)height*platform.scale), platform.scale);
}

void SetWindowOpacity(float opacity) { }
void SetWindowFocused(void) { }
void *GetWindowHandle(void) { return platform.metalLayer; }
int GetMonitorCount(void) { return 1; }
int GetCurrentMonitor(void) { return 0; }
Vector2 GetMonitorPosition(int monitor) { return (Vector2){ 0, 0 }; }
int GetMonitorWidth(int monitor) { return CORE.Window.screen.width; }
int GetMonitorHeight(int monitor) { return CORE.Window.screen.height; }
int GetMonitorPhysicalWidth(int monitor) { return 0; }
int GetMonitorPhysicalHeight(int monitor) { return 0; }
int GetMonitorRefreshRate(int monitor) { return 60; }
const char *GetMonitorName(int monitor) { return "iOS"; }
Vector2 GetWindowPosition(void) { return (Vector2){ 0, 0 }; }
Vector2 GetWindowScaleDPI(void) { return (Vector2){ platform.scale, platform.scale }; }
void SetClipboardText(const char *text) { }
const char *GetClipboardText(void) { return NULL; }
Image GetClipboardImage(void) { return (Image){ 0 }; }
void ShowCursor(void) { CORE.Input.Mouse.cursorHidden = false; }
void HideCursor(void) { CORE.Input.Mouse.cursorHidden = true; }
void EnableCursor(void) { CORE.Input.Mouse.cursorHidden = false; }
void DisableCursor(void) { CORE.Input.Mouse.cursorHidden = true; }

void SwapScreenBuffer(void)
{
    rlSwapScreenBuffers();
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Misc
//----------------------------------------------------------------------------------
double GetTime(void)
{
    struct timespec ts = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &ts);

    unsigned long long now = (unsigned long long)ts.tv_sec*1000000000LLU + (unsigned long long)ts.tv_nsec;
    if (CORE.Time.base == 0) CORE.Time.base = now;

    return (double)(now - CORE.Time.base)*1e-9;
}

void OpenURL(const char *url)
{
    TRACELOG(LOG_WARNING, "OpenURL() not implemented by rcore_ios_metal");
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Inputs
//----------------------------------------------------------------------------------
int SetGamepadMappings(const char *mappings) { return 0; }
void SetGamepadVibration(int gamepad, float leftMotor, float rightMotor, float duration) { }

void SetMousePosition(int x, int y)
{
    CORE.Input.Mouse.currentPosition = (Vector2){ (float)x, (float)y };
    CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;
}

void SetMouseCursor(int cursor) { }
const char *GetKeyName(int key) { return ""; }

void PollInputEvents(void)
{
#if SUPPORT_GESTURES_SYSTEM
    UpdateGestures();
#endif

    CORE.Input.Keyboard.keyPressedQueueCount = 0;
    CORE.Input.Keyboard.charPressedQueueCount = 0;

    for (int i = 0; i < MAX_KEYBOARD_KEYS; i++)
    {
        CORE.Input.Keyboard.previousKeyState[i] = CORE.Input.Keyboard.currentKeyState[i];
        CORE.Input.Keyboard.keyRepeatInFrame[i] = 0;
    }

    CORE.Input.Gamepad.lastButtonPressed = 0;

    for (int i = 0; i < MAX_TOUCH_POINTS; i++)
    {
        CORE.Input.Touch.previousTouchState[i] = CORE.Input.Touch.currentTouchState[i];
    }
}

//----------------------------------------------------------------------------------
// Module Internal Functions Definition
//----------------------------------------------------------------------------------
int InitPlatform(void)
{
    if (platform.metalLayer == NULL)
    {
        TRACELOG(LOG_FATAL, "PLATFORM: iOS Metal layer was not provided before InitWindow()");
        return -1;
    }

    if (platform.widthPx <= 0) platform.widthPx = CORE.Window.screen.width;
    if (platform.heightPx <= 0) platform.heightPx = CORE.Window.screen.height;
    if (platform.scale <= 0.0f) platform.scale = 1.0f;

    CORE.Window.display.width = platform.widthPx;
    CORE.Window.display.height = platform.heightPx;
    CORE.Window.screen.width = (int)((float)platform.widthPx/platform.scale);
    CORE.Window.screen.height = (int)((float)platform.heightPx/platform.scale);
    CORE.Window.render.width = platform.widthPx;
    CORE.Window.render.height = platform.heightPx;
    CORE.Window.currentFbo.width = platform.widthPx;
    CORE.Window.currentFbo.height = platform.heightPx;

    rlmtSetMetalLayer(platform.metalLayer, platform.widthPx, platform.heightPx);
    rlLoadExtensions(NULL);

    CORE.Window.ready = true;
    CORE.Storage.basePath = GetWorkingDirectory();

    TRACELOG(LOG_INFO, "DISPLAY: Device initialized successfully");
    TRACELOG(LOG_INFO, "    > Display size: %i x %i", CORE.Window.display.width, CORE.Window.display.height);
    TRACELOG(LOG_INFO, "    > Screen size:  %i x %i", CORE.Window.screen.width, CORE.Window.screen.height);
    TRACELOG(LOG_INFO, "    > Render size:  %i x %i", CORE.Window.render.width, CORE.Window.render.height);
    TRACELOG(LOG_INFO, "PLATFORM: IOS (Metal external layer): Initialized successfully");

    return 0;
}

void ClosePlatform(void)
{
    platform.metalLayer = NULL;
}

// EOF
