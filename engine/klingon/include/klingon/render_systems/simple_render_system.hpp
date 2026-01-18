#pragma once

#include <memory>
#include <vulkan/vulkan.h>

#include "batleth/device.hpp"
#include "batleth/pipeline.hpp"
#include "klingon/frame_info.hpp"
#include "klingon/render_system_interface.hpp"

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
     * Render system for G-buffer generation in deferred rendering.
     * Outputs position, normal, and albedo to multiple render targets.
     *
     * This replaces the previous forward rendering approach.
     */
    class KLINGON_API GBufferRenderSystem : public IRenderSystem {
    public:
        GBufferRenderSystem(batleth::Device &device, VkDescriptorSetLayout global_set_layout);

        ~GBufferRenderSystem() override;

        GBufferRenderSystem(const GBufferRenderSystem &) = delete;

        GBufferRenderSystem &operator=(const GBufferRenderSystem &) = delete;

        // IRenderSystem interface
        auto render(FrameInfo &frame_info) -> void override;

        auto on_swapchain_recreate(VkFormat format) -> void override;

    private:
        struct PushConstantData {
            glm::mat4 model_matrix{1.f};
            glm::mat4 normal_matrix{1.f};
        };

        auto create_pipeline(VkDescriptorSetLayout global_set_layout) -> void;

        batleth::Device &m_device;
        VkDescriptorSetLayout m_global_set_layout = VK_NULL_HANDLE;
        std::vector<std::unique_ptr<batleth::Shader> > m_shaders;
        std::unique_ptr<batleth::Pipeline> m_pipeline;
    };

    // Legacy alias for compatibility during migration
    using SimpleRenderSystem = GBufferRenderSystem;
} // namespace klingon
