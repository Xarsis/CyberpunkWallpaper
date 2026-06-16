// CyberpunkWallpaper - main.cpp
// Cyberpunk Matrix-style animated desktop wallpaper + screensaver
// Requires: GLFW3, GLAD, GLM, FreeType2
//
// Screensaver modes (Windows passes these automatically):
//   /s        — run fullscreen screensaver
//   /p <hwnd> — render into Settings preview pane
//   /c        — show config dialog (stub)
//   (none)    — normal windowed/wallpaper mode

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

#include "GlyphAtlas.h"
#include "ColumnManager.h"
#include "Renderer.h"
#include "PostProcessor.h"
#include "Wallpaper.h"

// ─── Config ───────────────────────────────────────────────────────────────────
static const int   SCREEN_W       = 1920;
static const int   SCREEN_H       = 1080;
static const bool  PIN_TO_DESKTOP = false; // set true to push behind desktop icons
static const float TARGET_FPS     = 60.0f;

// ─── Log file ─────────────────────────────────────────────────────────────────
static FILE* g_log = nullptr;
void logOpen()  { g_log = fopen("cwp_log.txt", "w"); }
void logClose() { if (g_log) { fclose(g_log); g_log = nullptr; } }
void logMsg(const char* fmt, ...) {
    if (!g_log) return;
    va_list args; va_start(args, fmt);
    vfprintf(g_log, fmt, args);
    fprintf(g_log, "\n");
    fflush(g_log);
    va_end(args);
}

// ─── Run modes ────────────────────────────────────────────────────────────────
enum class RunMode {
    Normal,      // /s not passed — windowed dev/wallpaper mode
    Screensaver, // /s — fullscreen, exits on any input
    Preview,     // /p <hwnd> — render into tiny Settings preview pane
    Configure,   // /c — show config dialog
};

// ─── Globals ──────────────────────────────────────────────────────────────────
GLFWwindow* g_window    = nullptr;
float       g_time      = 0.0f;
float       g_deltaTime = 0.0f;
RunMode     g_mode      = RunMode::Normal;
HWND        g_previewHwnd = nullptr; // set in Preview mode

// ─── Screensaver input: any key/mouse/move exits ──────────────────────────────
static bool      g_firstMousePos = true;
static double    g_lastMouseX    = 0.0;
static double    g_lastMouseY    = 0.0;

void key_callback(GLFWwindow* window, int key, int /*scan*/, int action, int /*mods*/) {
    if (action == GLFW_PRESS) {
        if (g_mode == RunMode::Screensaver)
            glfwSetWindowShouldClose(window, true);
        if (g_mode == RunMode::Normal && key == GLFW_KEY_ESCAPE)
            glfwSetWindowShouldClose(window, true);
    }
}

void mouse_button_callback(GLFWwindow* window, int /*btn*/, int action, int /*mods*/) {
    if (g_mode == RunMode::Screensaver && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    if (g_mode != RunMode::Screensaver) return;
    if (g_firstMousePos) {
        g_lastMouseX = xpos; g_lastMouseY = ypos;
        g_firstMousePos = false;
        return;
    }
    // Exit on meaningful movement (>5px) to ignore tiny jitter on wakeup
    double dx = xpos - g_lastMouseX;
    double dy = ypos - g_lastMouseY;
    if (dx * dx + dy * dy > 25.0)
        glfwSetWindowShouldClose(window, true);
}

void framebuffer_size_callback(GLFWwindow* /*window*/, int w, int h) {
    glViewport(0, 0, w, h);
}

// ─── Parse command line ────────────────────────────────────────────────────────
void parseArgs(int argc, char* argv[], RunMode& mode, HWND& previewHwnd) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        // Lowercase for comparison
        std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);
        // Strip leading - or /
        if (!arg.empty() && (arg[0] == '/' || arg[0] == '-'))
            arg = arg.substr(1);

        if (arg == "s") {
            mode = RunMode::Screensaver;
        } else if (arg == "p" && i + 1 < argc) {
            mode = RunMode::Preview;
            previewHwnd = (HWND)(UINT_PTR)std::stoull(argv[++i]);
        } else if (arg == "c") {
            mode = RunMode::Configure;
        }
    }
}

// ─── Init OpenGL window ────────────────────────────────────────────────────────
bool initGL(int w, int h, bool fullscreen, bool borderless, HWND parentHwnd = nullptr) {
    if (!glfwInit()) return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DECORATED,  borderless ? GLFW_FALSE : GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE,  GLFW_FALSE);

    // In preview mode we want a small child window; no cursor capture
    if (parentHwnd)
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // we'll show it after parenting

    GLFWmonitor* monitor = fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    g_window = glfwCreateWindow(w, h, "CyberpunkWallpaper", monitor, nullptr);
    if (!g_window) { glfwTerminate(); return false; }

    // ── Preview: re-parent GLFW window into the Settings preview HWND ─────────
    if (parentHwnd) {
        HWND childHwnd = glfwGetWin32Window(g_window);
        SetParent(childHwnd, parentHwnd);
        // Stretch to fill the preview rect
        RECT rc; GetClientRect(parentHwnd, &rc);
        SetWindowPos(childHwnd, nullptr, 0, 0,
                     rc.right - rc.left, rc.bottom - rc.top,
                     SWP_NOZORDER | SWP_SHOWWINDOW);
    }

    glfwMakeContextCurrent(g_window);
    glfwSetFramebufferSizeCallback(g_window, framebuffer_size_callback);
    glfwSetKeyCallback(g_window,         key_callback);
    glfwSetMouseButtonCallback(g_window, mouse_button_callback);
    glfwSetCursorPosCallback(g_window,   cursor_pos_callback);

    // Hide cursor in screensaver mode
    if (fullscreen)
        glfwSetInputMode(g_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwTerminate(); return false;
    }

    glViewport(0, 0, w, h);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    return true;
}

