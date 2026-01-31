#include "klingon/render_systems/depth_prepass_system.hpp"
#include "klingon/render_systems/simple_render_system.hpp"  // For RenderMode enum
#include "klingon/model/mesh.h"
#include "klingon/material.hpp"  // For Material access
#include "federation/log.hpp"
#include "batleth/shader.hpp"

#include <stdexcept>
#include <ranges>

namespace klingon {
    DepthPrepassSystem::DepthPrepassSystem(
        batleth::Device &device,
        VkFormat depth_format,
        VkDescriptorSetLayout global_layout
    )
        : m_device{device}
          , m_depth_format{depth_format}
          , m_global_set_layout{global_layout} {
        create_pipeline(depth_format, global_layout);

        // Store the pipeline layout for later use
        m_pipeline_layout = m_pipeline->get_layout();
    }

    DepthPrepassSystem::~DepthPrepassSystem() {
        // Pipeline is RAII-managed
    }

    auto DepthPrepassSystem::create_pipeline(VkFormat depth_format, VkDescriptorSetLayout global_layout) -> void {
        auto vertConfig = batleth::Shader::Config{};
        vertConfig.device = m_device.get_logical_device();
        vertConfig.filepath = "assets/shaders/depth_prepass.vert";
        vertConfig.stage = batleth::Shader::Stage::Vertex;
        vertConfig.enable_hot_reload = true;
        auto vert_shader_module = batleth::Shader{vertConfig};

        auto fragConfig = batleth::Shader::Config{};
        fragConfig.device = m_device.get_logical_device();
        fragConfig.filepath = "assets/shaders/depth_prepass.frag";
        fragConfig.stage = batleth::Shader::Stage::Fragment;
        fragConfig.enable_hot_reload = true;
        auto frag_shader_module = batleth::Shader{fragConfig};

        // Get vertex input descriptions (same as SimpleRenderSystem)
        auto binding_descriptions = Vertex::get_binding_descriptions();
        auto attribute_descriptions = Vertex::get_attribute_descriptions();

        // Configure push constants (model and normal matrices)
        VkPushConstantRange push_constant_range{};
        push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        push_constant_range.offset = 0;
        push_constant_range.size = sizeof(PushConstantData);

        // Create pipeline config - depth-only rendering
        batleth::Pipeline::Config pipeline_config{};
        pipeline_config.device = m_device.get_logical_device();
        pipeline_config.color_format = VK_FORMAT_UNDEFINED;  // No color attachment for depth pre-pass
        pipeline_config.depth_format = depth_format;
        pipeline_config.shaders.push_back(&vert_shader_module);
        pipeline_config.shaders.push_back(&frag_shader_module);
        pipeline_config.vertex_binding_descriptions = binding_descriptions;
        pipeline_config.vertex_attribute_descriptions = attribute_descriptions;
        pipeline_config.descriptor_set_layouts = {global_layout};  // Need global UBO for camera matrices
        pipeline_config.push_constant_ranges = {push_constant_range};
        pipeline_config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pipeline_config.polygon_mode = VK_POLYGON_MODE_FILL;
        pipeline_config.cull_mode = VK_CULL_MODE_NONE;  // Don't enable back-face culling, messes us up on billboard quads
        pipeline_config.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        pipeline_config.enable_depth_test = true;
        pipeline_config.enable_depth_write = true;
        pipeline_config.depth_compare_op = VK_COMPARE_OP_LESS;

        m_pipeline = std::make_unique<batleth::Pipeline>(pipeline_config);
        FED_INFO("DepthPrepassSystem created successfully");
    }

    auto DepthPrepassSystem::render(FrameInfo &frame_info) -> void {
        // Delegate to the render mode version with All mode for backward compatibility
        render(frame_info, RenderMode::All);
    }

    auto DepthPrepassSystem::render(FrameInfo &frame_info, RenderMode mode) -> void {
        // Bind pipeline
        ::vkCmdBindPipeline(frame_info.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->get_handle());

        // Bind descriptor sets for camera matrices
        ::vkCmdBindDescriptorSets(
            frame_info.command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipeline->get_layout(),
            0,
            1,
            &frame_info.global_descriptor_set,
            0,
            nullptr
        );

        // Render each game object (depth only)
        for (auto &obj: frame_info.game_objects | std::views::values) {
            if (obj.model_data == nullptr) continue;

            // Render each mesh with filtering
            for (size_t mesh_idx = 0; mesh_idx < obj.model_data->meshes.size(); ++mesh_idx) {
                auto& mesh = obj.model_data->meshes[mesh_idx];

                // Check transparency if filtering is enabled
                if (mode != RenderMode::All) {
                    uint32_t material_idx = obj.model_data->mesh_material_indices[mesh_idx];
                    auto& material = obj.model_data->materials[material_idx];

                    bool is_transparent =
                        ((material.gpu_data.material_flags & 8u) != 0u) ||  // Has opacity texture
                        (material.gpu_data.base_color_factor.a < 0.99f);    // Material alpha

                    // Skip based on mode
                    if (mode == RenderMode::OpaqueOnly && is_transparent) continue;
                    if (mode == RenderMode::TransparentOnly && !is_transparent) continue;
                }

                // Setup push constants
                PushConstantData push{};
                push.model_matrix = obj.transform.mat4();
                push.normal_matrix = obj.transform.normal_matrix();

                ::vkCmdPushConstants(
                    frame_info.command_buffer,
                    m_pipeline->get_layout(),
                    VK_SHADER_STAGE_VERTEX_BIT,
                    0,
                    sizeof(PushConstantData),
                    &push
                );

                mesh->bind(frame_info.command_buffer);
                mesh->draw(frame_info.command_buffer);
            }
        }
    }

    auto DepthPrepassSystem::on_swapchain_recreate(VkFormat depth_format) -> void {
        m_depth_format = depth_format;
        m_pipeline.reset();
        create_pipeline(depth_format, m_global_set_layout);
        m_pipeline_layout = m_pipeline->get_layout();
        FED_INFO("DepthPrepassSystem pipeline recreated");
    }
} // namespace klingon
