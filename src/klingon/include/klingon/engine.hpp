#pragma once

#include <memory>

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
 */
class KLINGON_API Engine {
public:
    struct Config {
        const char* application_name = "Klingon Application";
        std::uint32_t window_width = 1280;
        std::uint32_t window_height = 720;
        bool enable_validation = true;
    };

    explicit Engine(const Config& config);
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = default;
    Engine& operator=(Engine&&) = default;

    auto run() -> void;
    auto shutdown() -> void;

private:
    std::unique_ptr<federation::Core> m_core;
    std::unique_ptr<borg::Window> m_window;
    std::unique_ptr<Renderer> m_renderer;
    bool m_running = false;
};

} // namespace klingon
