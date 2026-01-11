#pragma once

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

// Forward declarations
namespace borg { class Window; }

namespace batleth {

/**
 * RAII wrapper for VkSurfaceKHR.
 * Manages the lifetime of a Vulkan surface tied to a window.
 */
class BATLETH_API Surface {
public:
    /**
     * Create a surface for the given window.
     * @param instance Vulkan instance (must outlive this Surface)
     * @param window Borg window (provides platform abstraction)
     */
    Surface(VkInstance instance, borg::Window& window);
    ~Surface();

    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;
    Surface(Surface&&) noexcept;
    Surface& operator=(Surface&&) noexcept;

    auto get_handle() const -> VkSurfaceKHR { return m_surface; }

    /**
     * Query surface capabilities for a physical device.
     * Used during swapchain creation.
     */
    auto get_capabilities(VkPhysicalDevice device) const -> VkSurfaceCapabilitiesKHR;

    /**
     * Query supported surface formats.
     */
    auto get_formats(VkPhysicalDevice device) const -> std::vector<VkSurfaceFormatKHR>;

    /**
     * Query supported present modes.
     */
    auto get_present_modes(VkPhysicalDevice device) const -> std::vector<VkPresentModeKHR>;

private:
    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
};

} // namespace batleth
