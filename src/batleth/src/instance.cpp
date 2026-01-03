#include "batleth/instance.hpp"
#include "federation/log.hpp"
#include <stdexcept>
#include <cstring>

namespace batleth {

namespace {
    VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data) {

        (void)message_type;
        (void)user_data;

        // Map Vulkan severity to Federation log levels
        switch (message_severity) {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
                FED_TRACE("[Vulkan] {}", callback_data->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                FED_INFO("[Vulkan] {}", callback_data->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                FED_WARN("[Vulkan] {}", callback_data->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                FED_ERROR("[Vulkan] {}", callback_data->pMessage);
                break;
            default:
                FED_DEBUG("[Vulkan] {}", callback_data->pMessage);
                break;
        }

        return VK_FALSE;
    }
}

Instance::Instance(const Config& config) : m_validation_enabled(config.enable_validation) {
    FED_INFO("Creating Vulkan instance: {} (API 1.3)", config.application_name);

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = config.application_name.c_str();
    app_info.applicationVersion = config.application_version;
    app_info.pEngineName = config.engine_name.c_str();
    app_info.engineVersion = config.engine_version;
    app_info.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<std::uint32_t>(config.extensions.size());
    create_info.ppEnabledExtensionNames = config.extensions.data();

    if (m_validation_enabled && !config.validation_layers.empty()) {
        FED_DEBUG("Enabling {} validation layers", config.validation_layers.size());
        create_info.enabledLayerCount = static_cast<std::uint32_t>(config.validation_layers.size());
        create_info.ppEnabledLayerNames = config.validation_layers.data();
    }

    if (::vkCreateInstance(&create_info, nullptr, &m_instance) != VK_SUCCESS) {
        FED_FATAL("Failed to create Vulkan instance");
        throw std::runtime_error("Failed to create Vulkan instance");
    }

    FED_DEBUG("Vulkan instance created successfully");

    if (m_validation_enabled) {
        setup_debug_messenger();
    }
}

Instance::~Instance() {
    if (m_debug_messenger != VK_NULL_HANDLE) {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            ::vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (func) {
            func(m_instance, m_debug_messenger, nullptr);
        }
    }

    if (m_instance != VK_NULL_HANDLE) {
        FED_DEBUG("Destroying Vulkan instance");
        ::vkDestroyInstance(m_instance, nullptr);
        FED_DEBUG("Vulkan instance destroyed successfully");
    }
}

Instance::Instance(Instance&& other) noexcept
    : m_instance(other.m_instance)
    , m_debug_messenger(other.m_debug_messenger)
    , m_validation_enabled(other.m_validation_enabled) {
    other.m_instance = VK_NULL_HANDLE;
    other.m_debug_messenger = VK_NULL_HANDLE;
}

auto Instance::operator=(Instance&& other) noexcept -> Instance& {
    if (this != &other) {
        if (m_instance != VK_NULL_HANDLE) {
            ::vkDestroyInstance(m_instance, nullptr);
        }

        m_instance = other.m_instance;
        m_debug_messenger = other.m_debug_messenger;
        m_validation_enabled = other.m_validation_enabled;

        other.m_instance = VK_NULL_HANDLE;
        other.m_debug_messenger = VK_NULL_HANDLE;
    }
    return *this;
}

auto Instance::setup_debug_messenger() -> void {
    FED_DEBUG("Setting up Vulkan debug messenger");

    VkDebugUtilsMessengerCreateInfoEXT create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = debug_callback;

    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        ::vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));

    if (func && func(m_instance, &create_info, nullptr, &m_debug_messenger) != VK_SUCCESS) {
        FED_ERROR("Failed to set up debug messenger");
        throw std::runtime_error("Failed to set up debug messenger");
    }

    FED_DEBUG("Vulkan debug messenger active");
}

} // namespace batleth
