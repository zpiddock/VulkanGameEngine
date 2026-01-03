#include "klingon/engine.hpp"
#include <cstdlib>
#include <exception>
#include <iostream>

#include "federation/log.hpp"

auto main() -> int {

    federation::Logger::set_level(federation::LogLevel::Trace);
    try {
        klingon::Engine::Config config{};
        config.application_name = "Klingon Editor";
        config.window_width = 1920;
        config.window_height = 1080;
        config.enable_validation = true;  // Enable validation in editor builds

        auto engine = klingon::Engine{config};
        engine.run();

        return EXIT_SUCCESS;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
