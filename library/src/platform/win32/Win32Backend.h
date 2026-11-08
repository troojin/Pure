#pragma once

#include "../Backend.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wrl.h>
#include <wrl/event.h>
#include <WebView2.h>

#include <functional>
#include <string>

namespace Pure {

class Win32Backend final : public Backend {
public:
    Win32Backend() = default;
    ~Win32Backend() override;

    Win32Backend(const Win32Backend&) = delete;
    Win32Backend& operator=(const Win32Backend&) = delete;

    bool init(const char* title, int width, int height, MessageCallback on_message) override;
    bool attach(void* native_handle, bool transparent, MessageCallback on_message) override;
    bool valid() const override { return m_webview != nullptr && m_running; }
    bool load(const std::string& file) override;
    bool load_html(const std::string& html) override;
    void show() override;
    bool poll_events() override;
    void run() override;
    void close() override;
    void post_message(const std::string& message) override;
    void execute_script(const std::string& script) override;
    void set_title(const std::string& title) override;
    void set_size(int width, int height) override;
    void* native_handle() const override { return m_hwnd; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    static LRESULT CALLBACK HostWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam,
        UINT_PTR subclass_id, DWORD_PTR reference_data);

    bool init_webview(bool transparent);
    void resize();
    bool wait_for_completion(bool& done);

    HWND m_hwnd = nullptr;
    HBRUSH m_bg_brush = nullptr;
    Microsoft::WRL::ComPtr<ICoreWebView2Environment> m_environment;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> m_controller;
    Microsoft::WRL::ComPtr<ICoreWebView2> m_webview;
    EventRegistrationToken m_message_token = {};
    MessageCallback m_on_message;
    bool m_com_initialized = false;
    bool m_running = false;
    bool m_owns_window = false;
    bool m_subclassed = false;
};

}
