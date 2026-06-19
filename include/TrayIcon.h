// TrayIcon.h — Win32 system tray icon with right-click context menu
// Runs on a background thread so it doesn't block the render loop.

#pragma once
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <thread>
#include <atomic>

namespace TrayIcon {

    // ── Menu item IDs ─────────────────────────────────────────────────────────
    static const UINT ID_TRAY_PAUSE  = 1001;
    static const UINT ID_TRAY_EXIT   = 1002;
    static const UINT WM_TRAY        = WM_USER + 1;
    static const UINT TRAY_ICON_ID   = 1;

    // ── Shared state (written by tray thread, read by render loop) ────────────
    std::atomic<bool> g_exitRequested  { false };
    std::atomic<bool> g_paused         { false };

    // ── Internal state ────────────────────────────────────────────────────────
    static HWND            g_trayHwnd  = nullptr;
    static NOTIFYICONDATAA g_nid       = {};
    static std::thread     g_thread;

    // ── Build a simple 16x16 green matrix icon programmatically ──────────────
    // (No .ico file needed — we draw it with GDI)
    static HICON createMatrixIcon() {
        const int SZ = 32;
        HDC screenDC = GetDC(nullptr);
        HDC memDC    = CreateCompatibleDC(screenDC);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = SZ;
        bmi.bmiHeader.biHeight      = -SZ; // top-down
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        HBITMAP bmp  = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        HBITMAP mask = CreateBitmap(SZ, SZ, 1, 1, nullptr);
        SelectObject(memDC, bmp);

        // Fill black background
        HBRUSH bgBrush = CreateSolidBrush(RGB(0, 0, 0));
        RECT rc = { 0, 0, SZ, SZ };
        FillRect(memDC, &rc, bgBrush);
        DeleteObject(bgBrush);

        // Draw a simple "> _" cursor symbol in neon green
        HPEN pen = CreatePen(PS_SOLID, 2, RGB(0, 220, 80));
        SelectObject(memDC, pen);
        SelectObject(memDC, GetStockObject(NULL_BRUSH));

        // ">" chevron
        MoveToEx(memDC,  6, 10, nullptr); LineTo(memDC, 14, 16);
        MoveToEx(memDC, 14, 16, nullptr); LineTo(memDC,  6, 22);

        // "_" underbar (cursor blink suggestion)
        MoveToEx(memDC, 16, 22, nullptr); LineTo(memDC, 26, 22);

        DeleteObject(pen);
        DeleteDC(memDC);
        ReleaseDC(nullptr, screenDC);

        ICONINFO ii  = {};
        ii.fIcon     = TRUE;
        ii.hbmMask   = mask;
        ii.hbmColor  = bmp;
        HICON icon   = CreateIconIndirect(&ii);

        DeleteObject(bmp);
        DeleteObject(mask);
        return icon;
    }

    // ── Window procedure for the hidden tray message window ───────────────────
    static LRESULT CALLBACK trayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        if (msg == WM_TRAY) {
            if (lp == WM_RBUTTONUP || lp == WM_LBUTTONUP) {
                // Build context menu
                HMENU menu = CreatePopupMenu();

                // Pause/Resume toggle
                const char* pauseLabel = g_paused.load() ? "Resume" : "Pause";
                AppendMenuA(menu, MF_STRING, ID_TRAY_PAUSE, pauseLabel);
                AppendMenuA(menu, MF_SEPARATOR, 0, nullptr);
                AppendMenuA(menu, MF_STRING, ID_TRAY_EXIT, "Exit");

                // Show menu at cursor position
                POINT pt; GetCursorPos(&pt);
                SetForegroundWindow(hwnd);
                int cmd = TrackPopupMenu(menu,
                    TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTALIGN | TPM_BOTTOMALIGN,
                    pt.x, pt.y, 0, hwnd, nullptr);
                DestroyMenu(menu);

                if (cmd == ID_TRAY_PAUSE)
                    g_paused = !g_paused.load();
                else if (cmd == ID_TRAY_EXIT)
                    g_exitRequested = true;
            }
            return 0;
        }
        if (msg == WM_DESTROY) {
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcA(hwnd, msg, wp, lp);
    }

    // ── Tray thread: creates hidden window + icon, runs message loop ──────────
    static void trayThreadFunc() {
        // Register window class
        WNDCLASSA wc   = {};
        wc.lpfnWndProc = trayWndProc;
        wc.hInstance   = GetModuleHandleA(nullptr);
        wc.lpszClassName = "CWP_TrayClass";
        RegisterClassA(&wc);

        // Create invisible message-only window
        g_trayHwnd = CreateWindowExA(0, "CWP_TrayClass", "CyberpunkWallpaper Tray",
            0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);

        if (!g_trayHwnd) return;

        // Add tray icon
        HICON icon = createMatrixIcon();

        ZeroMemory(&g_nid, sizeof(g_nid));
        g_nid.cbSize           = sizeof(NOTIFYICONDATAA);
        g_nid.hWnd             = g_trayHwnd;
        g_nid.uID              = TRAY_ICON_ID;
        g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        g_nid.uCallbackMessage = WM_TRAY;
        g_nid.hIcon            = icon;
        strcpy_s(g_nid.szTip, "CyberpunkWallpaper");
        Shell_NotifyIconA(NIM_ADD, &g_nid);

        // Message loop
        MSG msg;
        while (GetMessageA(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);

            // Exit loop when main signals shutdown
            if (g_exitRequested.load()) break;
        }

        // Cleanup
        Shell_NotifyIconA(NIM_DELETE, &g_nid);
        DestroyIcon(icon);
        DestroyWindow(g_trayHwnd);
        g_trayHwnd = nullptr;
    }

    // ── Public API ────────────────────────────────────────────────────────────

    inline void start() {
        g_thread = std::thread(trayThreadFunc);
    }

    inline void stop() {
        g_exitRequested = true;
        if (g_trayHwnd)
            PostMessageA(g_trayHwnd, WM_QUIT, 0, 0);
        if (g_thread.joinable())
            g_thread.join();
    }

    inline bool exitRequested() { return g_exitRequested.load(); }
    inline bool isPaused()      { return g_paused.load(); }
}

#else
namespace TrayIcon {
    inline void start() {}
    inline void stop()  {}
    inline bool exitRequested() { return false; }
    inline bool isPaused()      { return false; }
}
#endif
