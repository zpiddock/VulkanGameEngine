#include "batleth/command_buffer.hpp"
#include <stdexcept>

namespace batleth {

CommandBuffer::CommandBuffer(const Config& config) : m_device(config.device) {
    // Create command pool
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = config.queue_family_index;

    if (::vkCreateCommandPool(m_device, &pool_info, nullptr, &m_command_pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }

    // Allocate command buffers
    m_command_buffers.resize(config.buffer_count);

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = m_command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = static_cast<std::uint32_t>(m_command_buffers.size());

    if (::vkAllocateCommandBuffers(m_device, &alloc_info, m_command_buffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }
}

CommandBuffer::~CommandBuffer() {
    if (m_command_pool != VK_NULL_HANDLE) {
        ::vkDestroyCommandPool(m_device, m_command_pool, nullptr);
    }
}

CommandBuffer::CommandBuffer(CommandBuffer&& other) noexcept
    : m_device(other.m_device)
    , m_command_pool(other.m_command_pool)
    , m_command_buffers(std::move(other.m_command_buffers)) {
    other.m_device = VK_NULL_HANDLE;
    other.m_command_pool = VK_NULL_HANDLE;
}

auto CommandBuffer::operator=(CommandBuffer&& other) noexcept -> CommandBuffer& {
    if (this != &other) {
        if (m_command_pool != VK_NULL_HANDLE) {
            ::vkDestroyCommandPool(m_device, m_command_pool, nullptr);
        }

        m_device = other.m_device;
        m_command_pool = other.m_command_pool;
        m_command_buffers = std::move(other.m_command_buffers);

        other.m_device = VK_NULL_HANDLE;
        other.m_command_pool = VK_NULL_HANDLE;
    }
    return *this;
}

auto CommandBuffer::reset() -> void {
    ::vkResetCommandPool(m_device, m_command_pool, 0);
}

} // namespace batleth
