#pragma once

#include <batleth/sync.hpp>
#include <batleth/command_buffer.hpp>
#include <vulkan/vulkan.h>
#include <memory>
#include <cstdint>

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
 * Per-frame GPU resources and synchronization primitives.
 *
 * Each frame in flight has its own FrameContext containing:
 * - Semaphore for swapchain image acquisition
 * - Semaphore for render completion signaling
 * - Fence for CPU-GPU synchronization
 * - Command buffer for recording draw commands
 *
 * This enables double/triple buffering where the CPU can prepare
 * frame N+1 while the GPU is still rendering frame N.
 */
struct KLINGON_API FrameContext {
    // Synchronization primitives (RAII managed)
    std::unique_ptr<batleth::Semaphore> image_available_semaphore;
    std::unique_ptr<batleth::Semaphore> render_finished_semaphore;
    std::unique_ptr<batleth::Fence> in_flight_fence;

    // Command buffer for this frame (non-owning, from shared pool)
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;

    // Frame timing
    float delta_time = 0.0f;
    float total_time = 0.0f;

    /**
     * Create a fully initialized frame context
     */
    static auto create(VkDevice device) -> FrameContext {
        FrameContext ctx;

        ctx.image_available_semaphore = std::make_unique<batleth::Semaphore>(
            batleth::Semaphore::Config{.device = device}
        );

        ctx.render_finished_semaphore = std::make_unique<batleth::Semaphore>(
            batleth::Semaphore::Config{.device = device}
        );

        ctx.in_flight_fence = std::make_unique<batleth::Fence>(
            batleth::Fence::Config{
                .device = device,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT  // Start signaled so first wait succeeds
            }
        );

        return ctx;
    }

    // Convenience accessors for raw handles (needed for Vulkan API calls)
    [[nodiscard]] auto get_image_available_semaphore() const -> VkSemaphore {
        return image_available_semaphore ? image_available_semaphore->get_handle() : VK_NULL_HANDLE;
    }

    [[nodiscard]] auto get_render_finished_semaphore() const -> VkSemaphore {
        return render_finished_semaphore ? render_finished_semaphore->get_handle() : VK_NULL_HANDLE;
    }

    [[nodiscard]] auto get_fence() const -> VkFence {
        return in_flight_fence ? in_flight_fence->get_handle() : VK_NULL_HANDLE;
    }

    /**
     * Wait for this frame's GPU work to complete
     */
    auto wait_for_fence(uint64_t timeout_ns = UINT64_MAX) -> VkResult {
        if (in_flight_fence) {
            return in_flight_fence->wait(timeout_ns);
        }
        return VK_SUCCESS;
    }

    /**
     * Reset fence for reuse
     */
    auto reset_fence() -> void {
        if (in_flight_fence) {
            in_flight_fence->reset();
        }
    }
};

/**
 * Maximum number of frames that can be in flight simultaneously.
 * 2 = double buffering (good balance of latency vs throughput)
 * 3 = triple buffering (higher throughput, slightly more latency)
 */
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

} // namespace klingon
