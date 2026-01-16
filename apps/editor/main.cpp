#include "klingon/engine.hpp"
#include "klingon/scene.hpp"
#include "klingon/movement_controller.hpp"
#include "federation/log.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <cstdlib>
#include <exception>
#include <iostream>

#include "borg/window.hpp"

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

        // Create scene (owns camera automatically)
        klingon::Scene scene;
        scene.set_name("Editor Scene");
        scene.get_camera_transform().translation.z = -5.0f;  // Start camera further back for editor

        // Create movement controller for scene camera
        klingon::MovementController scene_camera_controller{};
        scene_camera_controller.set_target(&scene.get_camera_transform());
        scene_camera_controller.set_ui_mode(true);  // Start in UI mode for editor

        // Register controller with input system
        engine.get_input().add_subscriber(&scene_camera_controller);

        // Enable raw mouse motion if available
        auto* window = engine.get_window().get_native_handle();
        if (::glfwRawMouseMotionSupported()) {
            engine.get_window().set_input_mode(GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }

        // Set active scene (engine handles all rendering automatically)
        engine.set_active_scene(&scene);

        // Set up update callback (editor logic)
        engine.set_update_callback([&](float delta_time) {
            // Update scene camera
            scene_camera_controller.update(window, delta_time, scene.get_camera_transform());
        });

        // Set up ImGui callback (editor UI)
        engine.set_imgui_callback([&]() {
            // Main menu bar
            if (::ImGui::BeginMainMenuBar()) {
                if (::ImGui::BeginMenu("File")) {
                    if (::ImGui::MenuItem("Exit")) {
                        engine.shutdown();
                    }
                    ::ImGui::EndMenu();
                }
                if (::ImGui::BeginMenu("View")) {
                    // View options will go here
                    ::ImGui::EndMenu();
                }
                ::ImGui::EndMainMenuBar();
            }

            // Scene hierarchy window
            ::ImGui::Begin("Scene Hierarchy");
            ::ImGui::Text("Scene: %s", scene.get_name().c_str());
            ::ImGui::Separator();

            if (::ImGui::TreeNode("Game Objects")) {
                ::ImGui::Text("Count: %zu", scene.get_game_objects().size());
                for (const auto& [id, obj] : scene.get_game_objects()) {
                    ::ImGui::BulletText("Object %u", id);
                }
                ::ImGui::TreePop();
            }
            ::ImGui::End();

            // Properties window
            ::ImGui::Begin("Properties");
            ::ImGui::Text("Select an object to edit properties");
            ::ImGui::End();

            // Viewport stats
            ::ImGui::Begin("Viewport Stats");
            ::ImGui::Text("FPS: %.1f", ::ImGui::GetIO().Framerate);
            ::ImGui::Text("Frame Time: %.3f ms", 1000.0f / ::ImGui::GetIO().Framerate);
            ::ImGui::Separator();

            auto& cam_transform = scene.get_camera_transform();
            ::ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)",
                cam_transform.translation.x,
                cam_transform.translation.y,
                cam_transform.translation.z);
            ::ImGui::Text("Camera Rotation: (%.2f, %.2f, %.2f)",
                glm::degrees(cam_transform.rotation.x),
                glm::degrees(cam_transform.rotation.y),
                glm::degrees(cam_transform.rotation.z));

            ::ImGui::Separator();
            ::ImGui::Text("Controls:");
            ::ImGui::BulletText("F1 - Toggle Camera Mode");
            ::ImGui::BulletText("WASD - Move Camera (Scene mode)");
            ::ImGui::BulletText("Mouse - Rotate Camera (Scene mode)");
            ::ImGui::End();

            // Renderer settings
            ::ImGui::Begin("Renderer Settings");
            bool debug_rendering = engine.is_debug_rendering_enabled();
            if (::ImGui::Checkbox("Debug Rendering", &debug_rendering)) {
                engine.set_debug_rendering_enabled(debug_rendering);
            }
            ::ImGui::End();

            // Console window
            ::ImGui::Begin("Console");
            ::ImGui::TextWrapped("Editor console output will appear here");
            ::ImGui::End();
        });

        FED_INFO("Starting editor");

        // Run the engine (blocks until exit)
        engine.run();

        return EXIT_SUCCESS;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
