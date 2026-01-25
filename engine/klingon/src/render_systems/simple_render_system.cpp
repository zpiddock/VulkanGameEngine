#include "klingon/render_systems/simple_render_system.hpp"
#include "klingon/model/mesh.h"
#include "federation/log.hpp"

#include <stdexcept>
#include <array>
#include <ranges>

#include "batleth/shader.hpp"

namespace klingon {
    SimpleRenderSystem::SimpleRenderSystem(
        batleth::Device &device,
        VkFormat swapchain_format,
        VkDescriptorSetLayout global_set_layout,
        VkDescriptorSetLayout forward_plus_set_layout,
        VkDescriptorSetLayout texture_set_layout,
        bool use_forward_plus
    )
        : m_device{device}
          , m_global_set_layout{global_set_layout}
          , m_forward_plus_set_layout{forward_plus_set_layout}
          , m_texture_set_layout{texture_set_layout}
          , m_swapchain_format{swapchain_format}
          , m_use_forward_plus{use_forward_plus} {
        create_pipeline(swapchain_format, global_set_layout);
        FED_INFO("SimpleRenderSystem created with Forward+ {}", use_forward_plus ? "ENABLED" : "DISABLED");
    }

    SimpleRenderSystem::~SimpleRenderSystem() {
        // Pipeline is RAII-managed
    }

    auto SimpleRenderSystem::create_pipeline(VkFormat swapchain_format,
                                             VkDescriptorSetLayout global_set_layout) -> void {
        auto vertConfig = batleth::Shader::Config{};
        vertConfig.device = m_device.get_logical_device();
        vertConfig.filepath = "assets/shaders/simple_shader.vert";
        vertConfig.stage = batleth::Shader::Stage::Vertex;
        vertConfig.enable_hot_reload = true;
        auto vert_shader_module = batleth::Shader{vertConfig};

        auto fragConfig = batleth::Shader::Config{};
        fragConfig.device = m_device.get_logical_device();
        // Use Forward+ shader if enabled
        fragConfig.filepath = m_use_forward_plus
                                  ? "assets/shaders/simple_shader_forward_plus.frag"
                                  : "assets/shaders/simple_shader.frag";
        fragConfig.stage = batleth::Shader::Stage::Fragment;
        fragConfig.enable_hot_reload = true;
        auto frag_shader_module = batleth::Shader{fragConfig};

        FED_INFO("SimpleRenderSystem using shader: {}", fragConfig.filepath.string());

        // Get vertex input descriptions
        auto binding_descriptions = Vertex::get_binding_descriptions();
        auto attribute_descriptions = Vertex::get_attribute_descriptions();

        // Configure push constants
        VkPushConstantRange push_constant_range{};
        push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        push_constant_range.offset = 0;
        push_constant_range.size = sizeof(PushConstantData);

        // Create pipeline config
        batleth::Pipeline::Config pipeline_config{};
        pipeline_config.device = m_device.get_logical_device();
        pipeline_config.color_format = swapchain_format;
        pipeline_config.depth_format = VK_FORMAT_D32_SFLOAT;
        pipeline_config.shaders.push_back(&vert_shader_module);
        pipeline_config.shaders.push_back(&frag_shader_module);
        pipeline_config.vertex_binding_descriptions = binding_descriptions;
        pipeline_config.vertex_attribute_descriptions = attribute_descriptions;

        // Add descriptor set layouts (Set 0: Global, Set 1: Forward+, Set 2: Textures)
        if (m_use_forward_plus && m_forward_plus_set_layout != VK_NULL_HANDLE) {
            pipeline_config.descriptor_set_layouts = {global_set_layout, m_forward_plus_set_layout, m_texture_set_layout};
        } else {
            pipeline_config.descriptor_set_layouts = {global_set_layout, m_texture_set_layout};
        }

        pipeline_config.push_constant_ranges = {push_constant_range};
        pipeline_config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pipeline_config.polygon_mode = VK_POLYGON_MODE_FILL;
        pipeline_config.cull_mode = VK_CULL_MODE_NONE;
        pipeline_config.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        pipeline_config.enable_depth_test = true;
        pipeline_config.enable_depth_write = false;  // Depth prepass writes depth, main pass only reads
        pipeline_config.depth_compare_op = VK_COMPARE_OP_EQUAL;  // Exact match with depth prepass values

        // Enable alpha blending for transparency
        pipeline_config.enable_blending = true;
        pipeline_config.src_color_blend_factor = VK_BLEND_FACTOR_SRC_ALPHA;
        pipeline_config.dst_color_blend_factor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        pipeline_config.color_blend_op = VK_BLEND_OP_ADD;
        pipeline_config.src_alpha_blend_factor = VK_BLEND_FACTOR_ONE;
        pipeline_config.dst_alpha_blend_factor = VK_BLEND_FACTOR_ZERO;
        pipeline_config.alpha_blend_op = VK_BLEND_OP_ADD;

        m_pipeline = std::make_unique<batleth::Pipeline>(pipeline_config);
        FED_INFO("SimpleRenderSystem created successfully");
    }

