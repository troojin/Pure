#include "Context.h"

#include <utility>

namespace Pure {

Window::Window(const char* title, int width, int height)
    : m_impl(std::make_unique<Impl>(title, width, height)) {}

Window::~Window() = default;
Window::Window(Window&&) noexcept = default;
Window& Window::operator=(Window&&) noexcept = default;

bool Window::valid() const { return m_impl && m_impl->valid(); }
bool Window::load(const std::string& file) { return m_impl && m_impl->load(file); }
bool Window::load_html(const std::string& html) { return m_impl && m_impl->load_html(html); }
void Window::show() { if (m_impl) m_impl->show(); }
bool Window::poll_events() { return m_impl && m_impl->poll_events(); }
void Window::run() { if (m_impl) m_impl->run(); }
void Window::close() { if (m_impl) m_impl->close(); }
void Window::set_title(const std::string& title) { if (m_impl) m_impl->set_title(title); }
void Window::set_size(int width, int height) { if (m_impl) m_impl->set_size(width, height); }
NativeWindow Window::native_handle() const { return m_impl ? m_impl->native_handle() : nullptr; }
void Window::post_message(const std::string& message) { if (m_impl) m_impl->post_message(message); }
void Window::execute_script(const std::string& script) { if (m_impl) m_impl->execute_script(script); }
void Window::on_message(MessageHandler handler) { if (m_impl) m_impl->on_message(std::move(handler)); }

Window::Impl::Impl(const char* title, int width, int height)
    : m_backend(Backend::create()) {
    if (m_backend) {
        m_backend->init(title, width, height, [this](const std::string& message) {
            if (m_handler) m_handler(message);
        });
    }
}

bool Window::Impl::valid() const { return m_backend && m_backend->valid(); }
bool Window::Impl::load(const std::string& file) { return m_backend && m_backend->load(file); }
bool Window::Impl::load_html(const std::string& html) { return m_backend && m_backend->load_html(html); }
void Window::Impl::show() { if (m_backend) m_backend->show(); }
bool Window::Impl::poll_events() { return m_backend && m_backend->poll_events(); }
void Window::Impl::run() { if (m_backend) m_backend->run(); }
void Window::Impl::close() { if (m_backend) m_backend->close(); }
void Window::Impl::set_title(const std::string& title) { if (m_backend) m_backend->set_title(title); }
void Window::Impl::set_size(int width, int height) { if (m_backend) m_backend->set_size(width, height); }
NativeWindow Window::Impl::native_handle() const {
    return m_backend ? static_cast<NativeWindow>(m_backend->native_handle()) : nullptr;
}
void Window::Impl::post_message(const std::string& message) { if (m_backend) m_backend->post_message(message); }
void Window::Impl::execute_script(const std::string& script) { if (m_backend) m_backend->execute_script(script); }
void Window::Impl::on_message(MessageHandler handler) { m_handler = std::move(handler); }

Context::Context() : m_impl(std::make_unique<Impl>()) {}
Context::~Context() = default;
Context::Context(Context&&) noexcept = default;
Context& Context::operator=(Context&&) noexcept = default;

bool Context::attach(NativeWindow window, bool transparent) {
    return m_impl && m_impl->attach(static_cast<void*>(window), transparent);
}
#if defined(_WIN32)
bool Context::attach(void* window, bool transparent) {
    return m_impl && m_impl->attach(window, transparent);
}
#endif
bool Context::valid() const { return m_impl && m_impl->valid(); }
bool Context::load(const std::string& file) { return m_impl && m_impl->load(file); }
bool Context::load_html(const std::string& html) { return m_impl && m_impl->load_html(html); }
bool Context::poll_events() { return m_impl && m_impl->poll_events(); }
void Context::run() { if (m_impl) m_impl->run(); }
void Context::close() { if (m_impl) m_impl->close(); }
NativeWindow Context::native_handle() const { return m_impl ? m_impl->native_handle() : nullptr; }
void Context::post_message(const std::string& message) { if (m_impl) m_impl->post_message(message); }
void Context::execute_script(const std::string& script) { if (m_impl) m_impl->execute_script(script); }
void Context::on_message(MessageHandler handler) { if (m_impl) m_impl->on_message(std::move(handler)); }

Context::Impl::Impl() : m_backend(Backend::create()) {}

bool Context::Impl::attach(void* window, bool transparent) {
    return m_backend && m_backend->attach(window, transparent, [this](const std::string& message) {
        if (m_handler) m_handler(message);
    });
}
bool Context::Impl::valid() const { return m_backend && m_backend->valid(); }
bool Context::Impl::load(const std::string& file) { return m_backend && m_backend->load(file); }
bool Context::Impl::load_html(const std::string& html) { return m_backend && m_backend->load_html(html); }
bool Context::Impl::poll_events() { return m_backend && m_backend->poll_events(); }
void Context::Impl::run() { if (m_backend) m_backend->run(); }
void Context::Impl::close() { if (m_backend) m_backend->close(); }
NativeWindow Context::Impl::native_handle() const {
    return m_backend ? static_cast<NativeWindow>(m_backend->native_handle()) : nullptr;
}
void Context::Impl::post_message(const std::string& message) { if (m_backend) m_backend->post_message(message); }
void Context::Impl::execute_script(const std::string& script) { if (m_backend) m_backend->execute_script(script); }
void Context::Impl::on_message(MessageHandler handler) { m_handler = std::move(handler); }

}
