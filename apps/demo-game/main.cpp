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
#include "batleth/buffer.hpp"
#include "batleth/descriptors.hpp"
#include "borg/input.hpp"
#include "borg/window.hpp"
#include "federation/log.hpp"

#include <GLFW/glfw3.h>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <vector>

#include "imgui.h"

auto main() -> int {
    try {
        klingon::Engine::Config config{};
        config.application_name = "Klingon Game";
        config.window_width = 1920;
        config.window_height = 1080;
        config.enable_validation = true;
        config.enable_imgui = true;

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

        auto& renderer = engine.get_renderer();
        auto& device = renderer.get_device_ref();

        // Create global descriptor set layout for the UBO
        // Binding 0: GlobalUbo (projection, view, lights, etc.)
        auto global_set_layout = batleth::DescriptorSetLayout::Builder(device.get_logical_device())
            .add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .build();

        // Create uniform buffers (one per frame in flight)
        std::vector<std::unique_ptr<batleth::Buffer>> ubo_buffers;
        for (std::uint32_t i = 0; i < klingon::Renderer::get_max_frames_in_flight(); ++i) {
            auto buffer = std::make_unique<batleth::Buffer>(
                device,
                sizeof(klingon::GlobalUbo),
                1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            );
            buffer->map();
            ubo_buffers.push_back(std::move(buffer));
        }

        // Create descriptor pool
        auto global_pool = batleth::DescriptorPool::Builder(device.get_logical_device())
            .set_max_sets(klingon::Renderer::get_max_frames_in_flight())
            .add_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, klingon::Renderer::get_max_frames_in_flight())
            .build();

        // Allocate and write descriptor sets
        std::vector<VkDescriptorSet> global_descriptor_sets(klingon::Renderer::get_max_frames_in_flight());
        for (std::uint32_t i = 0; i < klingon::Renderer::get_max_frames_in_flight(); ++i) {
            auto buffer_info = ubo_buffers[i]->descriptor_info();
            batleth::DescriptorWriter(*global_set_layout, *global_pool)
                .write_buffer(0, &buffer_info)
                .build(global_descriptor_sets[i]);
        }

        FED_INFO("Created {} global descriptor sets", global_descriptor_sets.size());

        // Create render systems with the global descriptor set layout
        auto simple_render_system = std::make_unique<klingon::SimpleRenderSystem>(
            device,
            renderer.get_swapchain_format(),
            global_set_layout->get_layout()
        );

        auto point_light_system = std::make_unique<klingon::PointLightSystem>(
            device,
            renderer.get_swapchain_format(),
            global_set_layout->get_layout()
        );

        // Create game objects
        klingon::GameObject::Map game_objects;

        // Load smooth vase
        auto smooth_vase = klingon::GameObject::create_game_object();
        smooth_vase.model = klingon::Mesh::create_from_file(device, "assets/models/smooth_vase.obj");
        smooth_vase.transform.translation = {-0.5f, 0.5f, 0.0f};
        smooth_vase.transform.scale = glm::vec3{3.f};
        game_objects.emplace(smooth_vase.get_id(), std::move(smooth_vase));

        // Load flat vase
        auto flat_vase = klingon::GameObject::create_game_object();
        flat_vase.model = klingon::Mesh::create_from_file(device, "assets/models/flat_vase.obj");
        flat_vase.transform.translation = {0.5f, 0.5f, 0.0f};
        flat_vase.transform.scale = glm::vec3{3.f};
        game_objects.emplace(flat_vase.get_id(), std::move(flat_vase));

        // Load floor (quad)
        auto floor = klingon::GameObject::create_game_object();
        floor.model = klingon::Mesh::create_from_file(device, "assets/models/quad.obj");
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

        for (std::size_t i = 0; i < light_colors.size(); i++) {
            auto point_light = klingon::GameObject::create_point_light(0.2f, 0.1f, light_colors[i]);
            auto rotate_light = glm::rotate(
                glm::mat4(1.f),
                (static_cast<float>(i) * glm::two_pi<float>()) / static_cast<float>(light_colors.size()),
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
                glm::radians(60.0f),  // FOV
                aspect,
                0.1f,                 // Near plane
                100.0f                // Far plane
            );

            // Update UBO with camera matrices
            ubo.projection = camera.get_projection();
            ubo.view = camera.get_view();

            klingon::FrameInfo frame_info{
                static_cast<int>(renderer.get_current_frame_index()),
                delta_time,  // frame_time
                renderer.get_current_command_buffer(),
                camera,
                global_descriptor_sets[renderer.get_current_frame_index()],
                game_objects
            };

            // Update point lights in UBO
            point_light_system->update(frame_info, ubo);
        });

        // Set up render callback
        engine.set_render_callback([&] {
            // Get current frame index
            auto frame_index = renderer.get_current_frame_index();

            // Update the UBO for this frame
            ubo_buffers[frame_index]->write_to_buffer(&ubo);
            ubo_buffers[frame_index]->flush();

            // Create frame info
            klingon::FrameInfo frame_info{
                static_cast<int>(frame_index),
                0.0f,  // frame_time
                renderer.get_current_command_buffer(),
                camera,
                global_descriptor_sets[frame_index],
                game_objects
            };

            // Render game objects
            simple_render_system->render_game_objects(frame_info);

            // Render point lights
            point_light_system->render(frame_info);
        });

        engine.set_imgui_callback([&]() {

            if (controller.is_ui_mode()) {
                ::ImGui::Begin("Editor Stats");
                    ::ImGui::Text("FPS: %.1f", ::ImGui::GetIO().Framerate);
                    ::ImGui::Text("Frame time: %.3f ms", 1000.0f / ::ImGui::GetIO().Framerate);
                    ::ImGui::Separator();
                    ::ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)",
                        camera_transform.translation.x,
                        camera_transform.translation.y,
                        camera_transform.translation.z);
                    ::ImGui::Text("Camera Rotation: (%.2f, %.2f, %.2f)",
                        glm::degrees(camera_transform.rotation.x),
                        glm::degrees(camera_transform.rotation.y),
                        glm::degrees(camera_transform.rotation.z));
                    ::ImGui::Separator();
                    ::ImGui::Text("Controls:");
                    ::ImGui::BulletText("ESC - Toggle UI mode");
                    ::ImGui::BulletText("WASD - Move camera");
                    ::ImGui::BulletText("Mouse - Look around");
                    ::ImGui::BulletText("Space - Move up");
                    ::ImGui::BulletText("Shift - Move down");
                    ::ImGui::End();
            }
        });

        FED_INFO("Starting game loop");

        // Run the engine (blocks until exit)
        engine.run();

        // Wait for device to be idle before cleanup
        renderer.wait_idle();

        return EXIT_SUCCESS;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
