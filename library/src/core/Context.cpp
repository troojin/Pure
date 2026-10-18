#include "Context.h"
#include "../platform/win32/Win32Backend.h"

#include <utility>

namespace Pure {

Window::Window(const char* title, int width, int height)
    : m_impl(std::make_unique<Impl>(title, width, height)) {}

Window::~Window() = default;
Window::Window(Window&&) noexcept = default;
Window& Window::operator=(Window&&) noexcept = default;

bool Window::valid() const { return m_impl && m_impl->valid(); }
bool Window::load(const std::string& file) { return m_impl && m_impl->load(file); }
void Window::show() { if (m_impl) m_impl->show(); }
bool Window::poll_events() { return m_impl && m_impl->poll_events(); }
void Window::close() { if (m_impl) m_impl->close(); }
void Window::post_message(const std::string& message) { if (m_impl) m_impl->post_message(message); }
void Window::execute_script(const std::string& script) { if (m_impl) m_impl->execute_script(script); }
void Window::on_message(MessageHandler handler) { if (m_impl) m_impl->on_message(std::move(handler)); }

void Window::run() {
    while (poll_events()) {
        WaitMessage();
    }
}

Window::Impl::Impl(const char* title, int width, int height)
    : m_backend(std::make_unique<Win32Backend>()) {
    m_backend->Init(title, width, height, [this](const std::string& message) {
        if (m_handler) m_handler(message);
    });
}

bool Window::Impl::valid() const { return m_backend && m_backend->Valid(); }
bool Window::Impl::load(const std::string& file) { return m_backend && m_backend->Load(file); }
void Window::Impl::show() { if (m_backend) m_backend->Show(); }
bool Window::Impl::poll_events() { return m_backend && m_backend->PumpEvents(); }
void Window::Impl::close() { if (m_backend) m_backend->Close(); }
void Window::Impl::post_message(const std::string& message) { if (m_backend) m_backend->PostMessage(message); }
void Window::Impl::execute_script(const std::string& script) { if (m_backend) m_backend->ExecuteScript(script); }
void Window::Impl::on_message(MessageHandler handler) { m_handler = std::move(handler); }

Context::Context() : m_impl(std::make_unique<Impl>()) {}
Context::~Context() = default;
Context::Context(Context&&) noexcept = default;
Context& Context::operator=(Context&&) noexcept = default;

bool Context::attach(HWND hwnd, bool transparent) { return m_impl && m_impl->attach(hwnd, transparent); }
bool Context::valid() const { return m_impl && m_impl->valid(); }
bool Context::load(const std::string& file) { return m_impl && m_impl->load(file); }
bool Context::poll_events() { return m_impl && m_impl->poll_events(); }
void Context::close() { if (m_impl) m_impl->close(); }
void Context::post_message(const std::string& message) { if (m_impl) m_impl->post_message(message); }
void Context::execute_script(const std::string& script) { if (m_impl) m_impl->execute_script(script); }
void Context::on_message(MessageHandler handler) { if (m_impl) m_impl->on_message(std::move(handler)); }

void Context::run() {
    while (poll_events()) {
        WaitMessage();
    }
}

Context::Impl::Impl() : m_backend(std::make_unique<Win32Backend>()) {}

bool Context::Impl::attach(HWND hwnd, bool transparent) {
    return m_backend && m_backend->Attach(hwnd, transparent, [this](const std::string& message) {
        if (m_handler) m_handler(message);
    });
}
bool Context::Impl::valid() const { return m_backend && m_backend->Valid(); }
bool Context::Impl::load(const std::string& file) { return m_backend && m_backend->Load(file); }
bool Context::Impl::poll_events() { return m_backend && m_backend->PumpEvents(); }
void Context::Impl::close() { if (m_backend) m_backend->Close(); }
void Context::Impl::post_message(const std::string& message) { if (m_backend) m_backend->PostMessage(message); }
void Context::Impl::execute_script(const std::string& script) { if (m_backend) m_backend->ExecuteScript(script); }
void Context::Impl::on_message(MessageHandler handler) { m_handler = std::move(handler); }

}