#include "klingon/engine.hpp"
#include "klingon/scene.hpp"
#include "klingon/movement_controller.hpp"
#include "federation/log.hpp"
#include "federation/config_manager.hpp"
#include "editor_config.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <cstdlib>
#include <exception>
#include <iostream>

#include "borg/window.hpp"
#include "klingon/renderer.hpp"

void add_test_objects(klingon::Scene& scene, klingon::Engine& engine);

auto main() -> int {
    federation::Logger::set_level(federation::LogLevel::Trace);
    try {
        // Load editor-specific config (includes engine config)
        auto editor_config = federation::ConfigManager::load<EditorConfig>("editor.json");

        // Override engine config with editor-specific values
        editor_config.engine.application.name = "Klingon Editor";
        editor_config.engine.window.width = 1920;
        editor_config.engine.window.height = 1080;
        editor_config.engine.vulkan.instance.enable_validation = true;  // Always on for editor
        editor_config.engine.renderer.debug.enable_imgui = true;  // Always on for editor

        klingon::Engine engine{editor_config.engine};

        // Create scene (owns camera automatically)
        klingon::Scene scene{};
        scene.set_name("Editor Scene");
        scene.get_camera_transform().translation.z = -5.0f; // Start camera further back for editor

        // Create movement controller for scene camera
        klingon::MovementController scene_camera_controller{};
        scene_camera_controller.set_target(&scene.get_camera_transform());
        scene_camera_controller.set_ui_mode(true); // Start in UI mode for editor

        // Register controller with input system
        engine.get_input().add_subscriber(&scene_camera_controller);

        // Enable raw mouse motion if available
        auto *window = engine.get_window().get_native_handle();
        if (::glfwRawMouseMotionSupported()) {
            engine.get_window().set_input_mode(GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }

        add_test_objects(scene, engine);

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
                    if (::ImGui::MenuItem("Save Scene")) {
                        engine.save_scene(engine.get_active_scene(), "editor_scene.json");
                    }
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
                for (const auto &[id, obj]: scene.get_game_objects()) {
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

            auto &cam_transform = scene.get_camera_transform();
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
    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}

auto add_test_objects(klingon::Scene& scene, klingon::Engine& engine) -> void {

    auto &device = engine.get_renderer().get_device_ref();

    // Load smooth vase
    auto smooth_vase = klingon::GameObject::create_game_object();
    smooth_vase.model = klingon::Mesh::create_from_file(device, "assets/models/smooth_vase.obj");
    smooth_vase.transform.translation = {-0.5f, 0.5f, 0.0f};
    smooth_vase.transform.scale = glm::vec3{3.f};
    scene.add_game_object(std::move(smooth_vase));

    // Load flat vase
    auto flat_vase = klingon::GameObject::create_game_object();
    flat_vase.model = klingon::Mesh::create_from_file(device, "assets/models/flat_vase.obj");
    flat_vase.transform.translation = {0.5f, 0.5f, 0.0f};
    flat_vase.transform.scale = glm::vec3{3.f};
    scene.add_game_object(std::move(flat_vase));

    // Load floor (quad)
    auto floor = klingon::GameObject::create_game_object();
    floor.model = klingon::Mesh::create_from_file(device, "assets/models/quad.obj");
    floor.transform.translation = {0.f, 0.5f, 0.f};
    floor.transform.scale = glm::vec3{3.f, 1.f, 3.f};
    scene.add_game_object(std::move(floor));

    // Create point lights
    std::vector<glm::vec3> light_colors{
                {1.f, 0.1f, 0.1f},
                {0.1f, 0.1f, 1.f},
                {0.1f, 1.f, 0.1f},
                {1.f, 1.f, 0.1f},
                {0.1f, 1.f, 1.f},
                {1.f, 0.1f, 1.f}
    };

    for (std::size_t i = 0; i < light_colors.size(); i++) {
        auto point_light = klingon::GameObject::create_point_light(0.2f, 0.1f, light_colors[i]);
        auto rotate_light = glm::rotate(
            glm::mat4(1.f),
            (static_cast<float>(i) * glm::two_pi<float>()) / static_cast<float>(light_colors.size()),
            {0.f, -1.f, 0.f}
        );
        point_light.transform.translation = glm::vec3(rotate_light * glm::vec4(-1.f, -1.f, -1.f, 1.f));
        scene.add_game_object(std::move(point_light));
    }
}