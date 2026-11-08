#pragma once

#include <functional>
#include <memory>
#include <string>

#if defined(_WIN32)
struct HWND__;
using HWND = HWND__*;
#endif

namespace Pure {

#if defined(_WIN32)
using NativeWindow = HWND;
#else
using NativeWindow = void*;
#endif

class Window {
public:
    using MessageHandler = std::function<void(const std::string& message)>;

    Window(const char* title, int width = 1024, int height = 768);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) noexcept;
    Window& operator=(Window&&) noexcept;

    bool valid() const;
    bool load(const std::string& file);
    bool load_html(const std::string& html);
    void show();
    bool poll_events();
    void run();
    void close();

    void set_title(const std::string& title);
    void set_size(int width, int height);
    NativeWindow native_handle() const;

    void post_message(const std::string& message);
    void execute_script(const std::string& script);
    void on_message(MessageHandler handler);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class Context {
public:
    using MessageHandler = std::function<void(const std::string& message)>;

    Context();
    ~Context();

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&) noexcept;
    Context& operator=(Context&&) noexcept;

    bool attach(NativeWindow window, bool transparent = true);
#if defined(_WIN32)
    bool attach(void* window, bool transparent = true);
#endif
    bool valid() const;
    bool load(const std::string& file);
    bool load_html(const std::string& html);
    bool poll_events();
    void run();
    void close();

    NativeWindow native_handle() const;

    void post_message(const std::string& message);
    void execute_script(const std::string& script);
    void on_message(MessageHandler handler);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

}
