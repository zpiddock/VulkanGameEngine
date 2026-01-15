#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "batleth/transient_allocator.hpp"
#include "federation/log.hpp"

namespace batleth {

TransientAllocator::TransientAllocator(const Config& config)
    : m_device(config.device) {

    VmaAllocatorCreateInfo allocator_info{};
    allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocator_info.physicalDevice = config.physical_device;
    allocator_info.device = config.device;
    allocator_info.instance = config.instance;
    allocator_info.vulkanApiVersion = config.api_version;

    VkResult result = ::vmaCreateAllocator(&allocator_info, &m_allocator);
    if (result != VK_SUCCESS) {
        FED_ERROR("Failed to create VMA allocator: {}", static_cast<int>(result));
        throw std::runtime_error("Failed to create VMA allocator");
    }

    FED_INFO("TransientAllocator created successfully");
}

TransientAllocator::~TransientAllocator() {
    release_all();

    if (m_allocator) {
        ::vmaDestroyAllocator(m_allocator);
        m_allocator = nullptr;
    }
}

TransientAllocator::TransientAllocator(TransientAllocator&& other) noexcept
    : m_device(other.m_device)
    , m_allocator(other.m_allocator)
    , m_images(std::move(other.m_images))
    , m_buffers(std::move(other.m_buffers)) {
    other.m_device = VK_NULL_HANDLE;
    other.m_allocator = nullptr;
}

TransientAllocator& TransientAllocator::operator=(TransientAllocator&& other) noexcept {
    if (this != &other) {
        release_all();
        if (m_allocator) {
            ::vmaDestroyAllocator(m_allocator);
        }

        m_device = other.m_device;
        m_allocator = other.m_allocator;
        m_images = std::move(other.m_images);
        m_buffers = std::move(other.m_buffers);

        other.m_device = VK_NULL_HANDLE;
        other.m_allocator = nullptr;
    }
    return *this;
}

auto TransientAllocator::allocate_image(
    const ImageResourceDesc& desc,
    const ResourceLifetime& lifetime
) -> PhysicalImage {
    PhysicalImage result{};

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = desc.extent.depth > 1 ? VK_IMAGE_TYPE_3D :
                          (desc.extent.height > 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_1D);
    image_info.format = desc.format;
    image_info.extent = desc.extent;
    image_info.mipLevels = desc.mip_levels;
    image_info.arrayLayers = desc.array_layers;
    image_info.samples = desc.samples;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = desc.usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
    alloc_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    // For transient attachments, use lazy allocation if available
    if (desc.is_transient &&
        (desc.usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                       VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT))) {
        image_info.usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
        alloc_info.preferredFlags = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
    }

    VmaAllocation allocation = nullptr;
    VkResult vk_result = ::vmaCreateImage(
        m_allocator,
        &image_info,
        &alloc_info,
        &result.image,
        &allocation,
        nullptr
    );

    if (vk_result != VK_SUCCESS) {
        FED_ERROR("Failed to allocate transient image: {}", static_cast<int>(vk_result));
        throw std::runtime_error("Failed to allocate transient image");
    }

    result.allocation = allocation;
    result.format = desc.format;
    result.extent = desc.extent;

    // Create image view
    VkImageAspectFlags aspect_mask = format_to_aspect_mask(desc.format);
    result.view = create_image_view(
        result.image,
        desc.format,
        aspect_mask,
        desc.mip_levels,
        desc.array_layers
    );

    // Track allocation
    m_images.push_back({result, lifetime});

    FED_DEBUG("Allocated transient image: {}x{}x{}, format={}, passes=[{},{}]",
              desc.extent.width, desc.extent.height, desc.extent.depth,
              static_cast<int>(desc.format), lifetime.first_pass, lifetime.last_pass);

    return result;
}

auto TransientAllocator::allocate_buffer(
    const BufferResourceDesc& desc,
    const ResourceLifetime& lifetime
) -> PhysicalBuffer {
    PhysicalBuffer result{};

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = desc.size;
    buffer_info.usage = desc.usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
    alloc_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    VmaAllocation allocation = nullptr;
    VkResult vk_result = ::vmaCreateBuffer(
        m_allocator,
        &buffer_info,
        &alloc_info,
        &result.buffer,
        &allocation,
        nullptr
    );

    if (vk_result != VK_SUCCESS) {
        FED_ERROR("Failed to allocate transient buffer: {}", static_cast<int>(vk_result));
        throw std::runtime_error("Failed to allocate transient buffer");
    }

    result.allocation = allocation;
    result.size = desc.size;

    // Track allocation
    m_buffers.push_back({result, lifetime});

    FED_DEBUG("Allocated transient buffer: {} bytes, passes=[{},{}]",
              desc.size, lifetime.first_pass, lifetime.last_pass);

    return result;
}

auto TransientAllocator::free_image(PhysicalImage& image) -> void {
    if (image.view != VK_NULL_HANDLE) {
        ::vkDestroyImageView(m_device, image.view, nullptr);
        image.view = VK_NULL_HANDLE;
    }

    if (image.image != VK_NULL_HANDLE && image.allocation != nullptr) {
        ::vmaDestroyImage(m_allocator, image.image, image.allocation);
        image.image = VK_NULL_HANDLE;
        image.allocation = nullptr;
    }
}

auto TransientAllocator::free_buffer(PhysicalBuffer& buffer) -> void {
    if (buffer.buffer != VK_NULL_HANDLE && buffer.allocation != nullptr) {
        ::vmaDestroyBuffer(m_allocator, buffer.buffer, buffer.allocation);
        buffer.buffer = VK_NULL_HANDLE;
        buffer.allocation = nullptr;
    }
}

auto TransientAllocator::reset() -> void {
    // In a more sophisticated implementation, we would keep the allocations
    // and reuse them. For now, we simply clear tracking.
    // The actual resources stay allocated until release_all() or destructor.
}

auto TransientAllocator::release_all() -> void {
    for (auto& alloc : m_images) {
        free_image(alloc.image);
    }
    m_images.clear();

    for (auto& alloc : m_buffers) {
        free_buffer(alloc.buffer);
    }
    m_buffers.clear();
}

auto TransientAllocator::get_stats() const -> Stats {
    Stats stats{};
    stats.image_count = m_images.size();
    stats.buffer_count = m_buffers.size();

    for (const auto& alloc : m_images) {
        VmaAllocationInfo info{};
        ::vmaGetAllocationInfo(m_allocator, alloc.image.allocation, &info);
        stats.total_allocated += info.size;
    }

    for (const auto& alloc : m_buffers) {
        VmaAllocationInfo info{};
        ::vmaGetAllocationInfo(m_allocator, alloc.buffer.allocation, &info);
        stats.total_allocated += info.size;
    }

    return stats;
}

auto TransientAllocator::create_image_view(
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspect_mask,
    std::uint32_t mip_levels,
    std::uint32_t array_layers
) -> VkImageView {
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = array_layers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = aspect_mask;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = mip_levels;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = array_layers;

    VkImageView view = VK_NULL_HANDLE;
    VkResult result = ::vkCreateImageView(m_device, &view_info, nullptr, &view);
    if (result != VK_SUCCESS) {
        FED_ERROR("Failed to create image view: {}", static_cast<int>(result));
        throw std::runtime_error("Failed to create image view");
    }

    return view;
}

} // namespace batleth
