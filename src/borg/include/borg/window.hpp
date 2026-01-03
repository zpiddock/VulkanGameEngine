#pragma once

#include <GLFW/glfw3.h>
#include <string>
#include <cstdint>

// DLL export/import macros for Windows
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
 * RAII wrapper around GLFW window.
 * Handles window creation, destruction, and basic window operations.
 */
class BORG_API Window {
public:
    struct Config {
        std::string title = "Vulkan Application";
        std::uint32_t width = 1280;
        std::uint32_t height = 720;
        bool resizable = true;
    };

    explicit Window(const Config& config);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) noexcept;
    Window& operator=(Window&&) noexcept;

    auto should_close() const -> bool;
    auto get_native_handle() -> GLFWwindow*;
    auto get_framebuffer_size() const -> std::pair<std::uint32_t, std::uint32_t>;
    auto poll_events() -> void;
    auto wait_events() -> void;

private:
    GLFWwindow* m_window = nullptr;
    Config m_config;
};

} // namespace borg
