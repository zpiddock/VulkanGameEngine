#include "klingon/movement_controller.hpp"
#include <glm/glm.hpp>
#include <limits>

namespace klingon {
    // TODO: Remove references to GLFWwindow and use our Window wrapper
    auto MovementController::update(GLFWwindow *window, float delta_time, Transform &transform) -> void {
        // Don't move if in UI mode
        if (m_ui_mode) {
            return;
        }

        float yaw = transform.rotation.y;
        const glm::vec3 forward_dir{std::sin(yaw), 0.f, std::cos(yaw)};
        const glm::vec3 right_dir{forward_dir.z, 0.f, -forward_dir.x};
        const glm::vec3 up_dir{0.f, 1.f, 0.f};

        glm::vec3 move_dir{0.f};

        if (::glfwGetKey(window, keys.move_forward) == GLFW_PRESS) {
            move_dir += forward_dir;
        }
        if (::glfwGetKey(window, keys.move_backward) == GLFW_PRESS) {
            move_dir -= forward_dir;
        }
        if (::glfwGetKey(window, keys.move_left) == GLFW_PRESS) {
            move_dir -= right_dir;
        }
        if (::glfwGetKey(window, keys.move_right) == GLFW_PRESS) {
            move_dir += right_dir;
        }
        if (::glfwGetKey(window, keys.move_up) == GLFW_PRESS) {
            move_dir -= up_dir;
        }
        if (::glfwGetKey(window, keys.move_down) == GLFW_PRESS) {
            move_dir += up_dir;
        }

        if (glm::dot(move_dir, move_dir) > std::numeric_limits<float>::epsilon()) {
            transform.translation += movement_speed * delta_time * glm::normalize(move_dir);
        }

        // Clamp pitch
        transform.rotation.x = glm::clamp(transform.rotation.x, -1.5f, 1.5f);

        // Normalize yaw
        transform.rotation.y = glm::mod(transform.rotation.y, glm::two_pi<float>());
    }

    void MovementController::on_key(GLFWwindow *window, int key, int scancode, int action, int mods) {
        // Toggle UI mode with Escape key
        if (key == keys.toggle_ui && action == GLFW_PRESS) {
            m_ui_mode = !m_ui_mode;

            if (m_ui_mode) {
                ::glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            } else {
                ::glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
        }
    }

    void MovementController::on_mouse_move(GLFWwindow *window, double xpos, double ypos) {
        // Don't rotate if UI mode is active
        if (m_ui_mode || !m_target) {
            m_first_mouse = true;
            return;
        }

        if (m_first_mouse) {
            m_last_mouse_x = xpos;
            m_last_mouse_y = ypos;
            m_first_mouse = false;
        }

        float xoffset = static_cast<float>(xpos - m_last_mouse_x);
        float yoffset = static_cast<float>(m_last_mouse_y - ypos); // Reversed: y-coords go from bottom to top

        m_last_mouse_x = xpos;
        m_last_mouse_y = ypos;

        xoffset *= mouse_sensitivity;
        yoffset *= mouse_sensitivity;

        // Apply rotation to the target transform
        m_target->rotation.y += xoffset;
        m_target->rotation.x += yoffset;

        // Clamp pitch so the camera doesn't flip upside down
        m_target->rotation.x = glm::clamp(m_target->rotation.x, -1.5f, 1.5f);

        // Normalize yaw to prevent overflow
        m_target->rotation.y = glm::mod(m_target->rotation.y, glm::two_pi<float>());
    }
} // namespace klingon
