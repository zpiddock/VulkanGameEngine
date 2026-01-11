#include "klingon/engine.hpp"
#include "klingon/renderer.hpp"
#include "federation/core.hpp"
#include "federation/log.hpp"
#include "borg/window.hpp"

#include <GLFW/glfw3.h>

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

    Renderer::Config renderer_config{};
    renderer_config.application_name = config.application_name;
    renderer_config.enable_validation = config.enable_validation;
    renderer_config.enable_imgui = config.enable_imgui;

    m_renderer = std::make_unique<Renderer>(renderer_config, *m_window);

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

        // Begin rendering
        if (m_renderer->begin_frame()) {
            // Render callback (custom rendering)
            if (m_render_callback) {
                m_render_callback();
            }

            // ImGui callback (if ImGui is enabled)
            if (m_imgui_callback && m_renderer->has_imgui()) {
                m_imgui_callback();
            }

            m_renderer->end_frame();
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
    m_window.reset();

    if (m_core) {
        m_core->shutdown();
    }

    FED_DEBUG("Engine shutdown complete");
}
} // namespace klingon
