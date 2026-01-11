#pragma once

#include <memory>
#include <functional>

#ifdef _WIN32
    #ifdef KLINGON_EXPORTS
        #define KLINGON_API __declspec(dllexport)
    #else
        #define KLINGON_API __declspec(dllimport)
    #endif
#else
    #define KLINGON_API
#endif

namespace federation { class Core; }
namespace borg { class Window; }

namespace klingon {

class Renderer;

/**
 * Main engine class that orchestrates all subsystems.
 * Designed with hot-reload capabilities in mind.
 *
 * The engine provides an extensible game loop through callbacks:
 * - on_update: Called every frame for game logic (receives delta time)
 * - on_render: Called during rendering phase (optional, for custom rendering)
 * - on_imgui: Called during ImGui phase (if ImGui is enabled)
 */
class KLINGON_API Engine {
public:
    struct Config {
        const char* application_name = "Klingon Application";
        std::uint32_t window_width = 1280;
        std::uint32_t window_height = 720;
        bool enable_validation = true;
        bool enable_imgui = false;  // Enable ImGui for editor
    };

    // Callback types
    using UpdateCallback = std::function<void(float delta_time)>;
    using RenderCallback = std::function<void()>;
    using ImGuiCallback = std::function<void()>;

    explicit Engine(const Config& config);
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = default;
    Engine& operator=(Engine&&) = default;

    /**
     * Set callback for frame updates (game logic)
     * @param callback Function called every frame with delta time in seconds
     */
    auto set_update_callback(UpdateCallback callback) -> void { m_update_callback = callback; }

    /**
     * Set callback for custom rendering
     * @param callback Function called during rendering phase
     */
    auto set_render_callback(RenderCallback callback) -> void { m_render_callback = callback; }

    /**
     * Set callback for ImGui rendering
     * @param callback Function called during ImGui phase (if enabled)
     */
    auto set_imgui_callback(ImGuiCallback callback) -> void { m_imgui_callback = callback; }

    /**
     * Run the main game loop
     * Blocks until the application exits
     */
    auto run() -> void;

    /**
     * Request engine shutdown (will exit on next frame)
     */
    auto shutdown() -> void;

    auto get_window() -> borg::Window& { return *m_window; };
    auto get_renderer() -> Renderer& { return *m_renderer; };


private:
    std::unique_ptr<federation::Core> m_core;
    std::unique_ptr<borg::Window> m_window;
    std::unique_ptr<Renderer> m_renderer;

    // Application callbacks
    UpdateCallback m_update_callback;
    RenderCallback m_render_callback;
    ImGuiCallback m_imgui_callback;

    bool m_running = false;
    float m_last_frame_time = 0.0f;
};

} // namespace klingon
