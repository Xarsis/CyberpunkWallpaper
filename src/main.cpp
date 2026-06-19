// CyberpunkWallpaper - main.cpp
// Multi-monitor support: one independent animation per display

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <random>
#include <string>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <thread>
#include <atomic>

#include "GlyphAtlas.h"
#include "ColumnManager.h"
#include "Renderer.h"
#include "PostProcessor.h"
#include "Wallpaper.h"
#include "TrayIcon.h"

// ─── Config ───────────────────────────────────────────────────────────────────
static const bool  PIN_TO_DESKTOP = true;
static const float TARGET_FPS     = 60.0f;
static const int   FONT_SIZE      = 28;

// ─── Log file ─────────────────────────────────────────────────────────────────
static FILE* g_log = nullptr;
void logOpen()  { g_log = fopen("cwp_log.txt", "w"); }
void logMsg(const char* fmt, ...) {
    if (!g_log) return;
    va_list args; va_start(args, fmt);
    vfprintf(g_log, fmt, args);
    fprintf(g_log, "\n");
    fflush(g_log);
    va_end(args);
}

// ─── Run modes ────────────────────────────────────────────────────────────────
enum class RunMode { Normal, Screensaver, Preview, Configure };

// ─── Globals ──────────────────────────────────────────────────────────────────
RunMode g_mode        = RunMode::Normal;
HWND    g_previewHwnd = nullptr;

static bool   g_firstMousePos = true;
static double g_lastMouseX    = 0.0;
static double g_lastMouseY    = 0.0;

// ─── Per-monitor state ────────────────────────────────────────────────────────
struct MonitorWindow {
    GLFWwindow*   window    = nullptr;
    GlyphAtlas*   atlas     = nullptr;
    ColumnManager* columns  = nullptr;
    Renderer*     renderer  = nullptr;
    PostProcessor* post     = nullptr;
    int x = 0, y = 0, w = 0, h = 0;
};

std::vector<MonitorWindow> g_monitors;

// ─── Callbacks ────────────────────────────────────────────────────────────────
void key_callback(GLFWwindow* window, int key, int, int action, int) {
    if (action == GLFW_PRESS) {
        if (g_mode == RunMode::Screensaver)
            glfwSetWindowShouldClose(window, true);
        if (g_mode == RunMode::Normal && key == GLFW_KEY_ESCAPE)
            for (auto& m : g_monitors)
                glfwSetWindowShouldClose(m.window, true);
    }
}
void mouse_button_callback(GLFWwindow* window, int, int action, int) {
    if (g_mode == RunMode::Screensaver && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}
void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    if (g_mode != RunMode::Screensaver) return;
    if (g_firstMousePos) { g_lastMouseX = xpos; g_lastMouseY = ypos; g_firstMousePos = false; return; }
    double dx = xpos - g_lastMouseX, dy = ypos - g_lastMouseY;
    if (dx*dx + dy*dy > 25.0)
        for (auto& m : g_monitors)
            glfwSetWindowShouldClose(m.window, true);
}
void framebuffer_size_callback(GLFWwindow*, int w, int h) { glViewport(0, 0, w, h); }

// ─── Parse args ───────────────────────────────────────────────────────────────
void parseArgs(int argc, char* argv[], RunMode& mode, HWND& previewHwnd) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);
        if (!arg.empty() && (arg[0]=='/'||arg[0]=='-')) arg = arg.substr(1);
        if      (arg=="s") { mode = RunMode::Screensaver; }
        else if (arg=="p" && i+1<argc) { mode = RunMode::Preview; previewHwnd = (HWND)(UINT_PTR)std::stoull(argv[++i]); }
        else if (arg=="c") { mode = RunMode::Configure; }
    }
}

