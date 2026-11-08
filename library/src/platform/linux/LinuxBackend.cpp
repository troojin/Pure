#include "LinuxBackend.h"

#include <cstdio>
#include <filesystem>
#include <limits.h>
#include <unistd.h>

namespace Pure {

namespace {

std::filesystem::path ExeDir() {
    char buffer[PATH_MAX] = {};
    const ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len <= 0) return {};
    buffer[len] = '\0';
    return std::filesystem::path(buffer).parent_path();
}

bool IsUrl(const std::string& target) {
    return target.rfind("http://", 0) == 0 ||
           target.rfind("https://", 0) == 0 ||
           target.rfind("file://", 0) == 0 ||
           target.rfind("data:", 0) == 0;
}

std::string EscapeJsString(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 16);
    out.push_back('"');
    for (unsigned char c : input) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"':  out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char hex[7];
                std::snprintf(hex, sizeof(hex), "\\u%04x", c);
                out += hex;
            } else {
                out.push_back(static_cast<char>(c));
            }
            break;
        }
    }
    out.push_back('"');
    return out;
}

const char* kBridgeScript =
    "(function(){"
    "if(window.Pure)return;"
    "const listeners=[];"
    "window.__pureDispatch=function(data){"
    "for(const fn of listeners){try{fn(data);}catch(err){console.error(err);}}"
    "};"
    "const send=function(msg){"
    "const payload=typeof msg==='string'?msg:JSON.stringify(msg);"
    "if(window.webkit&&window.webkit.messageHandlers&&window.webkit.messageHandlers.pure){"
    "window.webkit.messageHandlers.pure.postMessage(payload);"
    "}"
    "};"
    "window.Pure={"
    "send:send,"
    "postMessage:send,"
    "onMessage:function(fn){if(typeof fn==='function')listeners.push(fn);}"
    "};"
    "if(!window.chrome)window.chrome={};"
    "if(!window.chrome.webview){"
    "window.chrome.webview={"
    "postMessage:send,"
    "addEventListener:function(type,fn){"
    "if(type==='message'&&typeof fn==='function'){"
    "listeners.push(function(data){fn({data:data});});"
    "}"
    "},"
    "removeEventListener:function(){}"
    "};"
    "}"
    "})();";

}

std::unique_ptr<Backend> Backend::create() {
    return std::make_unique<LinuxBackend>();
}

void LinuxBackend::OnWindowDestroy(GtkWidget*, gpointer user_data) {
    auto* self = static_cast<LinuxBackend*>(user_data);
    if (self) {
        self->m_window = nullptr;
        self->m_webview = nullptr;
        self->m_running = false;
    }
}

void LinuxBackend::OnScriptMessage(WebKitUserContentManager*, WebKitJavascriptResult* result, gpointer user_data) {
    auto* self = static_cast<LinuxBackend*>(user_data);
    if (!self || !result || !self->m_on_message) return;

    JSCValue* value = webkit_javascript_result_get_js_value(result);
    if (!value) return;

    gchar* str = jsc_value_to_string(value);
    if (str) {
        self->m_on_message(std::string(str));
        g_free(str);
    }
}

void LinuxBackend::OnLoadChanged(WebKitWebView*, WebKitLoadEvent load_event, gpointer user_data) {
    auto* self = static_cast<LinuxBackend*>(user_data);
    if (!self) return;
    if (load_event == WEBKIT_LOAD_FINISHED) {
        self->m_nav_success = true;
        self->m_nav_done = true;
    }
}

gboolean LinuxBackend::OnLoadFailed(WebKitWebView*, WebKitLoadEvent, gchar*, GError*, gpointer user_data) {
    auto* self = static_cast<LinuxBackend*>(user_data);
    if (self) {
        self->m_nav_success = false;
        self->m_nav_done = true;
    }
    return FALSE;
}

bool LinuxBackend::init(const char* title, int width, int height, MessageCallback on_message) {
    if (!gtk_init_check(nullptr, nullptr)) return false;

    m_on_message = std::move(on_message);
    m_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    if (!m_window) return false;

    gtk_window_set_title(GTK_WINDOW(m_window), title ? title : "Pure");
    gtk_window_set_default_size(GTK_WINDOW(m_window), width > 0 ? width : 1024, height > 0 ? height : 768);
    gtk_window_set_position(GTK_WINDOW(m_window), GTK_WIN_POS_CENTER);
    g_signal_connect(m_window, "destroy", G_CALLBACK(OnWindowDestroy), this);

    m_owns_window = true;
    return init_webview(m_window, false);
}

bool LinuxBackend::attach(void* native_handle, bool transparent, MessageCallback on_message) {
    if (!native_handle || m_webview) return false;
    if (!gtk_init_check(nullptr, nullptr)) return false;

    GtkWidget* widget = static_cast<GtkWidget*>(native_handle);
    if (!GTK_IS_WIDGET(widget)) return false;

    m_on_message = std::move(on_message);
    m_host_widget = widget;
    m_owns_window = false;

    if (transparent) {
        GdkScreen* screen = gtk_widget_get_screen(widget);
        if (screen) {
            GdkVisual* visual = gdk_screen_get_rgba_visual(screen);
            if (visual) gtk_widget_set_visual(widget, visual);
        }
        gtk_widget_set_app_paintable(widget, TRUE);
    }

    return init_webview(widget, transparent);
}

