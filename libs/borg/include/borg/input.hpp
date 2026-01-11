#pragma once

#include <utility>
#include <vector>
#include <functional>
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

// Forward declaration
class Window;

/**
 * Interface for objects that want to receive input events.
 * Game-side controllers and systems can implement this interface
 * to respond to input events.
 */
class BORG_API IInputSubscriber {
public:
    virtual ~IInputSubscriber() = default;

    virtual void on_key(GLFWwindow* window, int key, int scancode, int action, int mods) {}
    virtual void on_mouse_move(GLFWwindow* window, double xpos, double ypos) {}
    virtual void on_mouse_button(GLFWwindow* window, int button, int action, int mods) {}
    virtual void on_scroll(GLFWwindow* window, double xoffset, double yoffset) {}
};

/**
 * Input handling system for keyboard and mouse input.
 * Supports both polling-based and callback-based input handling.
 *
 * The input system uses a subscriber pattern where multiple objects
 * can register to receive input callbacks. This makes it extensible
 * for both game and editor applications.
 */
class BORG_API Input {
public:
    explicit Input(Window& window);
    ~Input();

    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;
    Input(Input&&) = delete;
    Input& operator=(Input&&) = delete;

    // Polling-based input queries
    auto is_key_pressed(int key) const -> bool;
    auto is_mouse_button_pressed(int button) const -> bool;
    auto get_cursor_position() const -> std::pair<double, double>;

    // Subscriber management
    auto add_subscriber(IInputSubscriber* subscriber) -> void;
    auto remove_subscriber(IInputSubscriber* subscriber) -> void;

    // Direct callback setters (for ImGui integration)
    using KeyCallback = std::function<void(GLFWwindow*, int, int, int, int)>;
    using CursorPosCallback = std::function<void(GLFWwindow*, double, double)>;
    using MouseButtonCallback = std::function<void(GLFWwindow*, int, int, int)>;
    using ScrollCallback = std::function<void(GLFWwindow*, double, double)>;

    auto set_pre_key_callback(KeyCallback callback) -> void { m_pre_key_callback = callback; }
    auto set_pre_cursor_callback(CursorPosCallback callback) -> void { m_pre_cursor_callback = callback; }
    auto set_pre_mouse_button_callback(MouseButtonCallback callback) -> void { m_pre_mouse_button_callback = callback; }
    auto set_pre_scroll_callback(ScrollCallback callback) -> void { m_pre_scroll_callback = callback; }

private:
    // Static GLFW callbacks
    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void cursor_callback(GLFWwindow* window, double xpos, double ypos);
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

    // Context structure to be stored in GLFW user pointer
    struct WindowContext {
        Window& window;
        Input& input_manager;
    };

    GLFWwindow* m_window = nullptr;
    WindowContext m_context;
    std::vector<IInputSubscriber*> m_subscribers;

    // Pre-callbacks (e.g., for ImGui)
    KeyCallback m_pre_key_callback;
    CursorPosCallback m_pre_cursor_callback;
    MouseButtonCallback m_pre_mouse_button_callback;
    ScrollCallback m_pre_scroll_callback;
};

} // namespace borg
