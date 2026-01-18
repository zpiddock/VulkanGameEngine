#pragma once

#include "render_graph_resource.hpp"
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
    // Barrier information for a single resource transition
    struct BATLETH_API BarrierInfo {
        ResourceHandle resource = INVALID_RESOURCE;
        ResourceState before{};
        ResourceState after{};
    };

    /**
 * Batches Vulkan Synchronization2 barriers for efficient submission.
 * Collects image and buffer barriers, then flushes them in a single
 * vkCmdPipelineBarrier2 call.
 */
    class BATLETH_API BarrierBatcher {
    public:
        BarrierBatcher() = default;

        ~BarrierBatcher() = default;

        BarrierBatcher(const BarrierBatcher &) = delete;

        BarrierBatcher &operator=(const BarrierBatcher &) = delete;

        BarrierBatcher(BarrierBatcher &&) = default;

        BarrierBatcher &operator=(BarrierBatcher &&) = default;

        /**
     * Add an image barrier to the batch.
     * @param image The VkImage to transition
     * @param before The resource state before the barrier
     * @param after The resource state after the barrier
     * @param aspect_mask Image aspect flags (color, depth, stencil)
     * @param base_mip_level First mip level to transition
     * @param level_count Number of mip levels to transition
     * @param base_array_layer First array layer to transition
     * @param layer_count Number of array layers to transition
     */
        auto add_image_barrier(
            VkImage image,
            const ResourceState &before,
            const ResourceState &after,
            VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
            std::uint32_t base_mip_level = 0,
            std::uint32_t level_count = 1,
            std::uint32_t base_array_layer = 0,
            std::uint32_t layer_count = 1
        ) -> void;

        /**
     * Add a buffer barrier to the batch.
     * @param buffer The VkBuffer to transition
     * @param before The resource state before the barrier
     * @param after The resource state after the barrier
     * @param offset Offset into the buffer
     * @param size Size of the buffer region (VK_WHOLE_SIZE for entire buffer)
     */
        auto add_buffer_barrier(
            VkBuffer buffer,
            const ResourceState &before,
            const ResourceState &after,
            VkDeviceSize offset = 0,
            VkDeviceSize size = VK_WHOLE_SIZE
        ) -> void;

        /**
     * Add a global memory barrier (no specific resource).
     * Useful for synchronizing all memory accesses.
     */
        auto add_memory_barrier(
            const ResourceState &before,
            const ResourceState &after
        ) -> void;

        /**
     * Flush all batched barriers to the command buffer.
     * Uses vkCmdPipelineBarrier2 (Synchronization2).
     */
        auto flush(VkCommandBuffer cmd) -> void;

        /**
     * Clear all batched barriers without submitting.
     */
        auto clear() -> void;

        /**
     * Check if there are any barriers to submit.
     */
        [[nodiscard]] auto is_empty() const -> bool;

        /**
     * Get the number of batched barriers.
     */
        [[nodiscard]] auto barrier_count() const -> std::size_t;

    private:
        std::vector<VkImageMemoryBarrier2> m_image_barriers;
        std::vector<VkBufferMemoryBarrier2> m_buffer_barriers;
        std::vector<VkMemoryBarrier2> m_memory_barriers;
    };

    /**
 * Compute an image memory barrier for a state transition.
 * Helper function for manual barrier creation.
 */
    BATLETH_API auto compute_image_barrier(
        VkImage image,
        const ResourceState &before,
        const ResourceState &after,
        VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
        std::uint32_t base_mip_level = 0,
        std::uint32_t level_count = 1,
        std::uint32_t base_array_layer = 0,
        std::uint32_t layer_count = 1
    ) -> VkImageMemoryBarrier2;

    /**
 * Compute a buffer memory barrier for a state transition.
 * Helper function for manual barrier creation.
 */
    BATLETH_API auto compute_buffer_barrier(
        VkBuffer buffer,
        const ResourceState &before,
        const ResourceState &after,
        VkDeviceSize offset = 0,
        VkDeviceSize size = VK_WHOLE_SIZE
    ) -> VkBufferMemoryBarrier2;

    /**
 * Check if a barrier is needed between two resource states.
 * Returns false if states are identical or no synchronization is needed.
 */
    BATLETH_API auto needs_barrier(
        const ResourceState &before,
        const ResourceState &after
    ) -> bool;

    /**
 * Check if a queue family ownership transfer is needed.
 */
    BATLETH_API auto needs_queue_transfer(
        const ResourceState &before,
        const ResourceState &after
    ) -> bool;
} // namespace batleth