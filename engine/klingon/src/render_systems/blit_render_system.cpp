#include "klingon/render_systems/blit_render_system.hpp"
#include "federation/log.hpp"
#include "batleth/shader.hpp"

#include <stdexcept>
#include <array>

namespace klingon {
    BlitRenderSystem::BlitRenderSystem(batleth::Device &device, VkFormat swapchain_format)
        : m_device{device}
          , m_swapchain_format{swapchain_format} {
        create_descriptor_set_layout();
        create_descriptor_pool();
        create_pipeline(swapchain_format);
    }

    BlitRenderSystem::~BlitRenderSystem() {
        if (m_descriptor_pool != VK_NULL_HANDLE) {
            ::vkDestroyDescriptorPool(m_device.get_logical_device(), m_descriptor_pool, nullptr);
        }
        if (m_descriptor_set_layout != VK_NULL_HANDLE) {
            ::vkDestroyDescriptorSetLayout(m_device.get_logical_device(), m_descriptor_set_layout, nullptr);
        }
    }

    auto BlitRenderSystem::create_descriptor_set_layout() -> void {
        // Single binding for the sampled texture
        VkDescriptorSetLayoutBinding sampler_binding{};
        sampler_binding.binding = 0;
        sampler_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sampler_binding.descriptorCount = 1;
        sampler_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        sampler_binding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = 1;
        layout_info.pBindings = &sampler_binding;

        if (::vkCreateDescriptorSetLayout(m_device.get_logical_device(), &layout_info, nullptr,
                                          &m_descriptor_set_layout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create blit descriptor set layout");
        }
    }

    auto BlitRenderSystem::create_descriptor_pool() -> void {
        // Pool size for a single combined image sampler
        VkDescriptorPoolSize pool_size{};
        pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_size.descriptorCount = 10; // Allow for multiple descriptor sets

        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.poolSizeCount = 1;
        pool_info.pPoolSizes = &pool_size;
        pool_info.maxSets = 10;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        if (::vkCreateDescriptorPool(m_device.get_logical_device(), &pool_info, nullptr,
                                     &m_descriptor_pool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create blit descriptor pool");
        }
    }

    auto BlitRenderSystem::create_pipeline(VkFormat swapchain_format) -> void {
        auto vertConfig = batleth::Shader::Config{};
        vertConfig.device = m_device.get_logical_device();
        vertConfig.filepath = "assets/shaders/fullscreen_blit.vert";
        vertConfig.stage = batleth::Shader::Stage::Vertex;
        vertConfig.enable_hot_reload = true;
        auto vert_shader_module = batleth::Shader{vertConfig};

        auto fragConfig = batleth::Shader::Config{};
        fragConfig.device = m_device.get_logical_device();
        fragConfig.filepath = "assets/shaders/fullscreen_blit.frag";
        fragConfig.stage = batleth::Shader::Stage::Fragment;
        fragConfig.enable_hot_reload = true;
        auto frag_shader_module = batleth::Shader{fragConfig};

        // Create pipeline config (no vertex input - fullscreen triangle generated in shader)
        batleth::Pipeline::Config pipeline_config{};
        pipeline_config.device = m_device.get_logical_device();
        pipeline_config.color_format = swapchain_format;
        pipeline_config.depth_format = VK_FORMAT_UNDEFINED; // No depth for blit
        pipeline_config.shaders.push_back(&vert_shader_module);
        pipeline_config.shaders.push_back(&frag_shader_module);
        pipeline_config.vertex_binding_descriptions = {}; // No vertex input
        pipeline_config.vertex_attribute_descriptions = {}; // No vertex input
        pipeline_config.descriptor_set_layouts = {m_descriptor_set_layout};
        pipeline_config.push_constant_ranges = {}; // No push constants
        pipeline_config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pipeline_config.polygon_mode = VK_POLYGON_MODE_FILL;
        pipeline_config.cull_mode = VK_CULL_MODE_NONE; // Don't cull fullscreen triangle
        pipeline_config.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        pipeline_config.enable_depth_test = false; // No depth testing for blit
        pipeline_config.enable_depth_write = false;
        pipeline_config.depth_compare_op = VK_COMPARE_OP_ALWAYS;

        m_pipeline = std::make_unique<batleth::Pipeline>(pipeline_config);
        FED_INFO("BlitRenderSystem created successfully");
    }

    auto BlitRenderSystem::update_descriptor_set(VkImageView image_view, VkSampler sampler) -> VkDescriptorSet {
        // Allocate descriptor set
        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = m_descriptor_pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &m_descriptor_set_layout;

        VkDescriptorSet descriptor_set;
        if (::vkAllocateDescriptorSets(m_device.get_logical_device(), &alloc_info, &descriptor_set) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate blit descriptor set");
        }

        // Update descriptor set with image view and sampler
        VkDescriptorImageInfo image_info{};
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info.imageView = image_view;
        image_info.sampler = sampler;

        VkWriteDescriptorSet descriptor_write{};
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.dstSet = descriptor_set;
        descriptor_write.dstBinding = 0;
        descriptor_write.dstArrayElement = 0;
        descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_write.descriptorCount = 1;
        descriptor_write.pImageInfo = &image_info;

        ::vkUpdateDescriptorSets(m_device.get_logical_device(), 1, &descriptor_write, 0, nullptr);

        return descriptor_set;
    }

    auto BlitRenderSystem::render(VkCommandBuffer command_buffer, VkImageView source_image_view,
                                   VkSampler source_sampler) -> void {
        // Create temporary descriptor set for this blit
        auto descriptor_set = update_descriptor_set(source_image_view, source_sampler);

        // Bind pipeline
        ::vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->get_handle());

        // Bind descriptor set
        ::vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipeline->get_layout(),
            0,
            1,
            &descriptor_set,
            0,
            nullptr
        );

        // Draw fullscreen triangle (3 vertices, no vertex buffer)
        ::vkCmdDraw(command_buffer, 3, 1, 0, 0);

        // Free descriptor set immediately after use
        ::vkFreeDescriptorSets(m_device.get_logical_device(), m_descriptor_pool, 1, &descriptor_set);
    }

    auto BlitRenderSystem::on_swapchain_recreate(VkFormat format) -> void {
        m_swapchain_format = format;
        m_pipeline.reset();
        create_pipeline(format);
        FED_INFO("BlitRenderSystem pipeline recreated for new swapchain");
    }
} // namespace klingon
