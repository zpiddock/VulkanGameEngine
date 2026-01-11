#include "klingon/render_systems/point_light_system.hpp"

#include <ranges>

#include "federation/log.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

#include "batleth/shader.hpp"

namespace klingon {

PointLightSystem::PointLightSystem(batleth::Device& device, VkFormat swapchain_format, VkDescriptorSetLayout global_set_layout)
    : m_device{device} {
    create_pipeline(swapchain_format, global_set_layout);
}

PointLightSystem::~PointLightSystem() {
    // Pipeline is RAII-managed
}

auto PointLightSystem::create_pipeline(VkFormat swapchain_format, VkDescriptorSetLayout global_set_layout) -> void {
    // Load compiled shaders
    auto vert_code = batleth::Pipeline::load_shader_from_file("assets/shaders/pointlight.vert.spv");
    auto frag_code = batleth::Pipeline::load_shader_from_file("assets/shaders/pointlight.frag.spv");

    // Configure push constants
    VkPushConstantRange push_constant_range{};
    push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(PointLightPushConstants);

    // Create pipeline config (no vertex input - geometry generated in shader)
    batleth::Pipeline::Config pipeline_config{};
    pipeline_config.device = m_device.get_logical_device();
    pipeline_config.color_format = swapchain_format;
    pipeline_config.depth_format = VK_FORMAT_D32_SFLOAT;
    pipeline_config.shader_stages = {
        {vert_code, VK_SHADER_STAGE_VERTEX_BIT},
        {frag_code, VK_SHADER_STAGE_FRAGMENT_BIT}
    };
    // No vertex input - billboard quad generated from gl_VertexIndex
    pipeline_config.descriptor_set_layouts = {global_set_layout};
    pipeline_config.push_constant_ranges = {push_constant_range};
    pipeline_config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipeline_config.polygon_mode = VK_POLYGON_MODE_FILL;
    pipeline_config.cull_mode = VK_CULL_MODE_NONE;
    pipeline_config.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    pipeline_config.enable_depth_test = true;
    pipeline_config.enable_depth_write = false; // Don't write to depth buffer for lights
    pipeline_config.depth_compare_op = VK_COMPARE_OP_LESS;

    m_pipeline = std::make_unique<batleth::Pipeline>(pipeline_config);
    FED_INFO("PointLightSystem created successfully");
}

auto PointLightSystem::update(FrameInfo& frame_info, GlobalUbo& ubo) -> void {
    // Rotate lights around the scene
    auto rotate = glm::rotate(glm::mat4(1.f), frame_info.frame_time, glm::vec3(0.f, 1.f, 0.f));

    int light_index = 0;
    for (auto &obj: frame_info.game_objects | std::views::values) {
        if (obj.point_light == nullptr) continue;

        // Update light translation
        obj.transform.translation = glm::vec3(rotate * glm::vec4(obj.transform.translation, 1.f));

        // Update UBO
        ubo.point_lights[light_index].position = glm::vec4(obj.transform.translation, 1.f);
        ubo.point_lights[light_index].color = glm::vec4(obj.color, obj.point_light->light_intensity);
        light_index++;
    }
    ubo.num_lights = light_index;
}

auto PointLightSystem::render(FrameInfo& frame_info) -> void {
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

    // Render each point light
    for (auto &obj: frame_info.game_objects | std::views::values) {
        if (obj.point_light == nullptr) continue;

        PointLightPushConstants push{};
        push.position = glm::vec4(obj.transform.translation, 1.f);
        push.color = glm::vec4(obj.color, obj.point_light->light_intensity);
        push.radius = obj.transform.scale.x;

        ::vkCmdPushConstants(
            frame_info.command_buffer,
            m_pipeline->get_layout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(PointLightPushConstants),
            &push
        );

        // Draw billboard quad (6 vertices generated in vertex shader)
        ::vkCmdDraw(frame_info.command_buffer, 6, 1, 0, 0);
    }
}

} // namespace klingon
