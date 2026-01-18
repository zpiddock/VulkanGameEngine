#pragma once

#include <vulkan/vulkan.h>

#include "frame_info.hpp"

#ifdef _WIN32
#ifdef KLINGON_EXPORTS
#define KLINGON_API __declspec(dllexport)
#else
#define KLINGON_API __declspec(dllimport)
#endif
#else
#define KLINGON_API
#endif

namespace klingon {
    /**
 * Interface for render systems that can be registered with the renderer.
 * Provides hooks for updating state, rendering, and handling swapchain recreation.
 */
    class KLINGON_API IRenderSystem {
    public:
        virtual ~IRenderSystem() = default;

        /**
     * Called before rendering starts (update UBO data, animations, etc.)
     * @param frame_info Frame context with command buffer, camera, game objects
     * @param ubo Global uniform buffer to update (lights, camera matrices, etc.)
     */
        virtual auto update(FrameInfo &frame_info, GlobalUbo &ubo) -> void {
        }

        /**
     * Called during rendering pass to record draw commands
     * @param frame_info Frame context with command buffer, camera, game objects
     */
        virtual auto render(FrameInfo &frame_info) -> void = 0;

        /**
     * Called when swapchain is recreated (rebuild pipelines with new format)
     * @param format New swapchain format
     */
        virtual auto on_swapchain_recreate(VkFormat format) -> void {
        }
    };
} // namespace klingon