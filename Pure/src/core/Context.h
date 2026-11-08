#pragma once

#include <Pure/Pure.h>
#include "../platform/Backend.h"

#include <memory>

class Pure::Window::Impl {
public:
    Impl(const char* title, int width, int height);

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
    std::unique_ptr<Backend> m_backend;
    MessageHandler m_handler;
};

class Pure::Context::Impl {
public:
    Impl();

    bool attach(void* window, bool transparent);
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
    std::unique_ptr<Backend> m_backend;
    MessageHandler m_handler;
};
