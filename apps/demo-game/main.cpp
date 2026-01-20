#include "klingon/engine.hpp"
#include "klingon/scene.hpp"
#include "klingon/movement_controller.hpp"
#include "klingon/game_object.hpp"
#include "klingon/model/mesh.h"
#include "borg/input.hpp"
#include "borg/window.hpp"
#include "federation/log.hpp"
#include "federation/config_manager.hpp"
#include "game_config.hpp"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdlib>
#include <exception>
#include <iostream>

#include "imgui.h"
#include "klingon/renderer.hpp"

auto main() -> int {
    try {
        federation::Logger::set_level(federation::LogLevel::Trace);

        // Load game-specific config (includes engine config)
        auto game_config = federation::ConfigManager::load<GameConfig>("game.json");

        // Override engine config with game-specific values
        game_config.engine.application.name = "Klingon Game";
        game_config.engine.window.width = 1920;
        game_config.engine.window.height = 1080;
        game_config.engine.vulkan.instance.enable_validation = true;
        game_config.engine.renderer.debug.enable_imgui = true;

        // Create engine from game's engine config
        auto engine = klingon::Engine{game_config.engine};

        // Create scene (owns camera automatically)
        klingon::Scene scene;
        scene.set_name("Demo Scene");

        // Create movement controller for camera
        klingon::MovementController controller{};
        controller.set_target(&scene.get_camera_transform());
        engine.get_input().add_subscriber(&controller);

        // Set cursor mode to disabled (FPS-style)
        auto *window = engine.get_window().get_native_handle();
        ::glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // Enable raw mouse motion if available
        if (::glfwRawMouseMotionSupported()) {
            ::glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }

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

        // Set active scene (engine handles all rendering automatically)
        engine.set_active_scene(&scene);

        // Update callback only for game logic
        engine.set_update_callback([&](float dt) {
            // Movement controller updates camera transform
            controller.update(window, dt, scene.get_camera_transform());
        });

        // ImGui callback for debug UI
        engine.set_imgui_callback([&]() {
            if (controller.is_ui_mode()) {
                ::ImGui::Begin("Editor Stats");
                ::ImGui::Text("FPS: %.1f", ::ImGui::GetIO().Framerate);
                ::ImGui::Text("Frame time: %.3f ms", 1000.0f / ::ImGui::GetIO().Framerate);
                ::ImGui::Separator();
                ::ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)",
                              scene.get_camera_transform().translation.x,
                              scene.get_camera_transform().translation.y,
                              scene.get_camera_transform().translation.z);
                ::ImGui::Text("Camera Rotation: (%.2f, %.2f, %.2f)",
                              glm::degrees(scene.get_camera_transform().rotation.x),
                              glm::degrees(scene.get_camera_transform().rotation.y),
                              glm::degrees(scene.get_camera_transform().rotation.z));
                ::ImGui::Separator();
                ::ImGui::Text("Scene: %s", scene.get_name().c_str());
                ::ImGui::BulletText("Game Objects: %zu", scene.get_game_objects().size());
                ::ImGui::Separator();
                ::ImGui::Text("Controls:");
                ::ImGui::BulletText("ESC - Toggle UI mode");
                ::ImGui::BulletText("WASD - Move camera");
                ::ImGui::BulletText("Mouse - Look around");
                ::ImGui::BulletText("Space - Move up");
                ::ImGui::BulletText("Shift - Move down");
                ::ImGui::Separator();
                ::ImGui::Text("Debug Rendering:");
                bool debug_rendering = engine.is_debug_rendering_enabled();
                if (::ImGui::Checkbox("Enable Debug Rendering", &debug_rendering)) {
                    engine.set_debug_rendering_enabled(debug_rendering);
                }
                ::ImGui::End();
            }
        });

        FED_INFO("Starting game loop");

        // Run the engine (blocks until exit)
        engine.run();

        return EXIT_SUCCESS;
    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
