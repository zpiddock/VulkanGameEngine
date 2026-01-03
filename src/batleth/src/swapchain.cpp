#include "batleth/swapchain.hpp"
#include "federation/log.hpp"
#include <stdexcept>
#include <algorithm>
#include <limits>

namespace batleth {

Swapchain::Swapchain(const Config& config) : m_device(config.device), m_config(config) {
    FED_INFO("Creating swapchain ({}x{})", config.width, config.height);
    create_swapchain();
    create_image_views();
    FED_DEBUG("Swapchain created with {} images", m_images.size());
}

Swapchain::~Swapchain() {

    FED_DEBUG("Destroying Swapchain");
    cleanup();
}

Swapchain::Swapchain(Swapchain&& other) noexcept
    : m_device(other.m_device)
    , m_swapchain(other.m_swapchain)
    , m_images(std::move(other.m_images))
    , m_image_views(std::move(other.m_image_views))
    , m_format(other.m_format)
    , m_extent(other.m_extent)
    , m_config(other.m_config) {
    other.m_device = VK_NULL_HANDLE;
    other.m_swapchain = VK_NULL_HANDLE;
}

auto Swapchain::operator=(Swapchain&& other) noexcept -> Swapchain& {
    if (this != &other) {
        cleanup();

        m_device = other.m_device;
        m_swapchain = other.m_swapchain;
        m_images = std::move(other.m_images);
        m_image_views = std::move(other.m_image_views);
        m_format = other.m_format;
        m_extent = other.m_extent;
        m_config = other.m_config;

        other.m_device = VK_NULL_HANDLE;
        other.m_swapchain = VK_NULL_HANDLE;
    }
    return *this;
}

auto Swapchain::resize(std::uint32_t width, std::uint32_t height) -> void {
    FED_INFO("Resizing swapchain from {}x{} to {}x{}", m_config.width, m_config.height, width, height);

    m_config.width = width;
    m_config.height = height;

    // Wait for device to be idle before recreating swapchain
    ::vkDeviceWaitIdle(m_device);

    cleanup();
    create_swapchain();
    create_image_views();

    FED_DEBUG("Swapchain resized successfully");
}

auto Swapchain::query_support(VkPhysicalDevice device, VkSurfaceKHR surface) -> SupportDetails {
    SupportDetails details;

    ::vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    std::uint32_t format_count;
    ::vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);

    if (format_count != 0) {
        details.formats.resize(format_count);
        ::vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, details.formats.data());
    }

    std::uint32_t present_mode_count;
    ::vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);

    if (present_mode_count != 0) {
        details.present_modes.resize(present_mode_count);
        ::vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, details.present_modes.data());
    }

    return details;
}

auto Swapchain::create_swapchain() -> void {
    auto support_details = query_support(m_config.physical_device, m_config.surface);

    auto surface_format = choose_surface_format(support_details.formats);
    auto present_mode = choose_present_mode(support_details.present_modes);
    auto extent = choose_extent(support_details.capabilities);

    std::uint32_t image_count = m_config.min_image_count;
    if (support_details.capabilities.maxImageCount > 0 && image_count > support_details.capabilities.maxImageCount) {
        image_count = support_details.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = m_config.surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.preTransform = support_details.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    if (::vkCreateSwapchainKHR(m_device, &create_info, nullptr, &m_swapchain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swapchain");
    }

    // Store format and extent
    m_format = surface_format.format;
    m_extent = extent;

    // Retrieve swapchain images
    ::vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, nullptr);
    m_images.resize(image_count);
    ::vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, m_images.data());
}

auto Swapchain::create_image_views() -> void {
    m_image_views.resize(m_images.size());

    for (std::size_t i = 0; i < m_images.size(); ++i) {
        VkImageViewCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = m_images[i];
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = m_format;
        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;

        if (::vkCreateImageView(m_device, &create_info, nullptr, &m_image_views[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image views");
        }
    }
}

auto Swapchain::cleanup() -> void {
    for (auto image_view : m_image_views) {
        ::vkDestroyImageView(m_device, image_view, nullptr);
    }
    m_image_views.clear();

    if (m_swapchain != VK_NULL_HANDLE) {
        ::vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
        FED_DEBUG("Destroyed Swapchain");
    }
}

auto Swapchain::choose_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats) -> VkSurfaceFormatKHR {
    // Prefer SRGB color space with BGRA8 format
    for (const auto& format : available_formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    // Fall back to first available format
    return available_formats[0];
}

auto Swapchain::choose_present_mode(const std::vector<VkPresentModeKHR>& available_modes) -> VkPresentModeKHR {
    // Try to find preferred mode
    for (const auto& mode : available_modes) {
        if (mode == m_config.preferred_present_mode) {
            return mode;
        }
    }

    // FIFO is guaranteed to be available
    return VK_PRESENT_MODE_FIFO_KHR;
}

auto Swapchain::choose_extent(const VkSurfaceCapabilitiesKHR& capabilities) -> VkExtent2D {
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    VkExtent2D actual_extent = {m_config.width, m_config.height};

    actual_extent.width = std::clamp(actual_extent.width,
                                     capabilities.minImageExtent.width,
                                     capabilities.maxImageExtent.width);
    actual_extent.height = std::clamp(actual_extent.height,
                                      capabilities.minImageExtent.height,
                                      capabilities.maxImageExtent.height);

    return actual_extent;
}

} // namespace batleth