// ─── Shared render loop ────────────────────────────────────────────────────────
void runLoop(int w, int h) {
    logMsg("runLoop start: %d x %d", w, h);

    GlyphAtlas atlas;
    logMsg("Loading font msgothic.ttc...");
    if (!atlas.init("C:/Windows/Fonts/msgothic.ttc", 28)) {
        logMsg("msgothic failed, trying cour.ttf...");
        if (!atlas.init("C:/Windows/Fonts/cour.ttf", 28)) {
            logMsg("ERROR: Both fonts failed to load. Aborting.");
            return;
        }
    }
    logMsg("Font loaded OK. Glyph count: %d", (int)atlas.glyphs.size());

    ColumnManager columns;
    columns.init(w, h);
    logMsg("ColumnManager init OK. Columns: %d", (int)columns.columns.size());

    Renderer renderer;
    logMsg("Renderer init...");
    if (!renderer.init(w, h, &atlas)) {
        logMsg("ERROR: Renderer init failed (shader load?)");
        return;
    }
    logMsg("Renderer init OK.");

    PostProcessor post;
    logMsg("PostProcessor init...");
    if (!post.init(w, h)) {
        logMsg("ERROR: PostProcessor init failed (shader load?)");
        return;
    }
    logMsg("PostProcessor init OK. Entering main loop.");

    auto lastFrame = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(g_window)) {
        auto now    = std::chrono::high_resolution_clock::now();
        g_deltaTime = std::chrono::duration<float>(now - lastFrame).count();
        lastFrame   = now;
        g_time     += g_deltaTime;

        glfwPollEvents();
        columns.update(g_deltaTime, g_time);

        post.bindFramebuffer();
        glClearColor(0.0f, 0.0f, 0.03f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        renderer.draw(columns, g_time);
        post.unbindFramebuffer();

        glClear(GL_COLOR_BUFFER_BIT);
        post.render(g_time);

        glfwSwapBuffers(g_window);
    }

    renderer.destroy();
    post.destroy();
    atlas.destroy();
}

// ─── Entry point ──────────────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int) {
    logOpen();
    logMsg("CyberpunkWallpaper starting...");

    // Log working directory so we know where shaders are being looked for
    char cwd[512] = {};
    GetCurrentDirectoryA(512, cwd);
    logMsg("Working directory: %s", cwd);

    // Set working directory to the folder containing the .exe
    // This ensures shaders\ is always found regardless of how VS launches us
    char exePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string exeDir = exePath;
    auto lastSlash = exeDir.find_last_of("\\/");
    if (lastSlash != std::string::npos)
        exeDir = exeDir.substr(0, lastSlash);
    SetCurrentDirectoryA(exeDir.c_str());
    logMsg("Exe directory (new working dir): %s", exeDir.c_str());

    // Parse __argv / __argc (available in MSVC without a console entry point)
    parseArgs(__argc, __argv, g_mode, g_previewHwnd);
    logMsg("RunMode: %d", (int)g_mode);

    switch (g_mode) {

    // ── /c — Config dialog ────────────────────────────────────────────────────
    case RunMode::Configure:
        MessageBoxA(nullptr,
            "CyberpunkWallpaper Screensaver\n\n"
            "Edit constants at the top of main.cpp to configure:\n"
            "  TARGET_FPS, color palette (ColumnManager.h),\n"
            "  glitch intensity (post.frag)",
            "CyberpunkWallpaper - Settings",
            MB_OK | MB_ICONINFORMATION);
        return 0;

    // ── /p <hwnd> — Preview pane ──────────────────────────────────────────────
    case RunMode::Preview: {
        if (!g_previewHwnd) return 1;
        RECT rc; GetClientRect(g_previewHwnd, &rc);
        int pw = rc.right  - rc.left;
        int ph = rc.bottom - rc.top;
        if (pw < 1 || ph < 1) { pw = 200; ph = 150; }
        if (!initGL(pw, ph, false, true, g_previewHwnd)) return 1;
        runLoop(pw, ph);
        break;
    }

    // ── /s — Fullscreen screensaver ───────────────────────────────────────────
    case RunMode::Screensaver: {
        // Use primary monitor resolution
        RECT desktop;
        GetWindowRect(GetDesktopWindow(), &desktop);
        int sw = desktop.right  - desktop.left;
        int sh = desktop.bottom - desktop.top;
        if (!initGL(sw, sh, true, true)) return 1;
        runLoop(sw, sh);
        break;
    }

    // ── Normal / wallpaper mode ───────────────────────────────────────────────
    default: {
        // In Normal mode use actual monitor size, with title bar so it's closeable
        RECT desktop;
        SystemParametersInfoA(SPI_GETWORKAREA, 0, &desktop, 0); // excludes taskbar
        int sw = desktop.right  - desktop.left;
        int sh = desktop.bottom - desktop.top;
        if (!initGL(sw, sh, false, false)) return 1; // false = keep decorations
        if (PIN_TO_DESKTOP)
            Wallpaper::pinToDesktop(g_window);
        runLoop(sw, sh);
        break;
    }

    } // switch

    glfwTerminate();
    return 0;
}
