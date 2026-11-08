#include "Win32Backend.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <shlobj.h>

#include <filesystem>

using namespace Microsoft::WRL;

namespace Pure {

namespace {

std::wstring ToWide(const std::string& text) {
    if (text.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) return {};
    std::wstring result(length, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
    return result;
}

std::string ToUtf8(const wchar_t* text) {
    if (!text || !*text) return {};
    const int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1) return {};
    std::string result(length - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), length, nullptr, nullptr);
    return result;
}

std::filesystem::path ExePath() {
    wchar_t buffer[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(buffer);
}

std::filesystem::path ExeDir() {
    return ExePath().parent_path();
}

std::wstring UserDataFolder() {
    PWSTR local_app_data = nullptr;
    std::filesystem::path base_dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local_app_data)) && local_app_data) {
        base_dir = local_app_data;
        CoTaskMemFree(local_app_data);
    } else {
        std::error_code ec;
        base_dir = std::filesystem::temp_directory_path(ec);
    }
    std::wstring exe_name = ExePath().stem().wstring();
    if (exe_name.empty()) exe_name = L"app";
    std::filesystem::path folder = base_dir / L"Pure" / (exe_name + L".WebView2");
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    if (ec) return {};
    return folder.wstring();
}

void EnableHighDpi() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

void ApplyDarkTitleBar(HWND hwnd) {
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
}

bool IsUrl(const std::string& target) {
    return target.rfind("http://", 0) == 0 ||
           target.rfind("https://", 0) == 0 ||
           target.rfind("file://", 0) == 0 ||
           target.rfind("data:", 0) == 0;
}

const wchar_t* kBridgeScript =
    L"(function(){"
    L"if(window.Pure)return;"
    L"const listeners=[];"
    L"window.chrome.webview.addEventListener('message',function(e){"
    L"for(const fn of listeners){try{fn(e.data);}catch(err){console.error(err);}}"
    L"});"
    L"const send=function(msg){"
    L"window.chrome.webview.postMessage(typeof msg==='string'?msg:JSON.stringify(msg));"
    L"};"
    L"window.Pure={"
    L"send:send,"
    L"postMessage:send,"
    L"onMessage:function(fn){if(typeof fn==='function')listeners.push(fn);}"
    L"};"
    L"})();";

}

std::unique_ptr<Backend> Backend::create() {
    return std::make_unique<Win32Backend>();
}

LRESULT CALLBACK Win32Backend::WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* self = message == WM_NCCREATE
        ? static_cast<Win32Backend*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams)
        : reinterpret_cast<Win32Backend*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    switch (message) {
    case WM_SIZE:
        if (self) {
            if (wparam == SIZE_MINIMIZED) {
                if (self->m_controller) self->m_controller->put_IsVisible(FALSE);
            } else {
                if (self->m_controller) self->m_controller->put_IsVisible(TRUE);
                self->resize();
            }
        }
        return 0;
    case WM_SHOWWINDOW:
        if (self && self->m_controller) {
            if (wparam) {
                self->m_controller->put_IsVisible(FALSE);
                self->m_controller->put_IsVisible(TRUE);
                self->resize();
            } else {
                self->m_controller->put_IsVisible(FALSE);
            }
        }
        break;
    case WM_MOVE:
    case WM_MOVING:
        if (self && self->m_controller) {
            self->m_controller->NotifyParentWindowPositionChanged();
        }
        break;
    case WM_DPICHANGED:
        if (lparam) {
            const auto* suggested = reinterpret_cast<const RECT*>(lparam);
            SetWindowPos(hwnd, nullptr,
                suggested->left, suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return 0;
    case WM_ERASEBKGND:
        if (self && self->m_webview) {
            return 1;
        }
        break;
    case WM_DESTROY:
        if (self) {
            self->m_hwnd = nullptr;
            self->m_running = false;
        }
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT CALLBACK Win32Backend::HostWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam,
    UINT_PTR, DWORD_PTR reference_data) {
    auto* self = reinterpret_cast<Win32Backend*>(reference_data);
    if (self) {
        switch (message) {
        case WM_SIZE:
            if (wparam == SIZE_MINIMIZED) {
                if (self->m_controller) self->m_controller->put_IsVisible(FALSE);
            } else {
                if (self->m_controller) self->m_controller->put_IsVisible(TRUE);
                self->resize();
            }
            break;
        case WM_SHOWWINDOW:
            if (self->m_controller) {
                if (wparam) {
                    self->m_controller->put_IsVisible(FALSE);
                    self->m_controller->put_IsVisible(TRUE);
                    self->resize();
                } else {
                    self->m_controller->put_IsVisible(FALSE);
                }
            }
            break;
        case WM_MOVE:
        case WM_MOVING:
            if (self->m_controller) {
                self->m_controller->NotifyParentWindowPositionChanged();
            }
            break;
        case WM_NCDESTROY:
            self->m_hwnd = nullptr;
            self->m_running = false;
            self->m_subclassed = false;
            break;
        default:
            break;
        }
    }
    return DefSubclassProc(hwnd, message, wparam, lparam);
}

bool Win32Backend::wait_for_completion(bool& done) {
    MSG message;
    while (!done) {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                m_running = false;
                PostQuitMessage(static_cast<int>(message.wParam));
                return false;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
            if (done) return true;
        }
        if (done) break;
        MsgWaitForMultipleObjectsEx(0, nullptr, 16, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }
    return true;
}

bool Win32Backend::init(const char* title, int width, int height, MessageCallback on_message) {
    EnableHighDpi();

    const HRESULT com_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com_result) && com_result != RPC_E_CHANGED_MODE) return false;
    m_com_initialized = SUCCEEDED(com_result);
    m_on_message = std::move(on_message);

    m_bg_brush = CreateSolidBrush(RGB(18, 19, 24));

    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = WndProc;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = m_bg_brush;
    window_class.lpszClassName = L"PureWindow";
    RegisterClassExW(&window_class);

    RECT rect = { 0, 0, width > 0 ? width : 1024, height > 0 ? height : 768 };
    AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0);

    m_hwnd = CreateWindowExW(
        0,
        window_class.lpszClassName,
        ToWide(title ? title : "Pure").c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        window_class.hInstance,
        this
    );
    if (!m_hwnd) return false;

    ApplyDarkTitleBar(m_hwnd);
    m_owns_window = true;
    return init_webview(false);
}

