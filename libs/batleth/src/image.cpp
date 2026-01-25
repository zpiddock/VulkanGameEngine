#include "batleth/image.hpp"
#include "federation/log.hpp"

namespace batleth {

Image::Image(const Config& config)
    : m_device(config.device)
    , m_allocator(config.allocator)
    , m_format(config.format)
    , m_aspect_flags(config.aspect_flags)
    , m_width(config.width)
    , m_height(config.height)
    , m_mip_levels(config.mip_levels)
    , m_array_layers(config.array_layers) {

    // Create VkImage
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = m_width;
    image_info.extent.height = m_height;
    image_info.extent.depth = 1;
    image_info.mipLevels = m_mip_levels;
    image_info.arrayLayers = m_array_layers;
    image_info.format = m_format;
    image_info.tiling = config.tiling;
    image_info.initialLayout = config.initial_layout;
    image_info.usage = config.usage;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // VMA allocation info
    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
    alloc_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    if (vmaCreateImage(m_allocator, &image_info, &alloc_info, &m_image, &m_allocation, nullptr) != VK_SUCCESS) {
        FED_FATAL("Failed to create VkImage with VMA");
        throw std::runtime_error("Failed to create VkImage");
    }

    FED_TRACE("Created VkImage ({}x{}, {} mip levels)", m_width, m_height, m_mip_levels);

    // Create image view if requested
    if (config.create_view) {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = m_image;
        view_info.viewType = (m_array_layers > 1) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = m_format;
        view_info.subresourceRange.aspectMask = m_aspect_flags;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = m_mip_levels;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = m_array_layers;

        if (::vkCreateImageView(m_device, &view_info, nullptr, &m_view) != VK_SUCCESS) {
            // Clean up image before throwing
            vmaDestroyImage(m_allocator, m_image, m_allocation);
            FED_FATAL("Failed to create VkImageView");
            throw std::runtime_error("Failed to create VkImageView");
        }

        FED_TRACE("Created VkImageView");
    }
}

Image::~Image() {
    if (m_view != VK_NULL_HANDLE) {
        ::vkDestroyImageView(m_device, m_view, nullptr);
        FED_TRACE("Destroyed VkImageView");
    }

    if (m_image != VK_NULL_HANDLE && m_allocator != nullptr) {
        vmaDestroyImage(m_allocator, m_image, m_allocation);
        FED_TRACE("Destroyed VkImage");
    }
}

Image::Image(Image&& other) noexcept
    : m_device(other.m_device)
    , m_allocator(other.m_allocator)
    , m_image(other.m_image)
    , m_view(other.m_view)
    , m_allocation(other.m_allocation)
    , m_format(other.m_format)
    , m_aspect_flags(other.m_aspect_flags)
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_mip_levels(other.m_mip_levels)
    , m_array_layers(other.m_array_layers) {

    // Clear moved-from object
    other.m_device = VK_NULL_HANDLE;
    other.m_allocator = nullptr;
    other.m_image = VK_NULL_HANDLE;
    other.m_view = VK_NULL_HANDLE;
    other.m_allocation = nullptr;
}

auto Image::operator=(Image&& other) noexcept -> Image& {
    if (this != &other) {
        // Clean up existing resources
        if (m_view != VK_NULL_HANDLE) {
            ::vkDestroyImageView(m_device, m_view, nullptr);
        }
        if (m_image != VK_NULL_HANDLE && m_allocator != nullptr) {
            vmaDestroyImage(m_allocator, m_image, m_allocation);
        }

        // Move resources
        m_device = other.m_device;
        m_allocator = other.m_allocator;
        m_image = other.m_image;
        m_view = other.m_view;
        m_allocation = other.m_allocation;
        m_format = other.m_format;
        m_aspect_flags = other.m_aspect_flags;
        m_width = other.m_width;
        m_height = other.m_height;
        m_mip_levels = other.m_mip_levels;
        m_array_layers = other.m_array_layers;

        // Clear moved-from object
        other.m_device = VK_NULL_HANDLE;
        other.m_allocator = nullptr;
        other.m_image = VK_NULL_HANDLE;
        other.m_view = VK_NULL_HANDLE;
        other.m_allocation = nullptr;
    }
    return *this;
}

auto Image::transition_layout(
    VkCommandBuffer cmd,
    VkImageLayout old_layout,
    VkImageLayout new_layout,
    uint32_t base_mip,
    uint32_t mip_count
) -> void {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_image;
    barrier.subresourceRange.aspectMask = m_aspect_flags;
    barrier.subresourceRange.baseMipLevel = base_mip;
    barrier.subresourceRange.levelCount = mip_count;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = m_array_layers;

    // Determine pipeline stages and access masks based on layouts
    VkPipelineStageFlags source_stage;
    VkPipelineStageFlags dest_stage;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dest_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dest_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else {
        FED_ERROR("Unsupported layout transition: {} -> {}", static_cast<int>(old_layout), static_cast<int>(new_layout));
        throw std::runtime_error("Unsupported layout transition");
    }

    ::vkCmdPipelineBarrier(
        cmd,
        source_stage, dest_stage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

} // namespace batleth