// ─── Create one monitor window ────────────────────────────────────────────────
// share = existing window to share GL context with (for texture sharing), or null
bool createMonitorWindow(MonitorWindow& mw, GLFWwindow* share, bool borderless) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DECORATED,  borderless ? GLFW_FALSE : GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE,  GLFW_FALSE);
    glfwWindowHint(GLFW_FOCUSED,    GLFW_FALSE);

    mw.window = glfwCreateWindow(mw.w, mw.h, "CyberpunkWallpaper", nullptr, share);
    if (!mw.window) return false;

    glfwSetWindowPos(mw.window, mw.x, mw.y);
    glfwSetKeyCallback(mw.window,         key_callback);
    glfwSetMouseButtonCallback(mw.window, mouse_button_callback);
    glfwSetCursorPosCallback(mw.window,   cursor_pos_callback);
    glfwSetFramebufferSizeCallback(mw.window, framebuffer_size_callback);

    if (borderless)
        glfwSetInputMode(mw.window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

    // Init GL for this context
    glfwMakeContextCurrent(mw.window);
    glfwSwapInterval(1);

    // Only load GLAD once (first window)
    if (!share) {
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return false;
    }

    glViewport(0, 0, mw.w, mw.h);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Each monitor gets its own atlas, columns, renderer, post-processor
    mw.atlas = new GlyphAtlas();
    if (!mw.atlas->init("C:/Windows/Fonts/msgothic.ttc", FONT_SIZE)) {
        if (!mw.atlas->init("C:/Windows/Fonts/cour.ttf", FONT_SIZE)) {
            logMsg("ERROR: font load failed for monitor %dx%d", mw.w, mw.h);
            return false;
        }
    }

    mw.columns = new ColumnManager();
    mw.columns->init(mw.w, mw.h);

    mw.renderer = new Renderer();
    if (!mw.renderer->init(mw.w, mw.h, mw.atlas)) {
        logMsg("ERROR: Renderer init failed for monitor %dx%d", mw.w, mw.h);
        return false;
    }

    mw.post = new PostProcessor();
    if (!mw.post->init(mw.w, mw.h)) {
        logMsg("ERROR: PostProcessor init failed for monitor %dx%d", mw.w, mw.h);
        return false;
    }

    logMsg("Monitor window OK: pos(%d,%d) size(%dx%d)", mw.x, mw.y, mw.w, mw.h);
    return true;
}

void destroyMonitorWindow(MonitorWindow& mw) {
    if (mw.renderer) { mw.renderer->destroy(); delete mw.renderer; mw.renderer = nullptr; }
    if (mw.post)     { mw.post->destroy();     delete mw.post;     mw.post     = nullptr; }
    if (mw.atlas)    { mw.atlas->destroy();    delete mw.atlas;    mw.atlas    = nullptr; }
    if (mw.columns)  {                         delete mw.columns;  mw.columns  = nullptr; }
    if (mw.window)   { glfwDestroyWindow(mw.window); mw.window = nullptr; }
}

// ─── Enumerate monitors via EnumDisplaySettings (always physical pixels) ──────
struct MonitorInfo { int x, y, w, h; };