bool Win32Backend::attach(void* native_handle, bool transparent, MessageCallback on_message) {
    HWND hwnd = static_cast<HWND>(native_handle);
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

    if (!init_webview(transparent)) {
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

bool Win32Backend::init_webview(bool transparent) {
    const std::wstring user_data = UserDataFolder();

    bool environment_done = false;
    HRESULT environment_result = E_FAIL;
    if (FAILED(CreateCoreWebView2EnvironmentWithOptions(
            nullptr,
            user_data.empty() ? nullptr : user_data.c_str(),
            nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [&](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                    environment_result = result;
                    if (SUCCEEDED(result)) m_environment = environment;
                    environment_done = true;
                    return S_OK;
                }).Get())) ||
        !wait_for_completion(environment_done) ||
        FAILED(environment_result) ||
        !m_environment) {
        return false;
    }

    bool controller_done = false;
    HRESULT controller_result = E_FAIL;
    if (FAILED(m_environment->CreateCoreWebView2Controller(
            m_hwnd,
            Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                [&](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                    controller_result = result;
                    if (SUCCEEDED(result) && controller) {
                        m_controller = controller;
                        m_controller->get_CoreWebView2(&m_webview);
                    }
                    controller_done = true;
                    return S_OK;
                }).Get())) ||
        !wait_for_completion(controller_done) ||
        FAILED(controller_result) ||
        !m_controller ||
        !m_webview) {
        return false;
    }

    ComPtr<ICoreWebView2Settings> settings;
    m_webview->get_Settings(&settings);
    if (settings) {
        settings->put_IsWebMessageEnabled(TRUE);
        settings->put_IsStatusBarEnabled(FALSE);
        settings->put_AreDevToolsEnabled(TRUE);
    }

    ComPtr<ICoreWebView2Controller2> controller2;
    if (SUCCEEDED(m_controller.As(&controller2))) {
        if (transparent) {
            controller2->put_DefaultBackgroundColor({ 0, 0, 0, 0 });
        } else {
            controller2->put_DefaultBackgroundColor({ 255, 18, 19, 24 });
        }
    }

    m_controller->put_IsVisible(TRUE);
    resize();

    bool script_done = false;
    m_webview->AddScriptToExecuteOnDocumentCreated(
        kBridgeScript,
        Callback<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler>(
            [&](HRESULT, LPCWSTR) -> HRESULT {
                script_done = true;
                return S_OK;
            }).Get());
    wait_for_completion(script_done);

    m_webview->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                wchar_t* message = nullptr;
                if (SUCCEEDED(args->TryGetWebMessageAsString(&message)) && message) {
                    if (m_on_message) m_on_message(ToUtf8(message));
                    CoTaskMemFree(message);
                }
                return S_OK;
            }).Get(),
        &m_message_token);

    m_running = true;
    return true;
}

void Win32Backend::resize() {
    if (!m_hwnd || !m_controller) return;
    RECT bounds = {};
    GetClientRect(m_hwnd, &bounds);
    m_controller->put_Bounds(bounds);
}

