#include "klingon/engine.hpp"
#include <cstdlib>
#include <exception>
#include <iostream>

#include "federation/log.hpp"
#include <imgui.h>

auto main() -> int {
    federation::Logger::set_level(federation::LogLevel::Trace);
    try {
        // Configure engine
        klingon::Engine::Config config{};
        config.application_name = "Klingon Editor";
        config.window_width = 1920;
        config.window_height = 1080;
        config.enable_validation = true; // Enable validation in editor builds
        config.enable_imgui = true;      // Enable ImGui for editor UI

        klingon::Engine engine{config};

        // Set up update callback (game logic)
        engine.set_update_callback([](float delta_time) {
            // TODO: Update editor state, handle input, etc.
            (void)delta_time; // Unused for now
        });

        // Set up ImGui callback (editor UI)
        bool show_demo = true;
        engine.set_imgui_callback([&show_demo]() {
            // Editor stats window
            ImGui::Begin("Editor Stats");
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::Text("Frame time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
            ImGui::Separator();
            ImGui::Checkbox("Show Demo Window", &show_demo);
            ImGui::End();

            // ImGui demo window for testing
            if (show_demo) {
                ImGui::ShowDemoWindow(&show_demo);
            }
        });

        // Run the engine (blocks until exit)
        FED_INFO("Starting Klingon Editor");
        engine.run();
        FED_INFO("Editor exited cleanly");

        return EXIT_SUCCESS;
    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
