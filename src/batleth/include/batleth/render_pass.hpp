#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#ifdef _WIN32
    #ifdef BATLETH_EXPORTS
        #define BATLETH_API __declspec(dllexport)
    #else
        #define BATLETH_API __declspec(dllimport)
    #endif
#else
    #define BATLETH_API
#endif

namespace batleth {

/**
 * RAII wrapper for VkRenderPass.
 * Defines the structure of rendering operations.
 */
class BATLETH_API RenderPass {
public:
    struct Config {
        VkDevice device = VK_NULL_HANDLE;
        VkFormat color_format = VK_FORMAT_B8G8R8A8_SRGB;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    };

    explicit RenderPass(const Config& config);
    ~RenderPass();

    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;
    RenderPass(RenderPass&&) noexcept;
    RenderPass& operator=(RenderPass&&) noexcept;

    auto get_handle() const -> VkRenderPass { return m_render_pass; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkRenderPass m_render_pass = VK_NULL_HANDLE;
};

} // namespace batleth
