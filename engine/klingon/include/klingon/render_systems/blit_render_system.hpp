#pragma once

#include <memory>
#include <vulkan/vulkan.h>

#include "batleth/device.hpp"
#include "batleth/pipeline.hpp"
#include "klingon/frame_info.hpp"

#ifdef _WIN32
#ifdef KLINGON_EXPORTS
#define KLINGON_API __declspec(dllexport)
#else
#define KLINGON_API __declspec(dllimport)
#endif
#else
#define KLINGON_API
#endif

namespace klingon {
    /**
     * Render system for fullscreen blitting from offscreen buffer to backbuffer.
     * Renders a fullscreen triangle and samples from the provided texture.
     */
    class KLINGON_API BlitRenderSystem {
    public:
        BlitRenderSystem(batleth::Device &device, VkFormat swapchain_format);

        ~BlitRenderSystem();

        BlitRenderSystem(const BlitRenderSystem &) = delete;

        BlitRenderSystem &operator=(const BlitRenderSystem &) = delete;

        /**
         * Render fullscreen blit from source texture to current render target.
         * @param command_buffer Command buffer to record commands into
         * @param source_image_view Source texture to sample from
         * @param source_sampler Sampler for the source texture
         */
        auto render(VkCommandBuffer command_buffer, VkImageView source_image_view, VkSampler source_sampler) -> void;

        auto on_swapchain_recreate(VkFormat format) -> void;

    private:
        auto create_pipeline(VkFormat swapchain_format) -> void;
        auto create_descriptor_set_layout() -> void;
        auto create_descriptor_pool() -> void;
        auto update_descriptor_set(VkImageView image_view, VkSampler sampler) -> VkDescriptorSet;

        batleth::Device &m_device;
        VkFormat m_swapchain_format = VK_FORMAT_UNDEFINED;
        std::vector<std::unique_ptr<batleth::Shader> > m_shaders;
        std::unique_ptr<batleth::Pipeline> m_pipeline;
        VkDescriptorSetLayout m_descriptor_set_layout = VK_NULL_HANDLE;
        VkDescriptorPool m_descriptor_pool = VK_NULL_HANDLE;
    };
} // namespace klingon
