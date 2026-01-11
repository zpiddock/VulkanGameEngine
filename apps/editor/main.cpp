#include "klingon/engine.hpp"
#include "klingon/camera.hpp"
#include "klingon/transform.hpp"
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

        // Create camera and transform for scene view
        klingon::Camera camera{};
        klingon::Transform camera_transform{};
        camera_transform.translation.z = -5.0f;  // Start camera further back for editor

        // Create movement controller for scene camera
        klingon::MovementController scene_camera_controller{};
        scene_camera_controller.set_target(&camera_transform);
        scene_camera_controller.set_ui_mode(true);  // Start in ui mode

        // Register controller with input system
        engine.get_input().add_subscriber(&scene_camera_controller);

        // Start with cursor disabled for scene camera
        auto* window = engine.get_window().get_native_handle();
        ::glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // Enable raw mouse motion if available
        if (::glfwRawMouseMotionSupported()) {
            ::glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }

        // Set up update callback (editor logic)
        engine.set_update_callback([&](float delta_time) {
            // Update scene camera only when not in UI mode
            if (!scene_camera_controller.is_ui_mode()) {
                scene_camera_controller.update(window, delta_time, camera_transform);
            }

            // Update camera view matrix
            camera.set_view_yxz(camera_transform.translation, camera_transform.rotation);

            // Set camera projection
            auto [width, height] = engine.get_window().get_framebuffer_size();
            float aspect = static_cast<float>(width) / static_cast<float>(height);
            camera.set_perspective_projection(
                glm::radians(60.0f),  // FOV
                aspect,
                0.1f,                 // Near plane
                100.0f                // Far plane
            );

            // TODO: Update editor state, scene hierarchy, etc.
        });

        // Set up render callback (custom rendering)
        engine.set_render_callback([&]() {
            // TODO: Render scene objects using the camera
            // For now, rendering infrastructure will be added later
        });

        // Set up ImGui callback (editor UI)
        bool show_demo = true;
        bool show_scene_window = true;
        bool show_hierarchy = true;
        bool show_stats = true;
        engine.set_imgui_callback([&]() {
            // Main menu bar
            if (ImGui::BeginMainMenuBar()) {
                if (ImGui::BeginMenu("File")) {
                    if (ImGui::MenuItem("New Scene")) {}
                    if (ImGui::MenuItem("Open Scene")) {}
                    if (ImGui::MenuItem("Save Scene")) {}
                    ImGui::Separator();
                    if (ImGui::MenuItem("Exit")) {
                        engine.shutdown();
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("View")) {
                    ImGui::MenuItem("Scene", nullptr, &show_scene_window);
                    ImGui::MenuItem("Hierarchy", nullptr, &show_hierarchy);
                    ImGui::MenuItem("Demo Window", nullptr, &show_demo);
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Stats")) {
                    ImGui::MenuItem("Show Editor Stats", nullptr, &show_stats);
                    ImGui::EndMenu();
                }
                ImGui::EndMainMenuBar();
            }

            // Editor stats window
            if (show_stats) {
                ImGui::Begin("Editor Stats");
                ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
                ImGui::Text("Frame time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
                ImGui::Separator();
                ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)",
                    camera_transform.translation.x,
                    camera_transform.translation.y,
                    camera_transform.translation.z);
                ImGui::Text("Camera Rotation: (%.2f, %.2f, %.2f)",
                    glm::degrees(camera_transform.rotation.x),
                    glm::degrees(camera_transform.rotation.y),
                    glm::degrees(camera_transform.rotation.z));
                ImGui::Separator();
                ImGui::Text("Controls:");
                ImGui::BulletText("ESC - Toggle UI mode");
                ImGui::BulletText("WASD - Move camera");
                ImGui::BulletText("Mouse - Look around");
                ImGui::BulletText("Space - Move up");
                ImGui::BulletText("Shift - Move down");
                ImGui::End();
            }

            // Scene hierarchy window
            if (show_hierarchy) {
                ImGui::Begin("Scene Hierarchy", &show_hierarchy);
                ImGui::Text("Scene objects will appear here");
                // TODO: Show scene hierarchy tree
                ImGui::End();
            }

            // Scene window (viewport)
            if (show_scene_window) {
                ImGui::Begin("Scene View", &show_scene_window);
                ImGui::Text("3D scene will render here");
                // TODO: Render scene to texture and display in ImGui window
                ImGui::End();
            }

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
