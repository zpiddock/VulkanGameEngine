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
     * Render system for standard mesh rendering with lighting.
     * Uses push constants for per-object data and UBO for global scene data.
     */
    class KLINGON_API SimpleRenderSystem : public IRenderSystem {
    public:
        SimpleRenderSystem(batleth::Device &device, VkFormat swapchain_format, VkDescriptorSetLayout global_set_layout);

        ~SimpleRenderSystem() override;

        SimpleRenderSystem(const SimpleRenderSystem &) = delete;

        SimpleRenderSystem &operator=(const SimpleRenderSystem &) = delete;

        // IRenderSystem interface
        auto render(FrameInfo &frame_info) -> void override;

        auto on_swapchain_recreate(VkFormat format) -> void override;

    private:
        struct PushConstantData {
            glm::mat4 model_matrix{1.f};
            glm::mat4 normal_matrix{1.f};
        };

        auto create_pipeline(VkFormat swapchain_format, VkDescriptorSetLayout global_set_layout) -> void;

        batleth::Device &m_device;
        VkDescriptorSetLayout m_global_set_layout = VK_NULL_HANDLE;
        VkFormat m_swapchain_format = VK_FORMAT_UNDEFINED;
        std::vector<std::unique_ptr<batleth::Shader> > m_shaders;
        std::unique_ptr<batleth::Pipeline> m_pipeline;
    };
} // namespace klingon
