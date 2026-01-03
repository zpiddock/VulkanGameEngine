#pragma once

#include <memory>
#include <cstdint>
#include <vulkan/vulkan.h>

#ifdef _WIN32
    #ifdef KLINGON_EXPORTS
        #define KLINGON_API __declspec(dllexport)
    #else
        #define KLINGON_API __declspec(dllimport)
    #endif
#else
    #define KLINGON_API
#endif

namespace borg { class Window; }
namespace batleth {
    class Instance;
    class Device;
    class Swapchain;
}

namespace klingon {

/**
 * Manages the Vulkan rendering pipeline and resources.
 * Encapsulates all Vulkan-specific rendering logic.
 */
class KLINGON_API Renderer {
public:
    struct Config {
        const char* application_name = "Klingon Application";
        std::uint32_t application_version = 1;
        bool enable_validation = true;
    };

    Renderer(const Config& config, borg::Window& window);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = default;
    Renderer& operator=(Renderer&&) = default;

    auto begin_frame() -> bool;
    auto end_frame() -> void;
    auto wait_idle() -> void;

private:
    auto create_instance() -> void;
    auto create_device() -> void;
    auto create_swapchain() -> void;

    borg::Window& m_window;
    Config m_config;

    std::unique_ptr<batleth::Instance> m_instance;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    std::unique_ptr<batleth::Device> m_device;
    std::unique_ptr<batleth::Swapchain> m_swapchain;
};

} // namespace klingon
