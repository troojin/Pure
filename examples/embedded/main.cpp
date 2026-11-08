#include <Pure/Pure.h>

#include <iostream>
#include <string>

#if defined(_WIN32)
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include <cmath>

namespace {

struct SimState {
    bool wireframe = false;
    bool paused = false;
    float angle = 0.4f;
    int inspect_count = 0;
};

SimState g_sim;

void DrawScene(HDC hdc, int width, int height) {
    RECT rc = { 0, 0, width, height };
    HBRUSH bg = CreateSolidBrush(RGB(18, 20, 26));
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

    HPEN grid_pen = CreatePen(PS_SOLID, 1, RGB(30, 34, 45));
    HGDIOBJ old_pen = SelectObject(hdc, grid_pen);
    const int step = 40;
    for (int x = 0; x < width; x += step) {
        MoveToEx(hdc, x, 0, nullptr);
        LineTo(hdc, x, height);
    }
    for (int y = 0; y < height; y += step) {
        MoveToEx(hdc, 0, y, nullptr);
        LineTo(hdc, width, y);
    }

    const float cx = width * 0.58f;
    const float cy = height * 0.5f;
    const float size = (width < height ? width : height) * 0.22f;
    const float c = std::cos(g_sim.angle);
    const float s = std::sin(g_sim.angle);
    const float tilt = 0.45f;
    const float ct = std::cos(tilt);
    const float st = std::sin(tilt);

    POINT pts[8];
    const float verts[8][3] = {
        {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
        {-1, -1,  1}, {1, -1,  1}, {1, 1,  1}, {-1, 1,  1}
    };

    for (int i = 0; i < 8; ++i) {
        float x = verts[i][0] * c - verts[i][2] * s;
        float z = verts[i][0] * s + verts[i][2] * c;
        float y = verts[i][1] * ct - z * st;
        float zp = verts[i][1] * st + z * ct;
        float scale = 3.2f / (4.2f + zp * 0.45f);
        pts[i].x = static_cast<LONG>(cx + x * size * scale);
        pts[i].y = static_cast<LONG>(cy + y * size * scale);
    }

    if (!g_sim.wireframe) {
        HBRUSH face_brush = CreateSolidBrush(RGB(28, 48, 68));
        HPEN face_pen = CreatePen(PS_SOLID, 1, RGB(56, 189, 248));
        SelectObject(hdc, face_brush);
        SelectObject(hdc, face_pen);
        const int faces[6][4] = {
            {0, 1, 2, 3}, {4, 5, 6, 7},
            {0, 1, 5, 4}, {2, 3, 7, 6},
            {0, 3, 7, 4}, {1, 2, 6, 5}
        };
        for (const auto& f : faces) {
            POINT poly[4] = { pts[f[0]], pts[f[1]], pts[f[2]], pts[f[3]] };
            Polygon(hdc, poly, 4);
        }
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        DeleteObject(face_brush);
        DeleteObject(face_pen);
    }

    HPEN cube_pen = CreatePen(PS_SOLID, 2, g_sim.wireframe ? RGB(56, 189, 248) : RGB(125, 211, 252));
    SelectObject(hdc, cube_pen);

    const int edges[12][2] = {
        {0,1}, {1,2}, {2,3}, {3,0},
        {4,5}, {5,6}, {6,7}, {7,4},
        {0,4}, {1,5}, {2,6}, {3,7}
    };
    for (const auto& e : edges) {
        MoveToEx(hdc, pts[e[0]].x, pts[e[0]].y, nullptr);
        LineTo(hdc, pts[e[1]].x, pts[e[1]].y);
    }

    SelectObject(hdc, old_pen);
    DeleteObject(cube_pen);
    DeleteObject(grid_pen);
}

LRESULT CALLBACK HostWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        SetTimer(hwnd, 1, 16, nullptr);
        return 0;
    case WM_TIMER:
        if (!g_sim.paused) {
            g_sim.angle += 0.02f;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        if (w > 0 && h > 0) {
            HDC mem_dc = CreateCompatibleDC(hdc);
            HBITMAP mem_bmp = CreateCompatibleBitmap(hdc, w, h);
            HGDIOBJ old_bmp = SelectObject(mem_dc, mem_bmp);
            DrawScene(mem_dc, w, h);
            BitBlt(hdc, 0, 0, w, h, mem_dc, 0, 0, SRCCOPY);
            SelectObject(mem_dc, old_bmp);
            DeleteObject(mem_bmp);
            DeleteDC(mem_dc);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int command_show) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    const wchar_t class_name[] = L"PureEmbeddedExample";
    WNDCLASSW window_class = {};
    window_class.hInstance = instance;
    window_class.lpszClassName = class_name;
    window_class.lpfnWndProc = HostWindowProc;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = CreateSolidBrush(RGB(18, 20, 26));
    RegisterClassW(&window_class);

    HWND host = CreateWindowExW(0, class_name, L"Pure Embedded Overlay", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1100, 700, nullptr, nullptr, instance, nullptr);
    if (!host) return 1;
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(host, 20, &dark, sizeof(dark));
    ShowWindow(host, command_show);
    UpdateWindow(host);

    Pure::Context gui;
    if (!gui.attach(host) || !gui.load("web/index.html")) {
        DestroyWindow(host);
        return 1;
    }
    gui.on_message([&gui, host](const std::string& message) {
        if (message.find("\"wireframe\":true") != std::string::npos) g_sim.wireframe = true;
        if (message.find("\"wireframe\":false") != std::string::npos) g_sim.wireframe = false;
        if (message.find("\"paused\":true") != std::string::npos) g_sim.paused = true;
        if (message.find("\"paused\":false") != std::string::npos) g_sim.paused = false;
        if (message.find("\"inspect\"") != std::string::npos) ++g_sim.inspect_count;
        InvalidateRect(host, nullptr, FALSE);
        gui.post_message(
            std::string("Mode: ") + (g_sim.wireframe ? "Wireframe" : "Solid") +
            " | Sim: " + (g_sim.paused ? "Paused" : "Running") +
            " | Inspects: " + std::to_string(g_sim.inspect_count)
        );
    });

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return 0;
}

#elif defined(__APPLE__)
#include <objc/message.h>
#include <objc/runtime.h>

int main() {
    id app = ((id(*)(Class, SEL))objc_msgSend)(objc_getClass("NSApplication"), sel_registerName("sharedApplication"));
    ((void(*)(id, SEL, long))objc_msgSend)(app, sel_registerName("setActivationPolicy:"), 0);
    ((void(*)(id, SEL))objc_msgSend)(app, sel_registerName("finishLaunching"));

    struct CGRect { struct { double x, y; } origin; struct { double w, h; } size; };
    CGRect frame = { { 0, 0 }, { 1100, 700 } };
    id window = ((id(*)(Class, SEL))objc_msgSend)(objc_getClass("NSWindow"), sel_registerName("alloc"));
    window = ((id(*)(id, SEL, CGRect, unsigned long, unsigned long, bool))objc_msgSend)(
        window, sel_registerName("initWithContentRect:styleMask:backing:defer:"), frame, 15, 2, false);

    id title = ((id(*)(Class, SEL, const char*))objc_msgSend)(
        objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), "Pure Embedded Overlay");
    ((void(*)(id, SEL, id))objc_msgSend)(window, sel_registerName("setTitle:"), title);
    ((void(*)(id, SEL, id))objc_msgSend)(window, sel_registerName("makeKeyAndOrderFront:"), nullptr);

    Pure::Context gui;
    if (!gui.attach(window) || !gui.load("web/index.html")) {
        std::cerr << "Failed to attach Pure context.\n";
        return 1;
    }
    gui.on_message([&gui](const std::string& message) {
        std::cout << "JavaScript: " << message << '\n';
        gui.post_message("C++ received: " + message);
    });
    gui.run();
    return 0;
}

#else
#include <gtk/gtk.h>

int main(int argc, char** argv) {
    gtk_init(&argc, &argv);

    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Pure Embedded Overlay");
    gtk_window_set_default_size(GTK_WINDOW(window), 1100, 700);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);

    GtkWidget* overlay = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(window), overlay);

    GtkWidget* canvas = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(overlay), canvas);
    gtk_widget_show_all(window);

    Pure::Context gui;
    if (!gui.attach(overlay) || !gui.load("web/index.html")) {
        std::cerr << "Failed to attach Pure context.\n";
        return 1;
    }
    gui.on_message([&gui](const std::string& message) {
        std::cout << "JavaScript: " << message << '\n';
        gui.post_message("C++ received: " + message);
    });

    gtk_main();
    return 0;
}
#endif
