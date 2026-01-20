#pragma once

#include <memory>
#include <functional>
#include <filesystem>

#include "renderer.hpp"
#include "scene.hpp"
#include "borg/input.hpp"
#include "borg/window.hpp"
#include "federation/core.hpp"
#include "klingon/config.hpp"

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
        // Callback types
        using UpdateCallback = std::function<void(float delta_time)>;
        using ImGuiCallback = std::function<void()>;

        /**
         * Create engine from unified KlingonConfig.
         */
        explicit Engine(const KlingonConfig& config);

        /**
         * Create engine from config file.
         * If file doesn't exist, creates it with defaults.
         */
        static auto from_file(const std::filesystem::path& config_path) -> Engine;

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
        auto set_imgui_callback(const ImGuiCallback &callback) -> void;

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

        /**
         * Reload configuration from file.
         * WARNING: Only certain settings can be reloaded at runtime.
         * Vulkan-related settings require engine restart.
         */
        auto reload_config(const std::filesystem::path& config_path) -> void;

        /**
         * Save current configuration to file.
         */
        auto save_config(const std::filesystem::path& config_path) const -> bool;

        /**
         * Get current engine configuration (read-only).
         */
        auto get_config() const -> const KlingonConfig& { return m_config; }

        auto get_window() -> borg::Window & { return *m_window; };
        auto get_renderer() -> Renderer & { return *m_renderer; };
        auto get_input() -> borg::Input & { return *m_input; };

       auto save_scene(Scene* scene, const std::filesystem::path& filepath) const -> bool;
       auto load_scene(Scene* scene, const std::filesystem::path& filepath) -> bool;

    private:
        KlingonConfig m_config;  // Unified configuration
        std::unique_ptr<federation::Core> m_core;
        std::unique_ptr<borg::Window> m_window;
        std::unique_ptr<borg::Input> m_input;
        std::unique_ptr<Renderer> m_renderer;

        // Application callbacks
        UpdateCallback m_update_callback;
        ImGuiCallback m_imgui_callback;

        // Scene management
        Scene* m_active_scene = nullptr;

        bool m_running = false;
        float m_last_frame_time = 0.0f;
    };
} // namespace klingon