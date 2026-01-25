#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
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
    class Device;

    /**
     * RAII wrapper for VkImage + VkImageView + VmaAllocation
     * Manages GPU image memory with VMA integration
     */
    class BATLETH_API Image {
    public:
        struct Config {
            VkDevice device = VK_NULL_HANDLE;
            VmaAllocator allocator = nullptr;
            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t mip_levels = 1;
            uint32_t array_layers = 1;
            VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
            VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
            VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT;
            VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
            VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
            bool create_view = true;
        };

        explicit Image(const Config& config);
        ~Image();

        // Delete copy operations
        Image(const Image&) = delete;
        auto operator=(const Image&) -> Image& = delete;

        // Move operations
        Image(Image&& other) noexcept;
        auto operator=(Image&& other) noexcept -> Image&;

        // Getters
        [[nodiscard]] auto get_image() const -> VkImage { return m_image; }
        [[nodiscard]] auto get_view() const -> VkImageView { return m_view; }
        [[nodiscard]] auto get_allocation() const -> VmaAllocation { return m_allocation; }
        [[nodiscard]] auto get_format() const -> VkFormat { return m_format; }
        [[nodiscard]] auto get_mip_levels() const -> uint32_t { return m_mip_levels; }
        [[nodiscard]] auto get_extent() const -> VkExtent2D { return {m_width, m_height}; }

        /**
         * Transition image layout with pipeline barrier
         * @param cmd Command buffer to record transition
         * @param old_layout Current image layout
         * @param new_layout Target image layout
         * @param base_mip Starting mip level (default 0)
         * @param mip_count Number of mip levels to transition (default VK_REMAINING_MIP_LEVELS)
         */
        auto transition_layout(
            VkCommandBuffer cmd,
            VkImageLayout old_layout,
            VkImageLayout new_layout,
            uint32_t base_mip = 0,
            uint32_t mip_count = VK_REMAINING_MIP_LEVELS
        ) -> void;

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VmaAllocator m_allocator = nullptr;
        VkImage m_image = VK_NULL_HANDLE;
        VkImageView m_view = VK_NULL_HANDLE;
        VmaAllocation m_allocation = nullptr;
        VkFormat m_format = VK_FORMAT_UNDEFINED;
        VkImageAspectFlags m_aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
        uint32_t m_width = 0;
        uint32_t m_height = 0;
        uint32_t m_mip_levels = 1;
        uint32_t m_array_layers = 1;
    };
} // namespace batleth
