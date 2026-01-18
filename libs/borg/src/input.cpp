#include "borg/input.hpp"
#include "borg/window.hpp"
#include <algorithm>

namespace borg {
    Input::Input(Window &window)
        : m_window(window.get_native_handle())
          , m_context{window, *this} {
        // Set the window user pointer to our context
        ::glfwSetWindowUserPointer(m_window, &m_context);

        // Register GLFW callbacks
        ::glfwSetKeyCallback(m_window, key_callback);
        ::glfwSetCursorPosCallback(m_window, cursor_callback);
        ::glfwSetMouseButtonCallback(m_window, mouse_button_callback);
        ::glfwSetScrollCallback(m_window, scroll_callback);
    }

    Input::~Input() {
        // Unregister callbacks
        ::glfwSetKeyCallback(m_window, nullptr);
        ::glfwSetCursorPosCallback(m_window, nullptr);
        ::glfwSetMouseButtonCallback(m_window, nullptr);
        ::glfwSetScrollCallback(m_window, nullptr);
    }

    auto Input::is_key_pressed(int key) const -> bool {
        return ::glfwGetKey(m_window, key) == GLFW_PRESS;
    }

    auto Input::is_mouse_button_pressed(int button) const -> bool {
        return ::glfwGetMouseButton(m_window, button) == GLFW_PRESS;
    }

    auto Input::get_cursor_position() const -> std::pair<double, double> {
        double x, y;
        ::glfwGetCursorPos(m_window, &x, &y);
        return {x, y};
    }

    auto Input::add_subscriber(IInputSubscriber *subscriber) -> void {
        m_subscribers.push_back(subscriber);
    }

    auto Input::remove_subscriber(IInputSubscriber *subscriber) -> void {
        auto it = std::find(m_subscribers.begin(), m_subscribers.end(), subscriber);
        if (it != m_subscribers.end()) {
            m_subscribers.erase(it);
        }
    }

    void Input::key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
        auto *ctx = reinterpret_cast<WindowContext *>(::glfwGetWindowUserPointer(window));
        if (!ctx) return;

        // Call pre-callback first (e.g., for ImGui)
        if (ctx->input_manager.m_pre_key_callback) {
            ctx->input_manager.m_pre_key_callback(window, key, scancode, action, mods);
        }

        // Notify all subscribers
        for (auto *subscriber: ctx->input_manager.m_subscribers) {
            subscriber->on_key(window, key, scancode, action, mods);
        }
    }

    void Input::cursor_callback(GLFWwindow *window, double xpos, double ypos) {
        auto *ctx = reinterpret_cast<WindowContext *>(::glfwGetWindowUserPointer(window));
        if (!ctx) return;

        // Call pre-callback first (e.g., for ImGui)
        if (ctx->input_manager.m_pre_cursor_callback) {
            ctx->input_manager.m_pre_cursor_callback(window, xpos, ypos);
        }

        // Notify all subscribers
        for (auto *subscriber: ctx->input_manager.m_subscribers) {
            subscriber->on_mouse_move(window, xpos, ypos);
        }
    }

    void Input::mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
        auto *ctx = reinterpret_cast<WindowContext *>(::glfwGetWindowUserPointer(window));
        if (!ctx) return;

        // Call pre-callback first (e.g., for ImGui)
        if (ctx->input_manager.m_pre_mouse_button_callback) {
            ctx->input_manager.m_pre_mouse_button_callback(window, button, action, mods);
        }

        // Notify all subscribers
        for (auto *subscriber: ctx->input_manager.m_subscribers) {
            subscriber->on_mouse_button(window, button, action, mods);
        }
    }

    void Input::scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
        auto *ctx = reinterpret_cast<WindowContext *>(::glfwGetWindowUserPointer(window));
        if (!ctx) return;

        // Call pre-callback first (e.g., for ImGui)
        if (ctx->input_manager.m_pre_scroll_callback) {
            ctx->input_manager.m_pre_scroll_callback(window, xoffset, yoffset);
        }

        // Notify all subscribers
        for (auto *subscriber: ctx->input_manager.m_subscribers) {
            subscriber->on_scroll(window, xoffset, yoffset);
        }
    }
} // namespace borg