bool LinuxBackend::init_webview(GtkWidget* container, bool transparent) {
    m_content_manager = webkit_user_content_manager_new();
    if (!m_content_manager) return false;

    WebKitUserScript* script = webkit_user_script_new(
        kBridgeScript,
        WEBKIT_USER_SCRIPT_INJECT_ALL_FRAMES,
        WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
        nullptr,
        nullptr
    );
    webkit_user_content_manager_add_script(m_content_manager, script);
    webkit_user_script_unref(script);

    g_signal_connect(m_content_manager, "script-message-received::pure", G_CALLBACK(OnScriptMessage), this);
    webkit_user_content_manager_register_script_message_handler(m_content_manager, "pure");

    m_webview = webkit_web_view_new_with_user_content_manager(m_content_manager);
    if (!m_webview) return false;

    WebKitSettings* settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(m_webview));
    if (settings) {
        webkit_settings_set_enable_developer_extras(settings, TRUE);
        webkit_settings_set_allow_file_access_from_file_urls(settings, TRUE);
        webkit_settings_set_allow_universal_access_from_file_urls(settings, TRUE);
    }

    GdkRGBA bg = transparent
        ? GdkRGBA{ 0.0, 0.0, 0.0, 0.0 }
        : GdkRGBA{ 18.0 / 255.0, 19.0 / 255.0, 24.0 / 255.0, 1.0 };
    webkit_web_view_set_background_color(WEBKIT_WEB_VIEW(m_webview), &bg);

    g_signal_connect(m_webview, "load-changed", G_CALLBACK(OnLoadChanged), this);
    g_signal_connect(m_webview, "load-failed", G_CALLBACK(OnLoadFailed), this);

    gtk_widget_set_hexpand(m_webview, TRUE);
    gtk_widget_set_vexpand(m_webview, TRUE);

    if (GTK_IS_OVERLAY(container)) {
        gtk_overlay_add_overlay(GTK_OVERLAY(container), m_webview);
    } else if (GTK_IS_CONTAINER(container)) {
        gtk_container_add(GTK_CONTAINER(container), m_webview);
    } else {
        return false;
    }

    gtk_widget_show(m_webview);
    m_running = true;
    return true;
}

bool LinuxBackend::wait_for_navigation() {
    while (!m_nav_done && m_running) {
        gtk_main_iteration_do(TRUE);
    }
    return m_nav_done && m_nav_success;
}

bool LinuxBackend::load(const std::string& file) {
    if (!m_webview || file.empty()) return false;

    m_nav_done = false;
    m_nav_success = false;

    if (IsUrl(file)) {
        webkit_web_view_load_uri(WEBKIT_WEB_VIEW(m_webview), file.c_str());
        return wait_for_navigation();
    }

    std::filesystem::path page(file);
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

    gchar* uri = g_filename_to_uri(page.string().c_str(), nullptr, nullptr);
    if (!uri) return false;

    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(m_webview), uri);
    g_free(uri);
    return wait_for_navigation();
}

bool LinuxBackend::load_html(const std::string& html) {
    if (!m_webview) return false;

    m_nav_done = false;
    m_nav_success = false;
    webkit_web_view_load_html(WEBKIT_WEB_VIEW(m_webview), html.c_str(), nullptr);
    return wait_for_navigation();
}

void LinuxBackend::show() {
    if (m_window) {
        gtk_widget_show_all(m_window);
    } else if (m_webview) {
        gtk_widget_show_all(m_webview);
    }
}

bool LinuxBackend::poll_events() {
    while (gtk_events_pending()) {
        gtk_main_iteration_do(FALSE);
    }
    return m_running;
}

void LinuxBackend::run() {
    while (m_running) {
        gtk_main_iteration_do(TRUE);
    }
}

void LinuxBackend::close() {
    m_running = false;
    if (m_owns_window && m_window) {
        GtkWidget* win = m_window;
        m_window = nullptr;
        m_webview = nullptr;
        gtk_widget_destroy(win);
    } else if (!m_owns_window && m_webview) {
        GtkWidget* wv = m_webview;
        m_webview = nullptr;
        gtk_widget_destroy(wv);
    }
}

void LinuxBackend::post_message(const std::string& message) {
    if (!m_webview) return;
    const std::string js = "window.__pureDispatch && window.__pureDispatch(" + EscapeJsString(message) + ");";
    execute_script(js);
}

void LinuxBackend::execute_script(const std::string& script) {
    if (!m_webview || script.empty()) return;
#if WEBKIT_CHECK_VERSION(2, 40, 0)
    webkit_web_view_evaluate_javascript(
        WEBKIT_WEB_VIEW(m_webview),
        script.c_str(),
        static_cast<gssize>(script.size()),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );
#else
    webkit_web_view_run_javascript(WEBKIT_WEB_VIEW(m_webview), script.c_str(), nullptr, nullptr, nullptr);
#endif
}

void LinuxBackend::set_title(const std::string& title) {
    if (m_window && GTK_IS_WINDOW(m_window)) {
        gtk_window_set_title(GTK_WINDOW(m_window), title.c_str());
    }
}

void LinuxBackend::set_size(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (m_window && GTK_IS_WINDOW(m_window)) {
        gtk_window_resize(GTK_WINDOW(m_window), width, height);
    }
}

LinuxBackend::~LinuxBackend() {
    close();
    if (m_content_manager) {
        g_object_unref(m_content_manager);
        m_content_manager = nullptr;
    }
}

}
