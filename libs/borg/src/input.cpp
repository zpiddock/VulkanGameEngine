#include "borg/input.hpp"

namespace borg {

Input::Input(GLFWwindow* window) : m_window(window) {}

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

} // namespace borg
