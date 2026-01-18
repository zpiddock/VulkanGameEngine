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
     * Render system for point light visualization using billboard quads.
     * Renders lights as camera-facing circles using geometry generated in the vertex shader.
     */
    class KLINGON_API PointLightSystem : public IRenderSystem {
    public:
        PointLightSystem(batleth::Device &device, VkFormat swapchain_format, VkDescriptorSetLayout global_set_layout);

        ~PointLightSystem() override;

        PointLightSystem(const PointLightSystem &) = delete;

        PointLightSystem &operator=(const PointLightSystem &) = delete;

        // IRenderSystem interface
        auto update(FrameInfo &frame_info, GlobalUbo &ubo) -> void override;

        auto render(FrameInfo &frame_info) -> void override;

        auto on_swapchain_recreate(VkFormat format) -> void override;

    private:
        struct PointLightPushConstants {
            glm::vec4 position{};
            glm::vec4 color{};
            float radius{};
        };

        auto create_pipeline(VkFormat swapchain_format, VkDescriptorSetLayout global_set_layout) -> void;

        batleth::Device &m_device;
        VkDescriptorSetLayout m_global_set_layout = VK_NULL_HANDLE;
        VkFormat m_swapchain_format = VK_FORMAT_UNDEFINED;
        std::vector<std::unique_ptr<batleth::Shader> > m_shaders;
        std::unique_ptr<batleth::Pipeline> m_pipeline;
    };
} // namespace klingon
