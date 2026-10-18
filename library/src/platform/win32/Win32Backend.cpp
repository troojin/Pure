#include "Win32Backend.h"

#include <commctrl.h>
#include <filesystem>

using namespace Microsoft::WRL;

namespace {

std::wstring ToWide(const std::string& text) {
    if (text.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(length, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
    return result;
}

std::string ToUtf8(const wchar_t* text) {
    if (!text || !*text) return {};
    const int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    std::string result(length, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), length, nullptr, nullptr);
    result.pop_back();
    return result;
}

}

LRESULT CALLBACK Win32Backend::WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* self = message == WM_NCCREATE
        ? static_cast<Win32Backend*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams)
        : reinterpret_cast<Win32Backend*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE) SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));

    switch (message) {
    case WM_SIZE:
        if (self) self->Resize();
        return 0;
    case WM_DESTROY:
        if (self) {
            self->m_hwnd = nullptr;
            self->m_running = false;
        }
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

LRESULT CALLBACK Win32Backend::HostWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam,
    UINT_PTR, DWORD_PTR reference_data) {
    auto* self = reinterpret_cast<Win32Backend*>(reference_data);
    if (message == WM_SIZE && self) self->Resize();
    if (message == WM_NCDESTROY && self) {
        self->m_hwnd = nullptr;
        self->m_running = false;
        self->m_subclassed = false;
    }
    return DefSubclassProc(hwnd, message, wparam, lparam);
}

bool Win32Backend::WaitForCompletion(bool& done) {
    MSG message;
    while (!done) {
        const int result = GetMessageW(&message, nullptr, 0, 0);
        if (result <= 0) {
            m_running = false;
            return false;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return true;
}

bool Win32Backend::Init(const char* title, int width, int height, MessageCallback on_message) {
    const HRESULT com_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com_result) && com_result != RPC_E_CHANGED_MODE) return false;
    m_com_initialized = SUCCEEDED(com_result);
    m_on_message = std::move(on_message);

    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = WndProc;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    window_class.lpszClassName = L"PureWindow";
    RegisterClassExW(&window_class);

    m_hwnd = CreateWindowExW(0, window_class.lpszClassName, ToWide(title ? title : "Pure").c_str(),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        nullptr, nullptr, window_class.hInstance, this);
    if (!m_hwnd) return false;

    m_owns_window = true;
    return InitWebView(false);
}

bool Win32Backend::Attach(HWND hwnd, bool transparent, MessageCallback on_message) {
    if (!hwnd || !IsWindow(hwnd) || m_hwnd) return false;

    const HRESULT com_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com_result) && com_result != RPC_E_CHANGED_MODE) return false;
    m_com_initialized = SUCCEEDED(com_result);
    m_on_message = std::move(on_message);
    m_hwnd = hwnd;

    if (!SetWindowSubclass(m_hwnd, HostWndProc, reinterpret_cast<UINT_PTR>(this), reinterpret_cast<DWORD_PTR>(this))) {
        m_hwnd = nullptr;
        if (m_com_initialized) {
            CoUninitialize();
            m_com_initialized = false;
        }
        return false;
    }
    m_subclassed = true;
    if (!InitWebView(transparent)) {
        RemoveWindowSubclass(m_hwnd, HostWndProc, reinterpret_cast<UINT_PTR>(this));
        m_subclassed = false;
        m_hwnd = nullptr;
        m_webview.Reset();
        m_controller.Reset();
        m_environment.Reset();
        if (m_com_initialized) {
            CoUninitialize();
            m_com_initialized = false;
        }
        return false;
    }
    return true;
}

