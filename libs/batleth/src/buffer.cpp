#include "batleth/buffer.hpp"
#include "batleth/device.hpp"
#include "federation/log.hpp"

#include <cassert>
#include <cstring>

namespace batleth {
    auto Buffer::get_alignment(VkDeviceSize instance_size, VkDeviceSize min_offset_alignment) -> VkDeviceSize {
        if (min_offset_alignment > 0) {
            return (instance_size + min_offset_alignment - 1) & ~(min_offset_alignment - 1);
        }
        return instance_size;
    }

    Buffer::Buffer(
        Device &device,
        VkDeviceSize instance_size,
        std::uint32_t instance_count,
        VkBufferUsageFlags usage_flags,
        VkMemoryPropertyFlags memory_property_flags,
        VkDeviceSize min_offset_alignment)
        : m_device{device}
          , m_instance_count{instance_count}
          , m_instance_size{instance_size}
          , m_usage_flags{usage_flags}
          , m_memory_property_flags{memory_property_flags} {
        m_alignment_size = get_alignment(instance_size, min_offset_alignment);
        m_buffer_size = m_alignment_size * instance_count;

        device.create_buffer(m_buffer_size, usage_flags, memory_property_flags, m_buffer, m_memory);
    }

    Buffer::~Buffer() {
        unmap();
        ::vkDestroyBuffer(m_device.get_logical_device(), m_buffer, nullptr);
        ::vkFreeMemory(m_device.get_logical_device(), m_memory, nullptr);
    }

    auto Buffer::map(VkDeviceSize size, VkDeviceSize offset) -> VkResult {
        assert(m_buffer && m_memory && "Called map on buffer before create");
        return ::vkMapMemory(m_device.get_logical_device(), m_memory, offset, size, 0, &m_mapped);
    }

    auto Buffer::unmap() -> void {
        if (m_mapped) {
            ::vkUnmapMemory(m_device.get_logical_device(), m_memory);
            m_mapped = nullptr;
        }
    }

    auto Buffer::write_to_buffer(void *data, VkDeviceSize size, VkDeviceSize offset) -> void {
        assert(m_mapped && "Cannot copy to unmapped buffer");

        if (size == VK_WHOLE_SIZE) {
            std::memcpy(m_mapped, data, m_buffer_size);
        } else {
            char *mem_offset = static_cast<char *>(m_mapped) + offset;
            std::memcpy(mem_offset, data, size);
        }
    }

    auto Buffer::flush(VkDeviceSize size, VkDeviceSize offset) -> VkResult {
        VkMappedMemoryRange mapped_range{};
        mapped_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mapped_range.memory = m_memory;
        mapped_range.offset = offset;
        mapped_range.size = size;
        return ::vkFlushMappedMemoryRanges(m_device.get_logical_device(), 1, &mapped_range);
    }

    auto Buffer::invalidate(VkDeviceSize size, VkDeviceSize offset) -> VkResult {
        VkMappedMemoryRange mapped_range{};
        mapped_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mapped_range.memory = m_memory;
        mapped_range.offset = offset;
        mapped_range.size = size;
        return ::vkInvalidateMappedMemoryRanges(m_device.get_logical_device(), 1, &mapped_range);
    }

    auto Buffer::descriptor_info(VkDeviceSize size, VkDeviceSize offset) -> VkDescriptorBufferInfo {
        return VkDescriptorBufferInfo{m_buffer, offset, size};
    }

    auto Buffer::write_to_index(void *data, int index) -> void {
        write_to_buffer(data, m_instance_size, index * m_alignment_size);
    }

    auto Buffer::flush_index(int index) -> VkResult {
        return flush(m_alignment_size, index * m_alignment_size);
    }

    auto Buffer::descriptor_info_for_index(int index) -> VkDescriptorBufferInfo {
        return descriptor_info(m_alignment_size, index * m_alignment_size);
    }
} // namespace batleth