#include "klingon/engine.hpp"
#include "klingon/renderer.hpp"
#include "klingon/scene.hpp"
#include "federation/core.hpp"
#include "federation/log.hpp"
#include "borg/window.hpp"
#include "borg/input.hpp"

#include <GLFW/glfw3.h>
#include <imgui_impl_glfw.h>

namespace klingon {

Engine::Engine(const Config& config) {
    FED_INFO("Initializing Klingon Engine");

    m_core = std::make_unique<federation::Core>();
    m_core->initialize();

    borg::Window::Config window_config{};
    window_config.title = config.application_name;
    window_config.width = config.window_width;
    window_config.height = config.window_height;

    m_window = std::make_unique<borg::Window>(window_config);

    // Create input manager (must be after window)
    m_input = std::make_unique<borg::Input>(*m_window);

    Renderer::Config renderer_config{};
    renderer_config.application_name = config.application_name;
    renderer_config.enable_validation = config.enable_validation;
    renderer_config.enable_imgui = config.enable_imgui;

    m_renderer = std::make_unique<Renderer>(renderer_config, *m_window);

    // Wire up ImGui input callbacks if enabled
    if (config.enable_imgui) {
        m_input->set_pre_key_callback(ImGui_ImplGlfw_KeyCallback);
        m_input->set_pre_cursor_callback(ImGui_ImplGlfw_CursorPosCallback);
        m_input->set_pre_mouse_button_callback(ImGui_ImplGlfw_MouseButtonCallback);
        m_input->set_pre_scroll_callback(ImGui_ImplGlfw_ScrollCallback);
        FED_DEBUG("ImGui input callbacks wired to Input system");
    }

    FED_INFO("Klingon Engine initialized");
}

Engine::~Engine() {
    shutdown();
}

auto Engine::run() -> void {
    FED_INFO("Starting main loop");
    m_running = true;
    m_last_frame_time = static_cast<float>(glfwGetTime());

    while (m_running && !m_window->should_close()) {
        // Calculate delta time
        float current_time = static_cast<float>(glfwGetTime());
        float delta_time = current_time - m_last_frame_time;
        m_last_frame_time = current_time;

        // Poll window events
        m_window->poll_events();

        // Update callback (game logic)
        if (m_update_callback) {
            m_update_callback(delta_time);
        }

        // Render scene (handles everything: camera updates, UBO, render graph execution, ImGui)
        if (m_active_scene) {
            m_renderer->render_scene(m_active_scene, delta_time);
        }
    }

    // Wait for all rendering to complete before cleanup
    m_renderer->wait_idle();
    FED_INFO("Main loop ended");
}

auto Engine::shutdown() -> void {
    if (m_running) {
        FED_INFO("Shutting down engine");
        m_running = false;
    }

    // Cleanup in reverse order of initialization
    if (m_renderer) {
        m_renderer->wait_idle();
    }

    m_renderer.reset();
    m_input.reset();
    m_window.reset();

    if (m_core) {
        m_core->shutdown();
    }

    FED_DEBUG("All RAII-wrapped resources destroyed");
    FED_INFO("Bye bye!");
    FED_DEBUG("Engine shutdown complete");
}

auto Engine::set_debug_rendering_enabled(bool enabled) -> void {
    m_renderer->set_debug_rendering_enabled(enabled);
}

auto Engine::is_debug_rendering_enabled() const -> bool {
    return m_renderer->is_debug_rendering_enabled();
}

auto Engine::set_imgui_callback(ImGuiCallback callback) -> void {
    m_imgui_callback = callback;
    // Pass to renderer immediately so it's ready when rendering starts
    if (m_renderer) {
        m_renderer->set_imgui_callback(m_imgui_callback);
    }
}

} // namespace klingon
