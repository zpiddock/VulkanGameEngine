#include "borg/window.hpp"
#include "federation/log.hpp"
#include <stdexcept>

namespace borg {

Window::Window(const Config& config) : m_config(config) {
    FED_INFO("Initializing GLFW");

    if (!::glfwInit()) {
        FED_FATAL("Failed to initialize GLFW");
        throw std::runtime_error("Failed to initialize GLFW");
    }

    FED_INFO("Creating window: \"{}\" ({}x{})", config.title, config.width, config.height);

    ::glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    ::glfwWindowHint(GLFW_MAXIMIZED, config.maximized ? GLFW_TRUE : GLFW_FALSE);
    ::glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);

    m_window = ::glfwCreateWindow(
        static_cast<int>(config.width),
        static_cast<int>(config.height),
        config.title.c_str(),
        nullptr,
        nullptr
    );

    if (!m_window) {
        ::glfwTerminate();
        FED_FATAL("Failed to create GLFW window");
        throw std::runtime_error("Failed to create GLFW window");
    }

    FED_DEBUG("Window created successfully");
}

Window::~Window() {
    if (m_window) {
        FED_DEBUG("Destroying window");
        ::glfwDestroyWindow(m_window);
        ::glfwTerminate();
        FED_DEBUG("Window successfully");
    }
}

Window::Window(Window&& other) noexcept
    : m_window(other.m_window), m_config(std::move(other.m_config)) {
    other.m_window = nullptr;
}

auto Window::operator=(Window&& other) noexcept -> Window& {
    if (this != &other) {
        if (m_window) {
            ::glfwDestroyWindow(m_window);
        }
        m_window = other.m_window;
        m_config = std::move(other.m_config);
        other.m_window = nullptr;
    }
    return *this;
}

auto Window::should_close() const -> bool {
    return ::glfwWindowShouldClose(m_window);
}

auto Window::get_native_handle() -> GLFWwindow* {
    return m_window;
}

auto Window::get_framebuffer_size() const -> std::pair<std::uint32_t, std::uint32_t> {
    int width, height;
    ::glfwGetFramebufferSize(m_window, &width, &height);
    return {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
}

auto Window::poll_events() -> void {
    ::glfwPollEvents();
}

auto Window::wait_events() -> void {
    ::glfwWaitEvents();
}

auto Window::set_input_mode(int mode, int value) -> void {

    ::glfwSetInputMode(m_window, mode, value);
}
} // namespace borg
