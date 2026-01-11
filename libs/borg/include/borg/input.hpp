#pragma once

#include <utility>
#include <GLFW/glfw3.h>

#ifdef _WIN32
    #ifdef BORG_EXPORTS
        #define BORG_API __declspec(dllexport)
    #else
        #define BORG_API __declspec(dllimport)
    #endif
#else
    #define BORG_API
#endif

namespace borg {

/**
 * Input handling system for keyboard and mouse input.
 * Designed to be extended for game-specific input requirements.
 */
class BORG_API Input {
public:
    explicit Input(GLFWwindow* window);
    ~Input() = default;

    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;
    Input(Input&&) = default;
    Input& operator=(Input&&) = default;

    auto is_key_pressed(int key) const -> bool;
    auto is_mouse_button_pressed(int button) const -> bool;
    auto get_cursor_position() const -> std::pair<double, double>;

private:
    GLFWwindow* m_window = nullptr;
};

} // namespace borg
