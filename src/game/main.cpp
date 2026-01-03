#include "klingon/engine.hpp"
#include <cstdlib>
#include <exception>
#include <iostream>

auto main() -> int {
    try {
        klingon::Engine::Config config{};
        config.application_name = "Klingon Game";
        config.window_width = 1920;
        config.window_height = 1080;
        config.enable_validation = false;  // Disable validation in game builds

        auto engine = klingon::Engine{config};
        engine.run();

        return EXIT_SUCCESS;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