static std::vector<MonitorInfo> enumerateMonitors() {
    std::vector<MonitorInfo> result;

    // EnumDisplayDevices + EnumDisplaySettingsA always returns physical pixels
    // regardless of DPI awareness — no manifest tricks needed.
    DISPLAY_DEVICEA dd = {};
    dd.cb = sizeof(dd);

    for (DWORD devIdx = 0; EnumDisplayDevicesA(nullptr, devIdx, &dd, 0); ++devIdx) {
        // Skip non-attached and mirroring devices
        if (!(dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) continue;
        if (  dd.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER)     continue;

        DEVMODEA dm = {};
        dm.dmSize = sizeof(dm);
        if (!EnumDisplaySettingsA(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) continue;

        MonitorInfo mi;
        mi.x = dm.dmPosition.x;
        mi.y = dm.dmPosition.y;
        mi.w = (int)dm.dmPelsWidth;
        mi.h = (int)dm.dmPelsHeight;
        result.push_back(mi);

        logMsg("Display '%s': pos(%d,%d) size(%dx%d)",
               dd.DeviceName, mi.x, mi.y, mi.w, mi.h);
    }
    return result;
}

// ─── Main render loop ─────────────────────────────────────────────────────────
void runLoop() {
    auto lastFrame = std::chrono::high_resolution_clock::now();
    float time = 0.0f;

    bool anyOpen = true;
    while (anyOpen && !TrayIcon::exitRequested()) {
        auto now      = std::chrono::high_resolution_clock::now();
        float dt      = std::chrono::duration<float>(now - lastFrame).count();
        lastFrame     = now;

        glfwPollEvents();

        if (TrayIcon::isPaused()) { Sleep(50); continue; }

        time += dt;
        anyOpen = false;

        for (auto& mw : g_monitors) {
            if (glfwWindowShouldClose(mw.window)) continue;
            anyOpen = true;

            glfwMakeContextCurrent(mw.window);

            mw.columns->update(dt, time);

            mw.post->bindFramebuffer();
            glClearColor(0.0f, 0.0f, 0.03f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            mw.renderer->draw(*mw.columns, time);
            mw.post->unbindFramebuffer();

            glClear(GL_COLOR_BUFFER_BIT);
            mw.post->render(time);

            glfwSwapBuffers(mw.window);
        }
    }
}

// ─── Entry point ──────────────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    logOpen();
    logMsg("CyberpunkWallpaper starting (multi-monitor)...");

    // Set working dir to exe location
    char exePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string exeDir = exePath;
    auto lastSlash = exeDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) exeDir = exeDir.substr(0, lastSlash);
    SetCurrentDirectoryA(exeDir.c_str());
    logMsg("Working dir: %s", exeDir.c_str());

    parseArgs(__argc, __argv, g_mode, g_previewHwnd);
    logMsg("RunMode: %d", (int)g_mode);

    if (!glfwInit()) { logMsg("glfwInit failed"); return 1; }

    // Don't let GLFW scale window content — we handle DPI ourselves
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_FALSE);

    switch (g_mode) {

    case RunMode::Configure:
        MessageBoxA(nullptr, "CyberpunkWallpaper\n\nEdit main.cpp to configure.",
                    "CyberpunkWallpaper", MB_OK | MB_ICONINFORMATION);
        glfwTerminate();
        return 0;

    case RunMode::Preview: {
        // Single window in preview pane
        if (!g_previewHwnd) { glfwTerminate(); return 1; }
        RECT rc; GetClientRect(g_previewHwnd, &rc);
        MonitorWindow mw;
        mw.x = 0; mw.y = 0;
        mw.w = std::max((int)(rc.right-rc.left), 200);
        mw.h = std::max((int)(rc.bottom-rc.top), 150);
        if (!createMonitorWindow(mw, nullptr, true)) { glfwTerminate(); return 1; }

        // Re-parent into preview hwnd
        HWND childHwnd = glfwGetWin32Window(mw.window);
        SetParent(childHwnd, g_previewHwnd);
        SetWindowPos(childHwnd, nullptr, 0, 0, mw.w, mw.h, SWP_NOZORDER|SWP_SHOWWINDOW);

        g_monitors.push_back(mw);
        runLoop();
        break;
    }

    case RunMode::Screensaver: {
        // One window per monitor, fullscreen
        auto monInfos = enumerateMonitors();
        logMsg("Screensaver: %d monitors found", (int)monInfos.size());

        GLFWwindow* sharedCtx = nullptr;
        for (auto& mi : monInfos) {
            MonitorWindow mw;
            mw.x = mi.x; mw.y = mi.y; mw.w = mi.w; mw.h = mi.h;
            if (!createMonitorWindow(mw, sharedCtx, true)) continue;
            if (!sharedCtx) sharedCtx = mw.window;
            g_monitors.push_back(mw);
        }
        if (g_monitors.empty()) { glfwTerminate(); return 1; }
        runLoop();
        break;
    }

    default: {
        // Normal / wallpaper — one window per monitor
        auto monInfos = enumerateMonitors();
        logMsg("Normal: %d monitors found", (int)monInfos.size());

        GLFWwindow* sharedCtx = nullptr;
        for (auto& mi : monInfos) {
            MonitorWindow mw;
            mw.x = mi.x; mw.y = mi.y; mw.w = mi.w; mw.h = mi.h;
            if (!createMonitorWindow(mw, sharedCtx, PIN_TO_DESKTOP)) continue;
            if (!sharedCtx) sharedCtx = mw.window;
            g_monitors.push_back(mw);
        }

        if (g_monitors.empty()) { glfwTerminate(); return 1; }

        // Spawn WorkerW layer once, then pin each window with its exact rect
        if (PIN_TO_DESKTOP) {
            Wallpaper::init();
            for (auto& mw : g_monitors) {
                logMsg("Pinning monitor: screen pos(%d,%d) size(%dx%d)", mw.x, mw.y, mw.w, mw.h);
                Wallpaper::pinToDesktop(mw.window, mw.x, mw.y, mw.w, mw.h);
            }
        }

        TrayIcon::start();
        runLoop();
        TrayIcon::stop();
        break;
    }

    }

    for (auto& mw : g_monitors) destroyMonitorWindow(mw);
    glfwTerminate();

    // Tell Windows to repaint the desktop so the original wallpaper is restored
    SystemParametersInfoA(SPI_SETDESKWALLPAPER, 0, nullptr, SPIF_UPDATEINIFILE);
    InvalidateRect(nullptr, nullptr, TRUE);

    return 0;
}
