#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

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
 * RAII wrapper for VkFramebuffer.
 * Manages framebuffers for each swapchain image.
 */
    class BATLETH_API Framebuffer {
    public:
        struct Config {
            VkDevice device = VK_NULL_HANDLE;
            VkRenderPass render_pass = VK_NULL_HANDLE;
            std::vector<VkImageView> image_views;
            std::uint32_t width = 1280;
            std::uint32_t height = 720;
        };

        explicit Framebuffer(const Config &config);

        ~Framebuffer();

        Framebuffer(const Framebuffer &) = delete;

        Framebuffer &operator=(const Framebuffer &) = delete;

        Framebuffer(Framebuffer &&) noexcept;

        Framebuffer &operator=(Framebuffer &&) noexcept;

        auto get_framebuffers() const -> const std::vector<VkFramebuffer> & { return m_framebuffers; }
        auto get_framebuffer(std::size_t index) const -> VkFramebuffer { return m_framebuffers[index]; }

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> m_framebuffers;
    };
} // namespace batleth