#include "klingon/engine.hpp"
#include "klingon/renderer.hpp"
#include "klingon/camera.hpp"
#include "klingon/transform.hpp"
#include "klingon/movement_controller.hpp"
#include "klingon/game_object.hpp"
#include "klingon/frame_info.hpp"
#include "klingon/model/mesh.h"
#include "klingon/render_systems/simple_render_system.hpp"
#include "klingon/render_systems/point_light_system.hpp"
#include "borg/input.hpp"
#include "borg/window.hpp"
#include "federation/log.hpp"

#include <GLFW/glfw3.h>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <vector>

auto main() -> int {
    try {
        klingon::Engine::Config config{};
        config.application_name = "Klingon Game";
        config.window_width = 1920;
        config.window_height = 1080;
        config.enable_validation = false;  // Disable validation in game builds

        auto engine = klingon::Engine{config};

        // Create camera and transform for player view
        klingon::Camera camera{};
        klingon::Transform camera_transform{};
        camera_transform.translation.z = -2.5f;  // Start camera 2.5 units back

        // Create movement controller
        klingon::MovementController controller{};
        controller.set_target(&camera_transform);

        // Register controller with input system
        engine.get_input().add_subscriber(&controller);

        // Set cursor mode to disabled (FPS-style)
        auto* window = engine.get_window().get_native_handle();
        ::glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // Enable raw mouse motion if available
        if (::glfwRawMouseMotionSupported()) {
            ::glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }

        // Create render systems
        auto& renderer = engine.get_renderer();
        auto simple_render_system = std::make_unique<klingon::SimpleRenderSystem>(
            renderer.get_device_ref(),
            renderer.get_swapchain_format(),
            VK_NULL_HANDLE  // TODO: Create global descriptor set layout
        );

        auto point_light_system = std::make_unique<klingon::PointLightSystem>(
            renderer.get_device_ref(),
            renderer.get_swapchain_format(),
            VK_NULL_HANDLE  // TODO: Create global descriptor set layout
        );

        // Create game objects
        klingon::GameObject::Map game_objects;

        // Load smooth vase
        auto smooth_vase = klingon::GameObject::create_game_object();
        smooth_vase.model = klingon::Mesh::create_from_file(renderer.get_device_ref(), "assets/models/smooth_vase.obj");
        smooth_vase.transform.translation = {-0.5f, 0.5f, 0.0f};
        smooth_vase.transform.scale = glm::vec3{3.f};
        game_objects.emplace(smooth_vase.get_id(), std::move(smooth_vase));

        // Load flat vase
        auto flat_vase = klingon::GameObject::create_game_object();
        flat_vase.model = klingon::Mesh::create_from_file(renderer.get_device_ref(), "assets/models/flat_vase.obj");
        flat_vase.transform.translation = {0.5f, 0.5f, 0.0f};
        flat_vase.transform.scale = glm::vec3{3.f};
        game_objects.emplace(flat_vase.get_id(), std::move(flat_vase));

        // Load floor (quad)
        auto floor = klingon::GameObject::create_game_object();
        floor.model = klingon::Mesh::create_from_file(renderer.get_device_ref(), "assets/models/quad.obj");
        floor.transform.translation = {0.f, 0.5f, 0.f};
        floor.transform.scale = glm::vec3{3.f, 1.f, 3.f};
        game_objects.emplace(floor.get_id(), std::move(floor));

        // Create point lights
        std::vector<glm::vec3> light_colors{
            {1.f, 0.1f, 0.1f},
            {0.1f, 0.1f, 1.f},
            {0.1f, 1.f, 0.1f},
            {1.f, 1.f, 0.1f},
            {0.1f, 1.f, 1.f},
            {1.f, 0.1f, 1.f}
        };

        for (int i = 0; i < light_colors.size(); i++) {
            auto point_light = klingon::GameObject::create_point_light(0.2f, 0.1f, light_colors[i]);
            auto rotate_light = glm::rotate(
                glm::mat4(1.f),
                (i * glm::two_pi<float>()) / light_colors.size(),
                {0.f, -1.f, 0.f}
            );
            point_light.transform.translation = glm::vec3(rotate_light * glm::vec4(-1.f, -1.f, -1.f, 1.f));
            game_objects.emplace(point_light.get_id(), std::move(point_light));
        }

        // Global UBO (will be filled each frame)
        klingon::GlobalUbo ubo{};

        // Set up update callback (game logic)
        engine.set_update_callback([&](float delta_time) {
            // Update movement controller
            controller.update(window, delta_time, camera_transform);

            // Update camera view matrix
            camera.set_view_yxz(camera_transform.translation, camera_transform.rotation);

            // Set camera projection
            auto [width, height] = engine.get_window().get_framebuffer_size();
            float aspect = static_cast<float>(width) / static_cast<float>(height);
            camera.set_perspective_projection(
                glm::radians(50.0f),  // FOV
                aspect,
                0.1f,                 // Near plane
                100.0f                // Far plane
            );

            // Update UBO
            ubo.projection = camera.get_projection();
            ubo.view = camera.get_view();

            // TODO: Update point lights animation
        });

        // Set up render callback
        engine.set_render_callback([&]() {
            // Begin rendering (starts command buffer and dynamic rendering)
            renderer.begin_rendering();

            // Create frame info
            klingon::FrameInfo frame_info{
                static_cast<int>(renderer.get_current_frame_index()),
                0.0f,  // frame_time (will be implemented later)
                renderer.get_current_command_buffer(),
                camera,
                VK_NULL_HANDLE,  // TODO: Create descriptor set
                game_objects
            };

            // Render game objects
            simple_render_system->render_game_objects(frame_info);

            // Render point lights
            // point_light_system->render(frame_info);  // Disabled for now (needs descriptor set)

            // End rendering (ends dynamic rendering and command buffer)
            renderer.end_rendering();
        });

        FED_INFO("Starting game loop");

        // Run the engine (blocks until exit)
        engine.run();

        return EXIT_SUCCESS;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
