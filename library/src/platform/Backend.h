#pragma once

#include <functional>
#include <memory>
#include <string>

namespace Pure {

class Backend {
public:
    using MessageCallback = std::function<void(const std::string&)>;

    virtual ~Backend() = default;

    virtual bool init(const char* title, int width, int height, MessageCallback on_message) = 0;
    virtual bool attach(void* native_handle, bool transparent, MessageCallback on_message) = 0;
    virtual bool valid() const = 0;
    virtual bool load(const std::string& file) = 0;
    virtual bool load_html(const std::string& html) = 0;
    virtual void show() = 0;
    virtual bool poll_events() = 0;
    virtual void run() = 0;
    virtual void close() = 0;
    virtual void post_message(const std::string& message) = 0;
    virtual void execute_script(const std::string& script) = 0;
    virtual void set_title(const std::string& title) = 0;
    virtual void set_size(int width, int height) = 0;
    virtual void* native_handle() const = 0;

    static std::unique_ptr<Backend> create();
};

}
