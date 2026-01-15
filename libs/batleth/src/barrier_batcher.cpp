#include "batleth/barrier_batcher.hpp"

namespace batleth {

auto BarrierBatcher::add_image_barrier(
    VkImage image,
    const ResourceState& before,
    const ResourceState& after,
    VkImageAspectFlags aspect_mask,
    std::uint32_t base_mip_level,
    std::uint32_t level_count,
    std::uint32_t base_array_layer,
    std::uint32_t layer_count
) -> void {
    m_image_barriers.push_back(compute_image_barrier(
        image, before, after, aspect_mask,
        base_mip_level, level_count, base_array_layer, layer_count
    ));
}

auto BarrierBatcher::add_buffer_barrier(
    VkBuffer buffer,
    const ResourceState& before,
    const ResourceState& after,
    VkDeviceSize offset,
    VkDeviceSize size
) -> void {
    m_buffer_barriers.push_back(compute_buffer_barrier(
        buffer, before, after, offset, size
    ));
}

auto BarrierBatcher::add_memory_barrier(
    const ResourceState& before,
    const ResourceState& after
) -> void {
    VkMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    barrier.pNext = nullptr;
    barrier.srcStageMask = before.stage_mask;
    barrier.srcAccessMask = before.access_mask;
    barrier.dstStageMask = after.stage_mask;
    barrier.dstAccessMask = after.access_mask;

    m_memory_barriers.push_back(barrier);
}

auto BarrierBatcher::flush(VkCommandBuffer cmd) -> void {
    if (is_empty()) {
        return;
    }

    VkDependencyInfo dependency_info{};
    dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency_info.pNext = nullptr;
    dependency_info.dependencyFlags = 0;

    dependency_info.memoryBarrierCount = static_cast<std::uint32_t>(m_memory_barriers.size());
    dependency_info.pMemoryBarriers = m_memory_barriers.empty() ? nullptr : m_memory_barriers.data();

    dependency_info.bufferMemoryBarrierCount = static_cast<std::uint32_t>(m_buffer_barriers.size());
    dependency_info.pBufferMemoryBarriers = m_buffer_barriers.empty() ? nullptr : m_buffer_barriers.data();

    dependency_info.imageMemoryBarrierCount = static_cast<std::uint32_t>(m_image_barriers.size());
    dependency_info.pImageMemoryBarriers = m_image_barriers.empty() ? nullptr : m_image_barriers.data();

    ::vkCmdPipelineBarrier2(cmd, &dependency_info);

    clear();
}

auto BarrierBatcher::clear() -> void {
    m_image_barriers.clear();
    m_buffer_barriers.clear();
    m_memory_barriers.clear();
}

auto BarrierBatcher::is_empty() const -> bool {
    return m_image_barriers.empty() &&
           m_buffer_barriers.empty() &&
           m_memory_barriers.empty();
}

auto BarrierBatcher::barrier_count() const -> std::size_t {
    return m_image_barriers.size() +
           m_buffer_barriers.size() +
           m_memory_barriers.size();
}

auto compute_image_barrier(
    VkImage image,
    const ResourceState& before,
    const ResourceState& after,
    VkImageAspectFlags aspect_mask,
    std::uint32_t base_mip_level,
    std::uint32_t level_count,
    std::uint32_t base_array_layer,
    std::uint32_t layer_count
) -> VkImageMemoryBarrier2 {
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.pNext = nullptr;

    // Source state
    barrier.srcStageMask = before.stage_mask;
    barrier.srcAccessMask = before.access_mask;
    barrier.oldLayout = before.layout;
    barrier.srcQueueFamilyIndex = before.queue_family;

    // Destination state
    barrier.dstStageMask = after.stage_mask;
    barrier.dstAccessMask = after.access_mask;
    barrier.newLayout = after.layout;
    barrier.dstQueueFamilyIndex = after.queue_family;

    // Image info
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect_mask;
    barrier.subresourceRange.baseMipLevel = base_mip_level;
    barrier.subresourceRange.levelCount = level_count;
    barrier.subresourceRange.baseArrayLayer = base_array_layer;
    barrier.subresourceRange.layerCount = layer_count;

    return barrier;
}

auto compute_buffer_barrier(
    VkBuffer buffer,
    const ResourceState& before,
    const ResourceState& after,
    VkDeviceSize offset,
    VkDeviceSize size
) -> VkBufferMemoryBarrier2 {
    VkBufferMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    barrier.pNext = nullptr;

    // Source state
    barrier.srcStageMask = before.stage_mask;
    barrier.srcAccessMask = before.access_mask;
    barrier.srcQueueFamilyIndex = before.queue_family;

    // Destination state
    barrier.dstStageMask = after.stage_mask;
    barrier.dstAccessMask = after.access_mask;
    barrier.dstQueueFamilyIndex = after.queue_family;

    // Buffer info
    barrier.buffer = buffer;
    barrier.offset = offset;
    barrier.size = size;

    return barrier;
}

auto needs_barrier(
    const ResourceState& before,
    const ResourceState& after
) -> bool {
    // No barrier needed if states are identical
    if (before == after) {
        return false;
    }

    // Barrier needed for layout transitions
    if (before.layout != after.layout) {
        return true;
    }

    // Barrier needed for queue family transfers
    if (needs_queue_transfer(before, after)) {
        return true;
    }

    // Barrier needed if there's a write followed by any access
    // or if there's an access followed by a write
    bool before_writes = (before.access_mask & (
        VK_ACCESS_2_SHADER_WRITE_BIT |
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_2_TRANSFER_WRITE_BIT |
        VK_ACCESS_2_HOST_WRITE_BIT |
        VK_ACCESS_2_MEMORY_WRITE_BIT |
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT
    )) != 0;

    bool after_writes = (after.access_mask & (
        VK_ACCESS_2_SHADER_WRITE_BIT |
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_2_TRANSFER_WRITE_BIT |
        VK_ACCESS_2_HOST_WRITE_BIT |
        VK_ACCESS_2_MEMORY_WRITE_BIT |
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT
    )) != 0;

    // Write-after-write hazard
    if (before_writes && after_writes) {
        return true;
    }

    // Write-after-read hazard (WAR) - execution barrier only, but we insert for safety
    // Read-after-write hazard (RAW)
    if (before_writes || after_writes) {
        return true;
    }

    // No hazard detected - read-after-read is safe
    return false;
}

auto needs_queue_transfer(
    const ResourceState& before,
    const ResourceState& after
) -> bool {
    // No transfer if either queue family is ignored
    if (before.queue_family == VK_QUEUE_FAMILY_IGNORED ||
        after.queue_family == VK_QUEUE_FAMILY_IGNORED) {
        return false;
    }

    // Transfer needed if queue families differ
    return before.queue_family != after.queue_family;
}

} // namespace batleth
