#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

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
 * RAII wrapper for VkCommandPool and VkCommandBuffer.
 * Manages command buffer allocation and recording.
 */
    class BATLETH_API CommandBuffer {
    public:
        struct Config {
            VkDevice device = VK_NULL_HANDLE;
            std::uint32_t queue_family_index = 0;
            std::uint32_t buffer_count = 2;
        };

        explicit CommandBuffer(const Config &config);

        ~CommandBuffer();

        CommandBuffer(const CommandBuffer &) = delete;

        CommandBuffer &operator=(const CommandBuffer &) = delete;

        CommandBuffer(CommandBuffer &&) noexcept;

        CommandBuffer &operator=(CommandBuffer &&) noexcept;

        auto get_command_pool() const -> VkCommandPool { return m_command_pool; }
        auto get_command_buffers() const -> const std::vector<VkCommandBuffer> & { return m_command_buffers; }
        auto get_command_buffer(std::size_t index) const -> VkCommandBuffer { return m_command_buffers[index]; }

        /**
     * Resets the command pool, invalidating all allocated command buffers.
     */
        auto reset() -> void;

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkCommandPool m_command_pool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> m_command_buffers;
    };
} // namespace batleth