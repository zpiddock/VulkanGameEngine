#pragma once

#include <vulkan/vulkan.h>
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
    class Device;

    /**
 * RAII wrapper for Vulkan buffers with memory management.
 * Supports both host-visible (mappable) and device-local buffers.
 */
    class BATLETH_API Buffer {
    public:
        Buffer(
            Device &device,
            VkDeviceSize instance_size,
            std::uint32_t instance_count,
            VkBufferUsageFlags usage_flags,
            VkMemoryPropertyFlags memory_property_flags,
            VkDeviceSize min_offset_alignment = 1
        );

        ~Buffer();

        Buffer(const Buffer &) = delete;

        Buffer &operator=(const Buffer &) = delete;

        /**
     * Map the buffer memory for CPU access.
     * Only valid for host-visible buffers.
     * @return VK_SUCCESS on success
     */
        auto map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) -> VkResult;

        /**
     * Unmap the buffer memory.
     */
        auto unmap() -> void;

        /**
     * Write data to the mapped buffer.
     * @param data Pointer to data to write
     * @param size Size of data (VK_WHOLE_SIZE for entire buffer)
     * @param offset Offset into buffer
     */
        auto write_to_buffer(void *data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) -> void;

        /**
     * Flush the buffer memory to make writes visible to device.
     * Required for non-coherent memory after writing.
     * @param size Size to flush (VK_WHOLE_SIZE for entire buffer)
     * @param offset Offset into buffer
     * @return VK_SUCCESS on success
     */
        auto flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) -> VkResult;

        /**
     * Invalidate the buffer memory to make device writes visible to host.
     * Required for non-coherent memory before reading.
     * @param size Size to invalidate (VK_WHOLE_SIZE for entire buffer)
     * @param offset Offset into buffer
     * @return VK_SUCCESS on success
     */
        auto invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) -> VkResult;

        /**
     * Get descriptor info for this buffer (for use with descriptor sets).
     * @param size Size of the descriptor range (VK_WHOLE_SIZE for entire buffer)
     * @param offset Offset into buffer
     * @return VkDescriptorBufferInfo for this buffer
     */
        auto descriptor_info(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) -> VkDescriptorBufferInfo;

        /**
     * Write data to a specific instance in the buffer.
     * @param data Pointer to data to write
     * @param index Instance index
     */
        auto write_to_index(void *data, int index) -> void;

        /**
     * Flush a specific instance in the buffer.
     * @param index Instance index
     * @return VK_SUCCESS on success
     */
        auto flush_index(int index) -> VkResult;

        /**
     * Get descriptor info for a specific instance.
     * @param index Instance index
     * @return VkDescriptorBufferInfo for the instance
     */
        auto descriptor_info_for_index(int index) -> VkDescriptorBufferInfo;

        [[nodiscard]] auto get_buffer() const -> VkBuffer { return m_buffer; }
        [[nodiscard]] auto get_mapped_memory() const -> void * { return m_mapped; }
        [[nodiscard]] auto get_instance_count() const -> std::uint32_t { return m_instance_count; }
        [[nodiscard]] auto get_instance_size() const -> VkDeviceSize { return m_instance_size; }
        [[nodiscard]] auto get_alignment_size() const -> VkDeviceSize { return m_alignment_size; }
        [[nodiscard]] auto get_usage_flags() const -> VkBufferUsageFlags { return m_usage_flags; }

        [[nodiscard]] auto get_memory_property_flags() const -> VkMemoryPropertyFlags {
            return m_memory_property_flags;
        }

        [[nodiscard]] auto get_buffer_size() const -> VkDeviceSize { return m_buffer_size; }

    private:
        /**
     * Calculate the aligned size based on min offset alignment.
     */
        static auto get_alignment(VkDeviceSize instance_size, VkDeviceSize min_offset_alignment) -> VkDeviceSize;

        Device &m_device;
        void *m_mapped = nullptr;
        VkBuffer m_buffer = VK_NULL_HANDLE;
        VkDeviceMemory m_memory = VK_NULL_HANDLE;

        VkDeviceSize m_buffer_size;
        std::uint32_t m_instance_count;
        VkDeviceSize m_instance_size;
        VkDeviceSize m_alignment_size;
        VkBufferUsageFlags m_usage_flags;
        VkMemoryPropertyFlags m_memory_property_flags;
    };
} // namespace batleth