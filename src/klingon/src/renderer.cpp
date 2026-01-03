#include "klingon/renderer.hpp"
#include "borg/window.hpp"
#include "batleth/instance.hpp"
#include "batleth/device.hpp"
#include "batleth/swapchain.hpp"
#include "federation/log.hpp"

#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vector>

namespace klingon {

Renderer::Renderer(const Config& config, borg::Window& window)
    : m_window(window), m_config(config) {
    FED_INFO("Initializing renderer");

    create_instance();
    create_device();
    create_swapchain();

    FED_INFO("Renderer initialized successfully");
}

Renderer::~Renderer() {
    FED_DEBUG("Destroying renderer");

    if (m_device) {
        m_device->wait_idle();
    }

    // Cleanup in reverse order of creation
    m_swapchain.reset();
    m_device.reset();

    if (m_surface != VK_NULL_HANDLE && m_instance) {
        ::vkDestroySurfaceKHR(m_instance->get_handle(), m_surface, nullptr);
    }

    m_instance.reset();

    FED_DEBUG("Renderer destroyed successfully");
}

auto Renderer::begin_frame() -> bool {
    // TODO: Acquire next swapchain image
    // TODO: Begin command buffer recording
    return true;
}

auto Renderer::end_frame() -> void {
    // TODO: End command buffer recording
    // TODO: Submit command buffer
    // TODO: Present swapchain image
}

auto Renderer::wait_idle() -> void {
    if (m_device) {
        m_device->wait_idle();
    }
}

auto Renderer::create_instance() -> void {
    FED_DEBUG("Creating Vulkan instance");

    // FED_DEBUG("GLFW Initialized: {}", static_cast<bool>(glfwInit()));

    // Check if Vulkan is supported by GLFW
    if (!::glfwVulkanSupported()) {
        FED_ERROR("Vulkan is not supported by GLFW");
        throw std::runtime_error("Vulkan is not supported");
    }

    FED_DEBUG("Vulkan is supported");

    // List all available Vulkan extensions
    std::uint32_t extension_count = 0;
    ::vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> available_extensions(extension_count);
    ::vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, available_extensions.data());

    FED_INFO("Available Vulkan extensions ({}):", extension_count);
    for (const auto& extension : available_extensions) {
        FED_DEBUG(" - {} (version {})", extension.extensionName, extension.specVersion);
    }

    // Get required GLFW extensions
    std::uint32_t glfw_extension_count = 0;
    const char** glfw_extensions = ::glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    if (!glfw_extensions) {
        FED_ERROR("Failed to get required GLFW extensions");
        throw std::runtime_error("GLFW could not find required Vulkan extensions");
    }

    std::vector extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

    FED_DEBUG("GLFW requires {} Vulkan extensions", glfw_extension_count);
    for (std::uint32_t i = 0; i < glfw_extension_count; ++i) {
        FED_DEBUG(" - {}", glfw_extensions[i]);
    }

    // Add debug utils extension if validation is enabled
    if (m_config.enable_validation) {
        extensions.push_back("VK_EXT_debug_utils");
        FED_DEBUG("  - VK_EXT_debug_utils (for validation)");
    }

    batleth::Instance::Config instance_config{};
    instance_config.application_name = m_config.application_name;
    instance_config.application_version = m_config.application_version;
    instance_config.engine_name = "Klingon Engine";
    instance_config.engine_version = 1;
    instance_config.extensions = extensions;
    instance_config.enable_validation = m_config.enable_validation;

    if (m_config.enable_validation) {
        instance_config.validation_layers = {"VK_LAYER_KHRONOS_validation"};
    }

    m_instance = std::make_unique<batleth::Instance>(instance_config);
}

auto Renderer::create_device() -> void {
    FED_DEBUG("Creating Vulkan device");

    // Create window surface
    VkResult result = ::glfwCreateWindowSurface(m_instance->get_handle(), m_window.get_native_handle(),
                                                 nullptr, &m_surface);
    if (result != VK_SUCCESS) {
        FED_ERROR("Failed to create window surface, VkResult: {}", static_cast<int>(result));
        throw std::runtime_error("Failed to create window surface");
    }

    FED_DEBUG("Window surface created successfully");

    batleth::Device::Config device_config{};
    device_config.instance = m_instance->get_handle();
    device_config.surface = m_surface;
    device_config.device_extensions = {"VK_KHR_swapchain"};

    m_device = std::make_unique<batleth::Device>(device_config);
}

auto Renderer::create_swapchain() -> void {
    FED_DEBUG("Creating Vulkan swapchain");

    auto [width, height] = m_window.get_framebuffer_size();

    batleth::Swapchain::Config swapchain_config{};
    swapchain_config.device = m_device->get_logical_device();
    swapchain_config.physical_device = m_device->get_physical_device();
    swapchain_config.surface = m_device->get_surface();
    swapchain_config.width = width;
    swapchain_config.height = height;

    m_swapchain = std::make_unique<batleth::Swapchain>(swapchain_config);
}

} // namespace klingon
