#include "batleth/surface.hpp"
#include "borg/window.hpp"
#include "federation/log.hpp"

#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vector>

namespace batleth {
    Surface::Surface(VkInstance instance, borg::Window &window)
        : m_instance(instance) {
        FED_DEBUG("Creating Vulkan surface");

        // Create platform-specific surface through Borg's window abstraction
        VkResult result = ::glfwCreateWindowSurface(m_instance, window.get_native_handle(), nullptr, &m_surface);
        if (result != VK_SUCCESS) {
            FED_ERROR("Failed to create window surface");
            throw std::runtime_error("Failed to create window surface");
        }

        FED_DEBUG("Vulkan surface created successfully");
    }

    Surface::~Surface() {
        if (m_surface != VK_NULL_HANDLE) {
            FED_DEBUG("Destroying Vulkan surface");
            ::vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
            FED_DEBUG("Vulkan surface destroyed");
        }
    }

    Surface::Surface(Surface &&other) noexcept
        : m_instance(other.m_instance)
          , m_surface(other.m_surface) {
        other.m_instance = VK_NULL_HANDLE;
        other.m_surface = VK_NULL_HANDLE;
    }

    Surface &Surface::operator=(Surface &&other) noexcept {
        if (this != &other) {
            // Clean up existing resources
            if (m_surface != VK_NULL_HANDLE) {
                ::vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
            }

            // Move from other
            m_instance = other.m_instance;
            m_surface = other.m_surface;

            // Clear other
            other.m_instance = VK_NULL_HANDLE;
            other.m_surface = VK_NULL_HANDLE;
        }
        return *this;
    }

    auto Surface::get_capabilities(VkPhysicalDevice device) const -> VkSurfaceCapabilitiesKHR {
        VkSurfaceCapabilitiesKHR capabilities;
        ::vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &capabilities);
        return capabilities;
    }

    auto Surface::get_formats(VkPhysicalDevice device) const -> std::vector<VkSurfaceFormatKHR> {
        std::uint32_t format_count = 0;
        ::vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, nullptr);

        std::vector<VkSurfaceFormatKHR> formats;
        if (format_count != 0) {
            formats.resize(format_count);
            ::vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, formats.data());
        }

        return formats;
    }

    auto Surface::get_present_modes(VkPhysicalDevice device) const -> std::vector<VkPresentModeKHR> {
        std::uint32_t present_mode_count = 0;
        ::vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, nullptr);

        std::vector<VkPresentModeKHR> present_modes;
        if (present_mode_count != 0) {
            present_modes.resize(present_mode_count);
            ::vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, present_modes.data());
        }

        return present_modes;
    }
} // namespace batleth