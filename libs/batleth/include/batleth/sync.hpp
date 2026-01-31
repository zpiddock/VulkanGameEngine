#pragma once

#include <vulkan/vulkan.h>

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
 * RAII wrapper for VkSemaphore
 * Used for GPU-GPU synchronization (e.g., between queue submissions)
 */
class BATLETH_API Semaphore {
public:
    struct Config {
        VkDevice device = VK_NULL_HANDLE;
        VkSemaphoreCreateFlags flags = 0;
    };

    explicit Semaphore(const Config& config);
    ~Semaphore();

    // Delete copy operations
    Semaphore(const Semaphore&) = delete;
    auto operator=(const Semaphore&) -> Semaphore& = delete;

    // Move operations
    Semaphore(Semaphore&& other) noexcept;
    auto operator=(Semaphore&& other) noexcept -> Semaphore&;

    [[nodiscard]] auto get_handle() const -> VkSemaphore { return m_semaphore; }
    [[nodiscard]] auto get_handle_ptr() -> VkSemaphore* { return &m_semaphore; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkSemaphore m_semaphore = VK_NULL_HANDLE;
};

/**
 * RAII wrapper for VkFence
 * Used for CPU-GPU synchronization (e.g., waiting for frame completion)
 */
class BATLETH_API Fence {
public:
    struct Config {
        VkDevice device = VK_NULL_HANDLE;
        VkFenceCreateFlags flags = 0;  // Use VK_FENCE_CREATE_SIGNALED_BIT for pre-signaled
    };

    explicit Fence(const Config& config);
    ~Fence();

    // Delete copy operations
    Fence(const Fence&) = delete;
    auto operator=(const Fence&) -> Fence& = delete;

    // Move operations
    Fence(Fence&& other) noexcept;
    auto operator=(Fence&& other) noexcept -> Fence&;

    [[nodiscard]] auto get_handle() const -> VkFence { return m_fence; }
    [[nodiscard]] auto get_handle_ptr() -> VkFence* { return &m_fence; }

    /**
     * Wait for the fence to be signaled
     * @param timeout_ns Timeout in nanoseconds (default: max uint64)
     * @return VK_SUCCESS if signaled, VK_TIMEOUT if timed out
     */
    auto wait(uint64_t timeout_ns = UINT64_MAX) -> VkResult;

    /**
     * Reset the fence to unsignaled state
     */
    auto reset() -> void;

    /**
     * Check if the fence is currently signaled (non-blocking)
     */
    [[nodiscard]] auto is_signaled() const -> bool;

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkFence m_fence = VK_NULL_HANDLE;
};

} // namespace batleth
