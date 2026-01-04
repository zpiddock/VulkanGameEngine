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
        VkCommandPool command_pool = VK_NULL_HANDLE;  // Optional: for single-time commands
    };

    explicit Device(const Config& config);
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) noexcept;
    Device& operator=(Device&&) noexcept;

    auto get_instance() const -> VkInstance { return m_instance; }
    auto get_physical_device() const -> VkPhysicalDevice { return m_physical_device; }
    auto get_logical_device() const -> VkDevice { return m_device; }
    auto get_graphics_queue() const -> VkQueue { return m_graphics_queue; }
    auto get_graphics_queue_family() const -> std::uint32_t { return m_indices.graphics_family.value(); }
    auto get_present_queue() const -> VkQueue { return m_present_queue; }
    auto get_queue_family_indices() const -> const QueueFamilyIndices& { return m_indices; }
    auto get_surface() const -> VkSurfaceKHR { return m_surface; }
    auto get_command_pool() const -> VkCommandPool { return m_command_pool; }

    auto wait_idle() const -> void;

    /**
     * Begin a single-time command buffer for immediate operations (e.g., buffer copies, image uploads).
     * Must call end_single_time_commands() when done.
     */
    auto begin_single_time_commands() -> VkCommandBuffer;

    /**
     * End and submit a single-time command buffer.
     */
    auto end_single_time_commands(VkCommandBuffer command_buffer) -> void;

private:
    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphics_queue = VK_NULL_HANDLE;
    VkQueue m_present_queue = VK_NULL_HANDLE;
    VkCommandPool m_command_pool = VK_NULL_HANDLE;
    QueueFamilyIndices m_indices;
    bool m_owns_command_pool = false;

    auto pick_physical_device(VkInstance instance, VkSurfaceKHR surface) -> void;
    auto find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface) -> QueueFamilyIndices;
    auto is_device_suitable(VkPhysicalDevice device, VkSurfaceKHR surface,
                           const std::vector<const char*>& extensions) -> bool;
};

} // namespace batleth
