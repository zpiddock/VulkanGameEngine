#include "klingon/engine.hpp"
#include "klingon/renderer.hpp"
#include "klingon/render_graph.hpp"
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

        // Create render graph
        auto render_graph = std::make_unique<klingon::RenderGraph>(renderer);

        // Track last extent for recompilation on resize
        VkExtent2D last_extent = renderer.get_swapchain_extent();

        // Lambda to build the render graph
        auto build_render_graph = [&]() {
            auto extent = renderer.get_swapchain_extent();

            auto& builder = render_graph->begin_build();

            // Create depth buffer as transient resource
            auto depth_buffer = builder.create_image(
                "depth",
                batleth::ImageResourceDesc::create_2d(
                    renderer.get_depth_format(),
                    extent.width,
                    extent.height,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                )
            );

            // Get backbuffer handle (set by render graph from swapchain)
            auto backbuffer = render_graph->get_backbuffer_handle();

            // Main geometry pass
            builder.add_graphics_pass(
                "geometry",
                [&, depth_buffer, backbuffer](const batleth::PassExecutionContext& ctx) {
                    // Update UBO for this frame
                    ubo_buffers[ctx.frame_index]->write_to_buffer(&ubo);
                    ubo_buffers[ctx.frame_index]->flush();

                    // Create frame info
                    klingon::FrameInfo frame_info{
                        static_cast<int>(ctx.frame_index),
                        ctx.delta_time,
                        ctx.command_buffer,
                        camera,
                        global_descriptor_sets[ctx.frame_index],
                        game_objects
                    };

                    // Render game objects
                    simple_render_system->render_game_objects(frame_info);

                    if (engine.m_debug_render) {
                        // Render point lights
                        point_light_system->render(frame_info);
                    }
                }
            )
            .set_color_attachment(0, backbuffer, VK_ATTACHMENT_LOAD_OP_CLEAR, {{0.01f, 0.01f, 0.01f, 1.0f}})
            .set_depth_attachment(depth_buffer, VK_ATTACHMENT_LOAD_OP_CLEAR, {1.0f, 0})
            .write(backbuffer, batleth::ResourceUsage::ColorAttachment)
            .write(depth_buffer, batleth::ResourceUsage::DepthStencilWrite);

            builder.add_graphics_pass(
                "imgui",
                [&, depth_buffer, backbuffer](const batleth::PassExecutionContext& ctx) {
                    engine.get_renderer().get_imgui_context()->render(ctx.command_buffer);
                }
            )
            .set_color_attachment(0, backbuffer, VK_ATTACHMENT_LOAD_OP_LOAD, {{0.01f, 0.01f, 0.01f, 1.0f}})
            .write(backbuffer, batleth::ResourceUsage::ColorAttachment)
            .write(depth_buffer, batleth::ResourceUsage::DepthStencilWrite);

            render_graph->compile();
            FED_INFO("Render graph compiled with {} passes", render_graph->get_pass_count());
        };

        // Initial build
        build_render_graph();

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

            // Check for resize
            auto current_extent = renderer.get_swapchain_extent();
            if (current_extent.width != last_extent.width ||
                current_extent.height != last_extent.height) {
                last_extent = current_extent;
                build_render_graph();
            }
        });

        // Set up render graph callback
        engine.set_render_graph_callback([&](VkCommandBuffer cmd, std::uint32_t frame_index, float delta_time) {
            // Update backbuffer with current swapchain image
            render_graph->set_backbuffer(
                renderer.get_current_swapchain_image(),
                renderer.get_current_swapchain_image_view(),
                renderer.get_swapchain_format(),
                renderer.get_swapchain_extent()
            );

            // Execute the render graph
            render_graph->execute(cmd, frame_index, delta_time);
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
                    ::ImGui::Text("Render Graph:");
                    ::ImGui::BulletText("Passes: %zu", render_graph->get_pass_count());
                    ::ImGui::BulletText("Compiled: %s", render_graph->is_compiled() ? "Yes" : "No");
                    ::ImGui::Separator();
                    ::ImGui::Text("Controls:");
                    ::ImGui::BulletText("ESC - Toggle UI mode");
                    ::ImGui::BulletText("WASD - Move camera");
                    ::ImGui::BulletText("Mouse - Look around");
                    ::ImGui::BulletText("Space - Move up");
                    ::ImGui::BulletText("Shift - Move down");
                    ::ImGui::Separator();
                    ::ImGui::Text("Debug Rendering Settings:");
                    ::ImGui::Checkbox("Enable Debug Rendering", &engine.m_debug_render);
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
