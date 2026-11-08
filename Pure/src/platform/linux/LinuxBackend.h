#pragma once

#include "../Backend.h"

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#include <functional>
#include <string>

namespace Pure {

class LinuxBackend final : public Backend {
public:
    LinuxBackend() = default;
    ~LinuxBackend() override;

    LinuxBackend(const LinuxBackend&) = delete;
    LinuxBackend& operator=(const LinuxBackend&) = delete;

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
    void* native_handle() const override { return m_window ? m_window : m_host_widget; }

private:
    static void OnWindowDestroy(GtkWidget* widget, gpointer user_data);
    static void OnScriptMessage(WebKitUserContentManager* manager, WebKitJavascriptResult* result, gpointer user_data);
    static void OnLoadChanged(WebKitWebView* web_view, WebKitLoadEvent load_event, gpointer user_data);
    static gboolean OnLoadFailed(WebKitWebView* web_view, WebKitLoadEvent load_event, gchar* failing_uri, GError* error, gpointer user_data);

    bool init_webview(GtkWidget* container, bool transparent);
    bool wait_for_navigation();

    GtkWidget* m_window = nullptr;
    GtkWidget* m_host_widget = nullptr;
    GtkWidget* m_webview = nullptr;
    WebKitUserContentManager* m_content_manager = nullptr;
    MessageCallback m_on_message;
    bool m_running = false;
    bool m_owns_window = false;
    bool m_nav_done = false;
    bool m_nav_success = false;
};

}
