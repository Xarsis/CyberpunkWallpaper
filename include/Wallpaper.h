// Wallpaper.h — Win32 trick to parent GLFW window behind desktop icons

#pragma once
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Wallpaper {

    static HWND g_workerW = nullptr;

    static BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM) {
        HWND defView = FindWindowExA(hwnd, nullptr, "SHELLDLL_DefView", nullptr);
        if (defView)
            g_workerW = FindWindowExA(nullptr, hwnd, "WorkerW", nullptr);
        return TRUE;
    }

    // Call once before pinning any windows to spawn the WorkerW layer
    inline void init() {
        HWND progman = FindWindowA("Progman", nullptr);
        if (!progman) return;
        SendMessageTimeoutA(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, nullptr);
        g_workerW = nullptr;
        EnumWindows(enumWindowsProc, 0);
    }

    // Pin one GLFW window to the desktop at exact physical pixel coordinates.
    // x, y, w, h must be the monitor's physical pixel rect (from EnumDisplaySettings).
    inline void pinToDesktop(GLFWwindow* window, int x, int y, int w, int h) {
        if (!g_workerW) init();
        if (!g_workerW) {
            OutputDebugStringA("CyberpunkWallpaper: Could not find WorkerW\n");
            return;
        }

        HWND hwnd = glfwGetWin32Window(window);

        // Log WorkerW rect so we can see its coordinate space
        RECT workerRect = {};
        GetWindowRect(g_workerW, &workerRect);
        char buf[256];
        sprintf_s(buf, "WorkerW rect: (%d,%d)-(%d,%d) size(%dx%d)\n",
            workerRect.left, workerRect.top, workerRect.right, workerRect.bottom,
            workerRect.right - workerRect.left, workerRect.bottom - workerRect.top);
        OutputDebugStringA(buf);

        // Parent into WorkerW
        SetParent(hwnd, g_workerW);

        SetWindowLongA(hwnd, GWL_STYLE,
            GetWindowLongA(hwnd, GWL_STYLE) | WS_VISIBLE);

        // Coordinates after SetParent are relative to WorkerW client area.
        // Subtract WorkerW's own screen origin to convert from screen to client space.
        int relX = x - workerRect.left;
        int relY = y - workerRect.top;

        sprintf_s(buf, "Pinning hwnd to WorkerW: screen(%d,%d) -> relative(%d,%d) size(%dx%d)\n",
            x, y, relX, relY, w, h);
        OutputDebugStringA(buf);

        SetWindowPos(hwnd, HWND_BOTTOM,
            relX, relY, w, h,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    inline void restore() {
        if (g_workerW) {
            ShowWindow(g_workerW, SW_HIDE);
            g_workerW = nullptr;
        }
    }
}
#else
namespace Wallpaper {
    inline void init() {}
    inline void pinToDesktop(void*, int, int, int, int) {}
    inline void restore() {}
}
#endif
