#include "klingon/render_systems/deferred_lighting_system.hpp"
#include "federation/log.hpp"

#include <stdexcept>
#include <array>
#include "batleth/shader.hpp"

namespace klingon {

DeferredLightingSystem::DeferredLightingSystem(
    batleth::Device& device,
    VkFormat output_format,
    VkDescriptorSetLayout global_set_layout,
    VkDescriptorSetLayout gbuffer_set_layout
)
    : m_device{device}
    , m_output_format{output_format}
    , m_global_set_layout{global_set_layout}
    , m_gbuffer_set_layout{gbuffer_set_layout} {
    create_pipeline(output_format, global_set_layout, gbuffer_set_layout);
}

DeferredLightingSystem::~DeferredLightingSystem() {
    // Pipeline is RAII-managed
}

auto DeferredLightingSystem::create_pipeline(
    VkFormat output_format,
    VkDescriptorSetLayout global_set_layout,
    VkDescriptorSetLayout gbuffer_set_layout
) -> void {
    auto vertConfig = batleth::Shader::Config{};
    vertConfig.device = m_device.get_logical_device();
    vertConfig.filepath = "assets/shaders/deferred_lighting.vert";
    vertConfig.stage = batleth::Shader::Stage::Vertex;
    vertConfig.enable_hot_reload = true;
    auto vert_shader = std::make_unique<batleth::Shader>(vertConfig);

    auto fragConfig = batleth::Shader::Config{};
    fragConfig.device = m_device.get_logical_device();
    fragConfig.filepath = "assets/shaders/deferred_lighting.frag";
    fragConfig.stage = batleth::Shader::Stage::Fragment;
    fragConfig.enable_hot_reload = true;
    auto frag_shader = std::make_unique<batleth::Shader>(fragConfig);

    // Create pipeline config (no vertex input - procedural fullscreen triangle)
    batleth::Pipeline::Config pipeline_config{};
    pipeline_config.device = m_device.get_logical_device();
    pipeline_config.color_format = output_format;  // Render directly to backbuffer
    pipeline_config.depth_format = VK_FORMAT_UNDEFINED;  // No depth test for fullscreen pass
    pipeline_config.shaders.push_back(vert_shader.get());
    pipeline_config.shaders.push_back(frag_shader.get());

    // No vertex input (procedural triangle generated in vertex shader)
    pipeline_config.vertex_binding_descriptions = {};
    pipeline_config.vertex_attribute_descriptions = {};

    // Two descriptor sets: global UBO (set 0) and G-buffer textures (set 1)
    pipeline_config.descriptor_set_layouts = {global_set_layout, gbuffer_set_layout};
    pipeline_config.push_constant_ranges = {};

    pipeline_config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipeline_config.polygon_mode = VK_POLYGON_MODE_FILL;
    pipeline_config.cull_mode = VK_CULL_MODE_NONE;
    pipeline_config.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    pipeline_config.enable_depth_test = false;
    pipeline_config.enable_depth_write = false;

    m_shaders.push_back(std::move(vert_shader));
    m_shaders.push_back(std::move(frag_shader));
    m_pipeline = std::make_unique<batleth::Pipeline>(pipeline_config);

    FED_INFO("DeferredLightingSystem created successfully");
}

auto DeferredLightingSystem::render(FrameInfo& frame_info) -> void {
    // Bind pipeline
    ::vkCmdBindPipeline(frame_info.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->get_handle());

    // Bind descriptor sets
    std::array<VkDescriptorSet, 2> descriptor_sets = {
        frame_info.global_descriptor_set,  // Set 0: Global UBO
        m_gbuffer_descriptor_set           // Set 1: G-buffer textures
    };

    ::vkCmdBindDescriptorSets(
        frame_info.command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_pipeline->get_layout(),
        0,
        static_cast<uint32_t>(descriptor_sets.size()),
        descriptor_sets.data(),
        0,
        nullptr
    );

    // Draw fullscreen triangle (3 vertices, no vertex buffer)
    ::vkCmdDraw(frame_info.command_buffer, 3, 1, 0, 0);
}

auto DeferredLightingSystem::on_swapchain_recreate(VkFormat format) -> void {
    FED_INFO("DeferredLightingSystem rebuilding pipeline for new swapchain format");
    m_output_format = format;
    m_pipeline.reset();
    m_shaders.clear();
    create_pipeline(format, m_global_set_layout, m_gbuffer_set_layout);
}

} // namespace klingon
