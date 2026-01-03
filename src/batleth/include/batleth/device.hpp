#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>
#include <vector>
#include <optional>

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
 * RAII wrapper for VkDevice and VkPhysicalDevice.
 * Manages logical and physical device selection and creation.
 */
class BATLETH_API Device {
public:
    struct QueueFamilyIndices {
        std::optional<std::uint32_t> graphics_family;
        std::optional<std::uint32_t> present_family;

        auto is_complete() const -> bool {
            return graphics_family.has_value() && present_family.has_value();
        }
    };

    struct Config {
        VkInstance instance = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        std::vector<const char*> device_extensions;
    };

    explicit Device(const Config& config);
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) noexcept;
    Device& operator=(Device&&) noexcept;

    auto get_physical_device() const -> VkPhysicalDevice { return m_physical_device; }
    auto get_logical_device() const -> VkDevice { return m_device; }
    auto get_graphics_queue() const -> VkQueue { return m_graphics_queue; }
    auto get_present_queue() const -> VkQueue { return m_present_queue; }
    auto get_queue_family_indices() const -> const QueueFamilyIndices& { return m_indices; }
    auto get_surface() const -> VkSurfaceKHR { return m_surface; }

    auto wait_idle() const -> void;

private:
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphics_queue = VK_NULL_HANDLE;
    VkQueue m_present_queue = VK_NULL_HANDLE;
    QueueFamilyIndices m_indices;

    auto pick_physical_device(VkInstance instance, VkSurfaceKHR surface) -> void;
    auto find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface) -> QueueFamilyIndices;
    auto is_device_suitable(VkPhysicalDevice device, VkSurfaceKHR surface,
                           const std::vector<const char*>& extensions) -> bool;
};

} // namespace batleth
