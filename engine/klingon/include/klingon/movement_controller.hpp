#pragma once

#include "borg/input.hpp"
#include "klingon/transform.hpp"
#include <GLFW/glfw3.h>

#ifdef _WIN32
#ifdef KLINGON_EXPORTS
#define KLINGON_API __declspec(dllexport)
#else
#define KLINGON_API __declspec(dllimport)
#endif
#else
#define KLINGON_API
#endif

namespace klingon {
    /**
 * Camera/player movement controller that implements IInputSubscriber.
 * Supports WASD movement, mouse look, and escape key handling.
 * Can be used for both game and editor camera control.
 */
    class KLINGON_API MovementController : public borg::IInputSubscriber {
    public:
        struct KeyMappings {
            int move_left = GLFW_KEY_A;
            int move_right = GLFW_KEY_D;
            int move_forward = GLFW_KEY_W;
            int move_backward = GLFW_KEY_S;
            int move_up = GLFW_KEY_SPACE;
            int move_down = GLFW_KEY_LEFT_SHIFT;
            int look_left = GLFW_KEY_LEFT;
            int look_right = GLFW_KEY_RIGHT;
            int look_up = GLFW_KEY_UP;
            int look_down = GLFW_KEY_DOWN;
            int toggle_ui = GLFW_KEY_ESCAPE;
        };

        MovementController() = default;

        ~MovementController() = default;

        MovementController(const MovementController &) = delete;

        MovementController &operator=(const MovementController &) = delete;

        MovementController(MovementController &&) = default;

        MovementController &operator=(MovementController &&) = default;

        /**
     * Update movement for the target transform
     * @param window GLFW window for polling key states
     * @param delta_time Time since last frame in seconds
     * @param transform Transform to update
     */
        auto update(GLFWwindow *window, float delta_time, Transform &transform) -> void;

        /**
     * Set the target transform for this controller
     * @param transform Target transform to control
     */
        auto set_target(Transform *transform) -> void { m_target = transform; }

        /**
     * Get whether UI mode is active (cursor visible, no camera movement)
     */
        auto is_ui_mode() const -> bool { return m_ui_mode; }

        /**
     * Set UI mode (cursor visible, no camera movement)
     */
        auto set_ui_mode(bool enabled) -> void { m_ui_mode = enabled; }

        // IInputSubscriber interface
        void on_key(GLFWwindow *window, int key, int scancode, int action, int mods) override;

        void on_mouse_move(GLFWwindow *window, double xpos, double ypos) override;

        // Configuration
        KeyMappings keys{};
        float movement_speed = 3.0f;
        float look_speed = 1.5f;
        float mouse_sensitivity = 0.002f;

    private:
        Transform *m_target = nullptr;
        bool m_ui_mode = false;
        bool m_first_mouse = true;
        double m_last_mouse_x = 0.0;
        double m_last_mouse_y = 0.0;
    };
} // namespace klingon