bool Win32Backend::load(const std::string& file) {
    if (!m_webview || file.empty()) return false;

    std::wstring target_url;
    if (IsUrl(file)) {
        target_url = ToWide(file);
    } else {
        std::filesystem::path page(ToWide(file));
        std::error_code ec;
        if (page.is_relative()) {
            std::filesystem::path from_cwd = std::filesystem::absolute(page, ec);
            if (std::filesystem::is_regular_file(from_cwd, ec)) {
                page = from_cwd;
            } else {
                std::filesystem::path from_exe = ExeDir() / page;
                if (std::filesystem::is_regular_file(from_exe, ec)) {
                    page = std::filesystem::absolute(from_exe, ec);
                }
            }
        } else {
            page = std::filesystem::absolute(page, ec);
        }

        if (!std::filesystem::is_regular_file(page, ec)) return false;

        ComPtr<ICoreWebView2_3> webview3;
        if (SUCCEEDED(m_webview.As(&webview3))) {
            webview3->SetVirtualHostNameToFolderMapping(
                L"pure.internal",
                page.parent_path().wstring().c_str(),
                COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
        }
        target_url = L"file:///" + page.generic_wstring();
    }

    bool navigation_done = false;
    BOOL navigation_success = FALSE;
    EventRegistrationToken token = {};
    m_webview->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [&](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                if (args) args->get_IsSuccess(&navigation_success);
                navigation_done = true;
                return S_OK;
            }).Get(),
        &token);

    if (FAILED(m_webview->Navigate(target_url.c_str()))) {
        m_webview->remove_NavigationCompleted(token);
        return false;
    }

    const bool completed = wait_for_completion(navigation_done);
    m_webview->remove_NavigationCompleted(token);
    return completed && navigation_success == TRUE;
}

bool Win32Backend::load_html(const std::string& html) {
    if (!m_webview) return false;

    bool navigation_done = false;
    BOOL navigation_success = FALSE;
    EventRegistrationToken token = {};
    m_webview->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [&](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                if (args) args->get_IsSuccess(&navigation_success);
                navigation_done = true;
                return S_OK;
            }).Get(),
        &token);

    if (FAILED(m_webview->NavigateToString(ToWide(html).c_str()))) {
        m_webview->remove_NavigationCompleted(token);
        return false;
    }

    const bool completed = wait_for_completion(navigation_done);
    m_webview->remove_NavigationCompleted(token);
    return completed && navigation_success == TRUE;
}

void Win32Backend::show() {
    if (!m_hwnd) return;
    ShowWindow(m_hwnd, SW_SHOWDEFAULT);
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    if (m_controller) {
        m_controller->put_IsVisible(FALSE);
        m_controller->put_IsVisible(TRUE);
        resize();
    }
}

bool Win32Backend::poll_events() {
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

void Win32Backend::run() {
    while (poll_events()) {
        if (!m_running) break;
        MsgWaitForMultipleObjectsEx(0, nullptr, 16, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }
}

void Win32Backend::close() {
    m_running = false;
    if (m_controller) m_controller->Close();
    if (m_subclassed && m_hwnd && IsWindow(m_hwnd)) {
        RemoveWindowSubclass(m_hwnd, HostWndProc, reinterpret_cast<UINT_PTR>(this));
        m_subclassed = false;
    }
    if (m_owns_window && m_hwnd && IsWindow(m_hwnd)) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void Win32Backend::post_message(const std::string& message) {
    if (m_webview) {
        m_webview->PostWebMessageAsString(ToWide(message).c_str());
    }
}

void Win32Backend::execute_script(const std::string& script) {
    if (m_webview) {
        m_webview->ExecuteScript(ToWide(script).c_str(), nullptr);
    }
}

void Win32Backend::set_title(const std::string& title) {
    if (m_hwnd) {
        SetWindowTextW(m_hwnd, ToWide(title).c_str());
    }
}

void Win32Backend::set_size(int width, int height) {
    if (!m_hwnd || width <= 0 || height <= 0) return;
    if (m_owns_window) {
        RECT rect = { 0, 0, width, height };
        const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(m_hwnd, GWL_STYLE));
        const DWORD ex_style = static_cast<DWORD>(GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE));
        AdjustWindowRectEx(&rect, style, FALSE, ex_style);
        SetWindowPos(m_hwnd, nullptr, 0, 0,
            rect.right - rect.left, rect.bottom - rect.top,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    } else {
        resize();
    }
}

Win32Backend::~Win32Backend() {
    if (m_webview) m_webview->remove_WebMessageReceived(m_message_token);
    m_webview.Reset();
    m_controller.Reset();
    m_environment.Reset();
    if (m_subclassed && m_hwnd && IsWindow(m_hwnd)) {
        RemoveWindowSubclass(m_hwnd, HostWndProc, reinterpret_cast<UINT_PTR>(this));
    }
    if (m_owns_window && m_hwnd && IsWindow(m_hwnd)) {
        DestroyWindow(m_hwnd);
    }
    if (m_bg_brush) {
        DeleteObject(m_bg_brush);
    }
    if (m_com_initialized) {
        CoUninitialize();
    }
}

}