bool Win32Backend::InitWebView(bool transparent) {

    bool environment_done = false;
    HRESULT environment_result = E_FAIL;
    if (FAILED(CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [&](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                    environment_result = result;
                    if (SUCCEEDED(result)) m_environment = environment;
                    environment_done = true;
                    return S_OK;
                }).Get())) || !WaitForCompletion(environment_done) || FAILED(environment_result)) return false;

    bool controller_done = false;
    HRESULT controller_result = E_FAIL;
    if (FAILED(m_environment->CreateCoreWebView2Controller(m_hwnd,
            Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                [&](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                    controller_result = result;
                    if (SUCCEEDED(result)) {
                        m_controller = controller;
                        m_controller->get_CoreWebView2(&m_webview);
                    }
                    controller_done = true;
                    return S_OK;
                }).Get())) || !WaitForCompletion(controller_done) || FAILED(controller_result) || !m_webview) return false;

    ComPtr<ICoreWebView2Settings> settings;
    m_webview->get_Settings(&settings);
    if (settings) {
        settings->put_IsWebMessageEnabled(TRUE);
        settings->put_IsStatusBarEnabled(FALSE);
    }

    if (transparent) {
        ComPtr<ICoreWebView2Controller2> controller2;
        if (SUCCEEDED(m_controller.As(&controller2))) {
            controller2->put_DefaultBackgroundColor({ 0, 0, 0, 0 });
        }
    }
    Resize();
    m_webview->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>(
        [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
            wchar_t* message = nullptr;
            if (SUCCEEDED(args->TryGetWebMessageAsString(&message)) && message) {
                if (m_on_message) m_on_message(ToUtf8(message));
                CoTaskMemFree(message);
            }
            return S_OK;
        }).Get(), &m_message_token);

    m_running = true;
    return true;
}

void Win32Backend::Resize() {
    if (!m_hwnd || !m_controller) return;
    RECT bounds;
    GetClientRect(m_hwnd, &bounds);
    m_controller->put_Bounds(bounds);
}

bool Win32Backend::Load(const std::string& file) {
    if (!m_webview) return false;
    const std::filesystem::path page = std::filesystem::absolute(std::filesystem::u8path(file));
    if (!std::filesystem::is_regular_file(page)) return false;

    ComPtr<ICoreWebView2_3> webview3;
    if (FAILED(m_webview.As(&webview3))) return false;
    if (FAILED(webview3->SetVirtualHostNameToFolderMapping(L"app.local", page.parent_path().wstring().c_str(),
            COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW))) return false;

    bool navigation_done = false;
    BOOL navigation_success = FALSE;
    EventRegistrationToken token;
    m_webview->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>(
        [&](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
            args->get_IsSuccess(&navigation_success);
            navigation_done = true;
            return S_OK;
        }).Get(), &token);
    const std::wstring url = L"https://app.local/" + page.filename().wstring();
    m_webview->Navigate(url.c_str());
    const bool completed = WaitForCompletion(navigation_done);
    m_webview->remove_NavigationCompleted(token);
    return completed && navigation_success == TRUE;
}

void Win32Backend::Show() { if (m_hwnd) ShowWindow(m_hwnd, SW_SHOW); }

bool Win32Backend::PumpEvents() {
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            m_running = false;
            return false;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return m_running;
}

void Win32Backend::Close() {
    m_running = false;
    if (m_controller) m_controller->Close();
    if (m_subclassed && m_hwnd && IsWindow(m_hwnd)) {
        RemoveWindowSubclass(m_hwnd, HostWndProc, reinterpret_cast<UINT_PTR>(this));
        m_subclassed = false;
    }
    if (m_owns_window && m_hwnd) DestroyWindow(m_hwnd);
}
void Win32Backend::PostMessage(const std::string& message) { if (m_webview) m_webview->PostWebMessageAsString(ToWide(message).c_str()); }
void Win32Backend::ExecuteScript(const std::string& script) { if (m_webview) m_webview->ExecuteScript(ToWide(script).c_str(), nullptr); }

Win32Backend::~Win32Backend() {
    if (m_webview) m_webview->remove_WebMessageReceived(m_message_token);
    m_webview.Reset();
    m_controller.Reset();
    m_environment.Reset();
    if (m_subclassed && m_hwnd && IsWindow(m_hwnd)) {
        RemoveWindowSubclass(m_hwnd, HostWndProc, reinterpret_cast<UINT_PTR>(this));
    }
    if (m_owns_window && m_hwnd && IsWindow(m_hwnd)) DestroyWindow(m_hwnd);
    if (m_com_initialized) CoUninitialize();
}