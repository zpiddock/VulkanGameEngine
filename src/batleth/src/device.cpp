#include "batleth/device.hpp"
#include <stdexcept>
#include <set>
#include <cstring>

#include "federation/log.hpp"

namespace batleth {

Device::Device(const Config& config) : m_surface(config.surface) {
    pick_physical_device(config.instance, config.surface);

    m_indices = find_queue_families(m_physical_device, config.surface);

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<std::uint32_t> unique_queue_families = {
        m_indices.graphics_family.value(),
        m_indices.present_family.value()
    };

    float queue_priority = 1.0f;
    for (auto queue_family : unique_queue_families) {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_create_info);
    }

    VkPhysicalDeviceFeatures device_features{};

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = static_cast<std::uint32_t>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.pEnabledFeatures = &device_features;
    create_info.enabledExtensionCount = static_cast<std::uint32_t>(config.device_extensions.size());
    create_info.ppEnabledExtensionNames = config.device_extensions.data();

    if (::vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }

    ::vkGetDeviceQueue(m_device, m_indices.graphics_family.value(), 0, &m_graphics_queue);
    ::vkGetDeviceQueue(m_device, m_indices.present_family.value(), 0, &m_present_queue);
}

Device::~Device() {
    FED_DEBUG("Destroying Vulkan Device");
    if (m_device != VK_NULL_HANDLE) {
        ::vkDestroyDevice(m_device, nullptr);
        FED_DEBUG("Destroyed Vulkan Device");
    }
}

Device::Device(Device&& other) noexcept
    : m_surface(other.m_surface)
    , m_physical_device(other.m_physical_device)
    , m_device(other.m_device)
    , m_graphics_queue(other.m_graphics_queue)
    , m_present_queue(other.m_present_queue)
    , m_indices(other.m_indices) {
    other.m_surface = VK_NULL_HANDLE;
    other.m_physical_device = VK_NULL_HANDLE;
    other.m_device = VK_NULL_HANDLE;
    other.m_graphics_queue = VK_NULL_HANDLE;
    other.m_present_queue = VK_NULL_HANDLE;
}

auto Device::operator=(Device&& other) noexcept -> Device& {
    if (this != &other) {
        if (m_device != VK_NULL_HANDLE) {
            ::vkDestroyDevice(m_device, nullptr);
        }

        m_surface = other.m_surface;
        m_physical_device = other.m_physical_device;
        m_device = other.m_device;
        m_graphics_queue = other.m_graphics_queue;
        m_present_queue = other.m_present_queue;
        m_indices = other.m_indices;

        other.m_surface = VK_NULL_HANDLE;
        other.m_physical_device = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
        other.m_graphics_queue = VK_NULL_HANDLE;
        other.m_present_queue = VK_NULL_HANDLE;
    }
    return *this;
}

auto Device::wait_idle() const -> void {
    if (m_device != VK_NULL_HANDLE) {
        ::vkDeviceWaitIdle(m_device);
    }
}

auto Device::pick_physical_device(VkInstance instance, VkSurfaceKHR surface) -> void {
    std::uint32_t device_count = 0;
    ::vkEnumeratePhysicalDevices(instance, &device_count, nullptr);

    if (device_count == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support");
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    ::vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

    for (const auto& device : devices) {
        if (is_device_suitable(device, surface, {})) {
            m_physical_device = device;
            break;
        }
    }

    if (m_physical_device == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU");
    }
}

auto Device::find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface) -> QueueFamilyIndices {
    QueueFamilyIndices indices;

    std::uint32_t queue_family_count = 0;
    ::vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    ::vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    for (std::uint32_t i = 0; i < queue_families.size(); ++i) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics_family = i;
        }

        VkBool32 present_support = false;
        ::vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);

        if (present_support) {
            indices.present_family = i;
        }

        if (indices.is_complete()) {
            break;
        }
    }

    return indices;
}

auto Device::is_device_suitable(VkPhysicalDevice device, VkSurfaceKHR surface,
                               const std::vector<const char*>& extensions) -> bool {
    auto indices = find_queue_families(device, surface);

    // TODO: Check for extension support when needed
    (void)extensions;

    return indices.is_complete();
}

} // namespace batleth
