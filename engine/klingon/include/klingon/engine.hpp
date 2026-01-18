#pragma once

#include <vulkan/vulkan.h>
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

namespace federation {
    class Core;
}

namespace borg {
    class Window;
    class Input;
}

namespace klingon {
    class Renderer;
    class RenderGraph;
    class Scene;

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
            const char *application_name = "Klingon Application";
            std::uint32_t window_width = 1280;
            std::uint32_t window_height = 720;
            bool enable_validation = true;
            bool enable_imgui = false; // Enable ImGui for editor
        };

        // Callback types (old render callbacks removed)
        using UpdateCallback = std::function<void(float delta_time)>;
        using ImGuiCallback = std::function<void()>;

        explicit Engine(const Config &config);

        ~Engine();

        Engine(const Engine &) = delete;

        Engine &operator=(const Engine &) = delete;

        Engine(Engine &&) = default;

        Engine &operator=(Engine &&) = default;

        /**
     * Set callback for frame updates (game logic)
     * @param callback Function called every frame with delta time in seconds
     */
        auto set_update_callback(UpdateCallback callback) -> void { m_update_callback = callback; }

        /**
     * Set callback for ImGui rendering
     * @param callback Function called during ImGui phase (if enabled)
     */
        auto set_imgui_callback(ImGuiCallback callback) -> void;

        /**
     * Set the active scene for rendering
     * @param scene Pointer to scene (must outlive engine rendering calls)
     */
        auto set_active_scene(Scene *scene) -> void { m_active_scene = scene; }

        /**
     * Get the currently active scene
     * @return Pointer to active scene, or nullptr if none set
     */
        auto get_active_scene() -> Scene * { return m_active_scene; }
        auto get_active_scene() const -> const Scene * { return m_active_scene; }

        /**
     * Enable/disable debug rendering (point lights, etc.)
     * @param enabled true to enable debug rendering
     */
        auto set_debug_rendering_enabled(bool enabled) -> void;

        auto is_debug_rendering_enabled() const -> bool;

        /**
     * Run the main game loop
     * Blocks until the application exits
     */
        auto run() -> void;

        /**
     * Request engine shutdown (will exit on next frame)
     */
        auto shutdown() -> void;

        auto get_window() -> borg::Window & { return *m_window; };
        auto get_renderer() -> Renderer & { return *m_renderer; };
        auto get_input() -> borg::Input & { return *m_input; };

    private:
        std::unique_ptr<federation::Core> m_core;
        std::unique_ptr<borg::Window> m_window;
        std::unique_ptr<borg::Input> m_input;
        std::unique_ptr<Renderer> m_renderer;

        // Application callbacks
        UpdateCallback m_update_callback;
        ImGuiCallback m_imgui_callback;

        // Scene management
        Scene *m_active_scene = nullptr;

        bool m_running = false;
        float m_last_frame_time = 0.0f;
    };
} // namespace klingon