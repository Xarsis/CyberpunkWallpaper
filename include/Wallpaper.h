// Wallpaper.h — Win32 trick to parent GLFW window behind desktop icons
// Based on the well-known WorkerW / SHELLDLL_DefView technique

#pragma once
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Wallpaper {

    // Callback used by EnumWindows to find the WorkerW window
    static HWND g_workerW = nullptr;

    static BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam) {
        HWND defView = FindWindowExA(hwnd, nullptr, "SHELLDLL_DefView", nullptr);
        if (defView) {
            g_workerW = FindWindowExA(nullptr, hwnd, "WorkerW", nullptr);
        }
        return TRUE;
    }

    // ── Pin the GLFW window behind the desktop icons ──────────────────────────
    // Call once after glfwCreateWindow(). Sends the magic WM_SPAWN_WORKER_W
    // message to Progman so Windows creates a WorkerW layer, then re-parents
    // our HWND into it.
    inline void pinToDesktop(GLFWwindow* window) {
        HWND progman = FindWindowA("Progman", nullptr);
        if (!progman) return;

        // Tell Progman to spawn WorkerW
        SendMessageTimeoutA(progman, 0x052C, 0, 0,
            SMTO_NORMAL, 1000, nullptr);

        g_workerW = nullptr;
        EnumWindows(enumWindowsProc, 0);

        if (!g_workerW) {
            OutputDebugStringA("CyberpunkWallpaper: Could not find WorkerW\n");
            return;
        }

        HWND hwnd = glfwGetWin32Window(window);
        SetParent(hwnd, g_workerW);
        ShowWindow(hwnd, SW_MAXIMIZE);
    }

    // ── Restore the desktop to normal (call on shutdown) ─────────────────────
    inline void restore() {
        if (g_workerW) {
            ShowWindow(g_workerW, SW_HIDE);
            g_workerW = nullptr;
        }
    }
}
#else
// Stub for non-Windows builds
namespace Wallpaper {
    inline void pinToDesktop(void*) {}
    inline void restore() {}
}
#endif
