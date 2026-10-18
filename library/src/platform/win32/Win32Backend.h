#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <WebView2.h>
#include <wrl.h>
#include <wrl/event.h>

#include <functional>
#include <string>

class Win32Backend {
public:
    using MessageCallback = std::function<void(const std::string&)>;

    ~Win32Backend();
    Win32Backend() = default;
    Win32Backend(const Win32Backend&) = delete;
    Win32Backend& operator=(const Win32Backend&) = delete;

    bool Init(const char* title, int width, int height, MessageCallback on_message);
    bool Attach(HWND hwnd, bool transparent, MessageCallback on_message);
    bool Valid() const { return m_webview != nullptr && m_running; }
    bool Load(const std::string& file);
    void Show();
    bool PumpEvents();
    void Close();
    void PostMessage(const std::string& message);
    void ExecuteScript(const std::string& script);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    static LRESULT CALLBACK HostWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam,
        UINT_PTR subclass_id, DWORD_PTR reference_data);
    bool InitWebView(bool transparent);
    void Resize();
    bool WaitForCompletion(bool& done);

    HWND m_hwnd = nullptr;
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