#pragma once

#include "../Backend.h"

#include <functional>
#include <string>

#ifdef __OBJC__
@class NSWindow;
@class NSView;
@class WKWebView;
@class PureScriptMessageHandler;
@class PureNavigationDelegate;
@class PureWindowDelegate;
#else
typedef void NSWindow;
typedef void NSView;
typedef void WKWebView;
typedef void PureScriptMessageHandler;
typedef void PureNavigationDelegate;
typedef void PureWindowDelegate;
#endif

namespace Pure {

class MacOSBackend final : public Backend {
public:
    MacOSBackend() = default;
    ~MacOSBackend() override;

    MacOSBackend(const MacOSBackend&) = delete;
    MacOSBackend& operator=(const MacOSBackend&) = delete;

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
    void* native_handle() const override;

    void handle_message(const std::string& message);
    void handle_window_close();

private:
    bool init_webview(NSView* parent_view, bool transparent);
    bool wait_for_navigation();

    NSWindow* m_window = nullptr;
    NSView* m_host_view = nullptr;
    WKWebView* m_webview = nullptr;
    PureScriptMessageHandler* m_script_handler = nullptr;
    PureNavigationDelegate* m_nav_delegate = nullptr;
    PureWindowDelegate* m_window_delegate = nullptr;
    MessageCallback m_on_message;
    bool m_running = false;
    bool m_owns_window = false;
};

}