    auto SimpleRenderSystem::render(FrameInfo &frame_info) -> void {
        // Bind pipeline
        ::vkCmdBindPipeline(frame_info.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->get_handle());

        // Bind descriptor sets (Set 0: Global, Set 1: Forward+, Set 2: Textures)
        if (m_use_forward_plus && m_forward_plus_descriptor_set != VK_NULL_HANDLE) {
            VkDescriptorSet descriptor_sets[] = {
                frame_info.global_descriptor_set,      // Set 0
                m_forward_plus_descriptor_set,          // Set 1
                frame_info.texture_descriptor_set       // Set 2
            };

            ::vkCmdBindDescriptorSets(
                frame_info.command_buffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_pipeline->get_layout(),
                0,
                3,  // Bind 3 sets
                descriptor_sets,
                0,
                nullptr
            );
        } else {
            // No Forward+: bind global (Set 0) and textures (Set 2)
            // NOTE: Must bind to correct indices, skip Set 1
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

            ::vkCmdBindDescriptorSets(
                frame_info.command_buffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_pipeline->get_layout(),
                1,  // Bind to Set 1 (textures are now Set 1 when no Forward+)
                1,
                &frame_info.texture_descriptor_set,
                0,
                nullptr
            );
        }

        // Render each game object
        for (auto &obj: frame_info.game_objects | std::views::values) {
            if (obj.model_data == nullptr) continue;

            // Render each mesh with the root object transform (simplified, no node hierarchy)
            for (size_t mesh_idx = 0; mesh_idx < obj.model_data->meshes.size(); ++mesh_idx) {
                auto& mesh = obj.model_data->meshes[mesh_idx];

                // Setup push constants
                PushConstantData push{};
                push.model_matrix = obj.transform.mat4();
                push.normal_matrix = obj.transform.normal_matrix();

                // Get the correct material index for this mesh
                uint32_t mesh_material_idx = obj.model_data->mesh_material_indices[mesh_idx];
                push.material_index = obj.model_data->material_buffer_offset + mesh_material_idx;

                // Add Forward+ tile information if enabled
                if (m_use_forward_plus) {
                    push.tile_count = glm::uvec2(m_tile_count_x, m_tile_count_y);
                    push.tile_size = m_tile_size;
                    push.max_lights_per_tile = m_max_lights_per_tile;
                }

                ::vkCmdPushConstants(
                    frame_info.command_buffer,
                    m_pipeline->get_layout(),
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    0,
                    sizeof(PushConstantData),
                    &push
                );

                mesh->bind(frame_info.command_buffer);
                mesh->draw(frame_info.command_buffer);
            }
        }
    }

    auto SimpleRenderSystem::on_swapchain_recreate(VkFormat format) -> void {
        FED_INFO("SimpleRenderSystem rebuilding pipeline for new swapchain format");
        m_swapchain_format = format;
        m_pipeline.reset();
        create_pipeline(format, m_global_set_layout);
    }

    auto SimpleRenderSystem::set_forward_plus_resources(
        VkDescriptorSet forward_plus_descriptor_set,
        uint32_t tile_count_x,
        uint32_t tile_count_y,
        uint32_t tile_size,
        uint32_t max_lights_per_tile
    ) -> void {
        m_forward_plus_descriptor_set = forward_plus_descriptor_set;
        m_tile_count_x = tile_count_x;
        m_tile_count_y = tile_count_y;
        m_tile_size = tile_size;
        m_max_lights_per_tile = max_lights_per_tile;
    }
} // namespace klingon