#include "batleth/sync.hpp"
#include "federation/log.hpp"

#include <stdexcept>
#include <utility>

namespace batleth {

// ============================================================================
// Semaphore
// ============================================================================

Semaphore::Semaphore(const Config& config)
    : m_device(config.device) {

    if (m_device == VK_NULL_HANDLE) {
        throw std::runtime_error("Semaphore: device is null");
    }

    VkSemaphoreCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = config.flags
    };

    VkResult result = vkCreateSemaphore(m_device, &create_info, nullptr, &m_semaphore);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create semaphore");
    }

    FED_TRACE("Created VkSemaphore: {}", static_cast<void*>(m_semaphore));
}

Semaphore::~Semaphore() {
    if (m_semaphore != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
        FED_TRACE("Destroying VkSemaphore: {}", static_cast<void*>(m_semaphore));
        vkDestroySemaphore(m_device, m_semaphore, nullptr);
    }
}

Semaphore::Semaphore(Semaphore&& other) noexcept
    : m_device(other.m_device)
    , m_semaphore(other.m_semaphore) {
    other.m_device = VK_NULL_HANDLE;
    other.m_semaphore = VK_NULL_HANDLE;
}

auto Semaphore::operator=(Semaphore&& other) noexcept -> Semaphore& {
    if (this != &other) {
        // Destroy current resource
        if (m_semaphore != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device, m_semaphore, nullptr);
        }

        // Move from other
        m_device = other.m_device;
        m_semaphore = other.m_semaphore;

        other.m_device = VK_NULL_HANDLE;
        other.m_semaphore = VK_NULL_HANDLE;
    }
    return *this;
}

// ============================================================================
// Fence
// ============================================================================

Fence::Fence(const Config& config)
    : m_device(config.device) {

    if (m_device == VK_NULL_HANDLE) {
        throw std::runtime_error("Fence: device is null");
    }

    VkFenceCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = config.flags
    };

    VkResult result = vkCreateFence(m_device, &create_info, nullptr, &m_fence);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create fence");
    }

    FED_TRACE("Created VkFence: {} (signaled: {})",
        static_cast<void*>(m_fence),
        (config.flags & VK_FENCE_CREATE_SIGNALED_BIT) != 0);
}

Fence::~Fence() {
    if (m_fence != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
        FED_TRACE("Destroying VkFence: {}", static_cast<void*>(m_fence));
        vkDestroyFence(m_device, m_fence, nullptr);
    }
}

Fence::Fence(Fence&& other) noexcept
    : m_device(other.m_device)
    , m_fence(other.m_fence) {
    other.m_device = VK_NULL_HANDLE;
    other.m_fence = VK_NULL_HANDLE;
}

auto Fence::operator=(Fence&& other) noexcept -> Fence& {
    if (this != &other) {
        // Destroy current resource
        if (m_fence != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
            vkDestroyFence(m_device, m_fence, nullptr);
        }

        // Move from other
        m_device = other.m_device;
        m_fence = other.m_fence;

        other.m_device = VK_NULL_HANDLE;
        other.m_fence = VK_NULL_HANDLE;
    }
    return *this;
}

auto Fence::wait(uint64_t timeout_ns) -> VkResult {
    return vkWaitForFences(m_device, 1, &m_fence, VK_TRUE, timeout_ns);
}

auto Fence::reset() -> void {
    vkResetFences(m_device, 1, &m_fence);
}

auto Fence::is_signaled() const -> bool {
    return vkGetFenceStatus(m_device, m_fence) == VK_SUCCESS;
}

} // namespace batleth
