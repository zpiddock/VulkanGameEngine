#pragma once

#include "render_graph_resource.hpp"
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

// Forward declare VMA types
struct VmaAllocator_T;
using VmaAllocator = VmaAllocator_T *;

namespace batleth {
    class Device;

    // Resource lifetime information for aliasing analysis
    struct BATLETH_API ResourceLifetime {
        std::uint32_t first_pass = 0;
        std::uint32_t last_pass = 0;

        auto overlaps(const ResourceLifetime &other) const -> bool {
            return !(last_pass < other.first_pass || first_pass > other.last_pass);
        }
    };

    /**
 * Manages transient resource allocation with memory aliasing support.
 * Uses VulkanMemoryAllocator (VMA) for efficient GPU memory management.
 *
 * Transient resources are temporary render targets and buffers that only
 * exist for part of a frame. Resources with non-overlapping lifetimes
 * can share the same memory.
 */
    class BATLETH_API TransientAllocator {
    public:
        struct Config {
            VkInstance instance = VK_NULL_HANDLE;
            VkPhysicalDevice physical_device = VK_NULL_HANDLE;
            VkDevice device = VK_NULL_HANDLE;
            std::uint32_t api_version = VK_API_VERSION_1_3;
        };

        explicit TransientAllocator(const Config &config);

        ~TransientAllocator();

        TransientAllocator(const TransientAllocator &) = delete;

        TransientAllocator &operator=(const TransientAllocator &) = delete;

        TransientAllocator(TransientAllocator &&) noexcept;

        TransientAllocator &operator=(TransientAllocator &&) noexcept;

        /**
     * Allocate a transient image resource.
     * @param desc Image description
     * @param lifetime Resource lifetime (first/last pass indices)
     * @return Allocated physical image
     */
        auto allocate_image(
            const ImageResourceDesc &desc,
            const ResourceLifetime &lifetime
        ) -> PhysicalImage;

        /**
     * Allocate a transient buffer resource.
     * @param desc Buffer description
     * @param lifetime Resource lifetime (first/last pass indices)
     * @return Allocated physical buffer
     */
        auto allocate_buffer(
            const BufferResourceDesc &desc,
            const ResourceLifetime &lifetime
        ) -> PhysicalBuffer;

        /**
     * Free an image resource.
     * Typically called at end of frame or when graph is recompiled.
     */
        auto free_image(PhysicalImage &image) -> void;

        /**
     * Free a buffer resource.
     * Typically called at end of frame or when graph is recompiled.
     */
        auto free_buffer(PhysicalBuffer &buffer) -> void;

        /**
     * Reset the allocator for a new frame.
     * Preserves memory allocations but marks them available for reuse.
     */
        auto reset() -> void;

        /**
     * Release all allocated resources.
     */
        auto release_all() -> void;

        /**
     * Get the underlying VMA allocator handle.
     */
        [[nodiscard]] auto get_vma_allocator() const -> VmaAllocator { return m_allocator; }

        /**
     * Get memory usage statistics.
     */
        struct Stats {
            std::uint64_t total_allocated = 0;
            std::uint64_t image_count = 0;
            std::uint64_t buffer_count = 0;
        };

        [[nodiscard]] auto get_stats() const -> Stats;

    private:
        auto create_image_view(
            VkImage image,
            VkFormat format,
            VkImageAspectFlags aspect_mask,
            std::uint32_t mip_levels,
            std::uint32_t array_layers
        ) -> VkImageView;

        VkDevice m_device = VK_NULL_HANDLE;
        VmaAllocator m_allocator = nullptr;

        // Track allocations for statistics and cleanup
        struct ImageAllocation {
            PhysicalImage image;
            ResourceLifetime lifetime;
        };

        struct BufferAllocation {
            PhysicalBuffer buffer;
            ResourceLifetime lifetime;
        };

        std::vector<ImageAllocation> m_images;
        std::vector<BufferAllocation> m_buffers;
    };
} // namespace batleth