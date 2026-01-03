#include "klingon/engine.hpp"
#include "klingon/renderer.hpp"
#include "federation/core.hpp"
#include "federation/log.hpp"
#include "borg/window.hpp"

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

    m_renderer = std::make_unique<Renderer>(renderer_config, *m_window);

    FED_INFO("Klingon Engine initialized");
}

Engine::~Engine() {
    shutdown();
}

auto Engine::run() -> void {
    FED_INFO("Starting main loop");
    m_running = true;

    while (m_running && !m_window->should_close()) {
        m_window->poll_events();

        // Begin rendering
        if (m_renderer->begin_frame()) {
            // TODO: Record rendering commands

            m_renderer->end_frame();
        }

        // TODO: Update game logic
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
