#pragma once

#include <memory>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

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
     * Render system for depth pre-pass.
     * Renders scene geometry depth-only to populate the depth buffer before the main shading pass.
     * This enables early-Z rejection and improves performance by reducing fragment shader invocations.
     */
    class KLINGON_API DepthPrepassSystem {
    public:
        DepthPrepassSystem(
            batleth::Device &device,
            VkFormat depth_format,
            VkDescriptorSetLayout global_layout
        );

        ~DepthPrepassSystem();

        DepthPrepassSystem(const DepthPrepassSystem &) = delete;

        DepthPrepassSystem &operator=(const DepthPrepassSystem &) = delete;

        /**
         * Render depth pre-pass for all game objects.
         * @param frame_info Frame information including command buffer and game objects
         */
        auto render(FrameInfo &frame_info) -> void;

        auto on_swapchain_recreate(VkFormat depth_format) -> void;

    private:
        struct PushConstantData {
            glm::mat4 model_matrix{1.f};
            glm::mat4 normal_matrix{1.f};
        };

        auto create_pipeline(VkFormat depth_format, VkDescriptorSetLayout global_layout) -> void;

        batleth::Device &m_device;
        VkFormat m_depth_format = VK_FORMAT_UNDEFINED;
        VkDescriptorSetLayout m_global_set_layout = VK_NULL_HANDLE;
        std::unique_ptr<batleth::Pipeline> m_pipeline;
        VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
    };
} // namespace klingon
