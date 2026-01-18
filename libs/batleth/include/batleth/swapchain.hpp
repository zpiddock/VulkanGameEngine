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
 * RAII wrapper for VkSwapchainKHR.
 * Handles swapchain creation, image views, and resize support.
 */
    class BATLETH_API Swapchain {
    public:
        struct Config {
            VkPhysicalDevice physical_device = VK_NULL_HANDLE;
            VkDevice device = VK_NULL_HANDLE;
            VkSurfaceKHR surface = VK_NULL_HANDLE;
            std::uint32_t width = 1280;
            std::uint32_t height = 720;
            std::uint32_t min_image_count = 2;
            VkPresentModeKHR preferred_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
        };

        struct SupportDetails {
            VkSurfaceCapabilitiesKHR capabilities;
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> present_modes;
        };

        explicit Swapchain(const Config &config);

        ~Swapchain();

        Swapchain(const Swapchain &) = delete;

        Swapchain &operator=(const Swapchain &) = delete;

        Swapchain(Swapchain &&) noexcept;

        Swapchain &operator=(Swapchain &&) noexcept;

        auto get_handle() const -> VkSwapchainKHR { return m_swapchain; }
        auto get_images() const -> const std::vector<VkImage> & { return m_images; }
        auto get_image_views() const -> const std::vector<VkImageView> & { return m_image_views; }
        auto get_format() const -> VkFormat { return m_format; }
        auto get_extent() const -> VkExtent2D { return m_extent; }
        auto get_image_count() const -> std::uint32_t { return static_cast<std::uint32_t>(m_images.size()); }

        /**
     * Recreates the swapchain with new dimensions.
     * Used when the window is resized.
     */
        auto resize(std::uint32_t width, std::uint32_t height) -> void;

        /**
     * Query swapchain support details for a physical device and surface.
     */
        static auto query_support(VkPhysicalDevice device, VkSurfaceKHR surface) -> SupportDetails;

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
        std::vector<VkImage> m_images;
        std::vector<VkImageView> m_image_views;
        VkFormat m_format;
        VkExtent2D m_extent;
        Config m_config;

        auto create_swapchain() -> void;

        auto create_image_views() -> void;

        auto cleanup() -> void;

        auto choose_surface_format(const std::vector<VkSurfaceFormatKHR> &available_formats) -> VkSurfaceFormatKHR;

        auto choose_present_mode(const std::vector<VkPresentModeKHR> &available_modes) -> VkPresentModeKHR;

        auto choose_extent(const VkSurfaceCapabilitiesKHR &capabilities) -> VkExtent2D;
    };
} // namespace batleth