#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <functional>

#ifdef _WIN32
    #ifdef KLINGON_EXPORTS
        #define KLINGON_API __declspec(dllexport)
    #else
        #define KLINGON_API __declspec(dllimport)
    #endif
#else
    #define KLINGON_API
#endif

// Forward declarations
struct ImGui_ImplVulkan_InitInfo;
struct GLFWwindow;

namespace batleth {
    class Device;
    class DescriptorPool;
}

namespace klingon {

/**
 * Manages ImGui context and Vulkan integration.
 * Handles initialization, frame management, and rendering.
 */
class KLINGON_API ImGuiContext {
public:
    struct Config {
        bool enable_docking = true;
        bool enable_viewports = false;  // Multi-viewport support (experimental)
        float font_size = 16.0f;
    };

    ImGuiContext(GLFWwindow* window,
                 batleth::Device& device,
                 VkFormat color_format,
                 VkFormat depth_format,
                 std::uint32_t image_count);
    ImGuiContext(GLFWwindow* window,
                 batleth::Device& device,
                 VkFormat color_format,
                 VkFormat depth_format,
                 std::uint32_t image_count,
                 const Config& config);
    ~ImGuiContext();

    ImGuiContext(const ImGuiContext&) = delete;
    ImGuiContext& operator=(const ImGuiContext&) = delete;
    ImGuiContext(ImGuiContext&&) = delete;
    ImGuiContext& operator=(ImGuiContext&&) = delete;

    /**
     * Begin a new ImGui frame.
     * Call this at the start of your frame, before any ImGui calls.
     */
    auto begin_frame() -> void;

    /**
     * End the ImGui frame and prepare render data.
     * Call this after all ImGui calls, before rendering.
     */
    auto end_frame() -> void;

    /**
     * Render ImGui draw data to the given command buffer.
     * @param command_buffer Active command buffer (must be in recording state)
     */
    auto render(VkCommandBuffer command_buffer) -> void;

    /**
     * Handle window resize.
     */
    auto on_resize(std::uint32_t width, std::uint32_t height) -> void;

private:
    auto setup_style() -> void;

    batleth::Device* m_device;
    std::unique_ptr<batleth::DescriptorPool> m_descriptor_pool;
    Config m_config;
};

} // namespace klingon
