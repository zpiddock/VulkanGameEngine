#include "klingon/render_systems/simple_render_system.hpp"
#include "klingon/model/mesh.h"
#include "federation/log.hpp"

#include <stdexcept>
#include <array>
#include <ranges>

#include "batleth/shader.hpp"

namespace klingon {
    SimpleRenderSystem::SimpleRenderSystem(batleth::Device &device, VkFormat swapchain_format,
                                           VkDescriptorSetLayout global_set_layout)
        : m_device{device}
          , m_global_set_layout{global_set_layout}
          , m_swapchain_format{swapchain_format} {
        create_pipeline(swapchain_format, global_set_layout);
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
        fragConfig.filepath = "assets/shaders/simple_shader.frag";
        fragConfig.stage = batleth::Shader::Stage::Fragment;
        fragConfig.enable_hot_reload = true;
        auto frag_shader_module = batleth::Shader{fragConfig};

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
        pipeline_config.descriptor_set_layouts = {global_set_layout};
        pipeline_config.push_constant_ranges = {push_constant_range};
        pipeline_config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pipeline_config.polygon_mode = VK_POLYGON_MODE_FILL;
        pipeline_config.cull_mode = VK_CULL_MODE_NONE;
        pipeline_config.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        pipeline_config.enable_depth_test = true;
        pipeline_config.enable_depth_write = true;
        pipeline_config.depth_compare_op = VK_COMPARE_OP_LESS;

        m_pipeline = std::make_unique<batleth::Pipeline>(pipeline_config);
        FED_INFO("SimpleRenderSystem created successfully");
    }

    auto SimpleRenderSystem::render(FrameInfo &frame_info) -> void {
        // Bind pipeline
        ::vkCmdBindPipeline(frame_info.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->get_handle());

        // Bind descriptor sets
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

        // Render each game object
        for (auto &obj: frame_info.game_objects | std::views::values) {
            if (obj.model == nullptr) continue;

            PushConstantData push{};
            push.model_matrix = obj.transform.mat4();
            push.normal_matrix = obj.transform.normal_matrix();

            ::vkCmdPushConstants(
                frame_info.command_buffer,
                m_pipeline->get_layout(),
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(PushConstantData),
                &push
            );

            obj.model->bind(frame_info.command_buffer);
            obj.model->draw(frame_info.command_buffer);
        }
    }

    auto SimpleRenderSystem::on_swapchain_recreate(VkFormat format) -> void {
        FED_INFO("SimpleRenderSystem rebuilding pipeline for new swapchain format");
        m_swapchain_format = format;
        m_pipeline.reset();
        create_pipeline(format, m_global_set_layout);
    }
} // namespace klingon