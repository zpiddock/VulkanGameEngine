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
 * Deferred lighting pass - reads G-buffer and computes lighting.
 * Renders a fullscreen triangle and outputs final lit image.
 */
class KLINGON_API DeferredLightingSystem : public IRenderSystem {
public:
    DeferredLightingSystem(
        batleth::Device& device,
        VkFormat output_format,
        VkDescriptorSetLayout global_set_layout,
        VkDescriptorSetLayout gbuffer_set_layout
    );
    ~DeferredLightingSystem() override;

    DeferredLightingSystem(const DeferredLightingSystem&) = delete;
    DeferredLightingSystem& operator=(const DeferredLightingSystem&) = delete;

    // IRenderSystem interface
    auto render(FrameInfo& frame_info) -> void override;
    auto on_swapchain_recreate(VkFormat format) -> void override;

    // Bind G-buffer descriptor set before rendering
    auto set_gbuffer_descriptor_set(VkDescriptorSet descriptor_set) -> void {
        m_gbuffer_descriptor_set = descriptor_set;
    }

private:
    auto create_pipeline(
        VkFormat output_format,
        VkDescriptorSetLayout global_set_layout,
        VkDescriptorSetLayout gbuffer_set_layout
    ) -> void;

    batleth::Device& m_device;
    VkFormat m_output_format = VK_FORMAT_UNDEFINED;
    VkDescriptorSetLayout m_global_set_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_gbuffer_set_layout = VK_NULL_HANDLE;
    VkDescriptorSet m_gbuffer_descriptor_set = VK_NULL_HANDLE;
    std::vector<std::unique_ptr<batleth::Shader>> m_shaders;
    std::unique_ptr<batleth::Pipeline> m_pipeline;
};

} // namespace klingon
