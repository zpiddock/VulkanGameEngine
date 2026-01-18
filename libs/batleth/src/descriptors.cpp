#include "batleth/descriptors.hpp"
#include "federation/log.hpp"
#include <cassert>
#include <stdexcept>

namespace batleth {
    // *************** Descriptor Set Layout Builder *********************

    auto DescriptorSetLayout::Builder::add_binding(
        uint32_t binding,
        VkDescriptorType descriptor_type,
        VkShaderStageFlags stage_flags,
        uint32_t count) -> DescriptorSetLayout::Builder & {
        assert(m_bindings.count(binding) == 0 && "Binding already in use");
        VkDescriptorSetLayoutBinding layout_binding{};
        layout_binding.binding = binding;
        layout_binding.descriptorType = descriptor_type;
        layout_binding.descriptorCount = count;
        layout_binding.stageFlags = stage_flags;
        m_bindings[binding] = layout_binding;
        return *this;
    }

    auto DescriptorSetLayout::Builder::build() const -> std::unique_ptr<DescriptorSetLayout> {
        return std::make_unique<DescriptorSetLayout>(m_device, m_bindings);
    }

    // *************** Descriptor Set Layout *********************

    DescriptorSetLayout::DescriptorSetLayout(
        VkDevice device, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings)
        : m_device{device}, m_bindings{bindings} {
        std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings{};
        for (auto kv: bindings) {
            set_layout_bindings.push_back(kv.second);
        }

        VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info{};
        descriptor_set_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptor_set_layout_info.bindingCount = static_cast<uint32_t>(set_layout_bindings.size());
        descriptor_set_layout_info.pBindings = set_layout_bindings.data();

        if (::vkCreateDescriptorSetLayout(m_device, &descriptor_set_layout_info, nullptr, &m_layout) != VK_SUCCESS) {
            FED_ERROR("Failed to create descriptor set layout");
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    DescriptorSetLayout::~DescriptorSetLayout() {
        ::vkDestroyDescriptorSetLayout(m_device, m_layout, nullptr);
    }

    // *************** Descriptor Pool Builder *********************

    auto DescriptorPool::Builder::add_pool_size(VkDescriptorType descriptor_type,
                                                uint32_t count) -> DescriptorPool::Builder & {
        m_pool_sizes.push_back({descriptor_type, count});
        return *this;
    }

    auto DescriptorPool::Builder::set_pool_flags(VkDescriptorPoolCreateFlags flags) -> DescriptorPool::Builder & {
        m_pool_flags = flags;
        return *this;
    }

    auto DescriptorPool::Builder::set_max_sets(uint32_t count) -> DescriptorPool::Builder & {
        m_max_sets = count;
        return *this;
    }

    auto DescriptorPool::Builder::build() const -> std::unique_ptr<DescriptorPool> {
        return std::make_unique<DescriptorPool>(m_device, m_max_sets, m_pool_flags, m_pool_sizes);
    }

    // *************** Descriptor Pool *********************

    DescriptorPool::DescriptorPool(
        VkDevice device,
        uint32_t max_sets,
        VkDescriptorPoolCreateFlags pool_flags,
        const std::vector<VkDescriptorPoolSize> &pool_sizes)
        : m_device{device} {
        VkDescriptorPoolCreateInfo descriptor_pool_info{};
        descriptor_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptor_pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        descriptor_pool_info.pPoolSizes = pool_sizes.data();
        descriptor_pool_info.maxSets = max_sets;
        descriptor_pool_info.flags = pool_flags;

        if (::vkCreateDescriptorPool(m_device, &descriptor_pool_info, nullptr, &m_pool) != VK_SUCCESS) {
            FED_ERROR("Failed to create descriptor pool");
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    DescriptorPool::~DescriptorPool() {
        ::vkDestroyDescriptorPool(m_device, m_pool, nullptr);
    }

    auto DescriptorPool::allocate_descriptor_set(
        const VkDescriptorSetLayout descriptor_set_layout, VkDescriptorSet &descriptor) const -> bool {
        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = m_pool;
        alloc_info.pSetLayouts = &descriptor_set_layout;
        alloc_info.descriptorSetCount = 1;

        if (::vkAllocateDescriptorSets(m_device, &alloc_info, &descriptor) != VK_SUCCESS) {
            return false;
        }
        return true;
    }

    auto DescriptorPool::free_descriptors(std::vector<VkDescriptorSet> &descriptors) const -> void {
        ::vkFreeDescriptorSets(
            m_device,
            m_pool,
            static_cast<uint32_t>(descriptors.size()),
            descriptors.data());
    }

    auto DescriptorPool::reset_pool() -> void {
        ::vkResetDescriptorPool(m_device, m_pool, 0);
    }

    // *************** Descriptor Writer *********************

    DescriptorWriter::DescriptorWriter(DescriptorSetLayout &set_layout, DescriptorPool &pool)
        : m_set_layout{set_layout}, m_pool{pool} {
    }

    auto DescriptorWriter::write_buffer(uint32_t binding, VkDescriptorBufferInfo *buffer_info) -> DescriptorWriter & {
        assert(m_set_layout.m_bindings.count(binding) == 1 && "Layout does not contain specified binding");

        auto &binding_description = m_set_layout.m_bindings[binding];

        assert(
            binding_description.descriptorCount == 1 && "Binding single descriptor info, but binding expects multiple");

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorType = binding_description.descriptorType;
        write.dstBinding = binding;
        write.pBufferInfo = buffer_info;
        write.descriptorCount = 1;

        m_writes.push_back(write);
        return *this;
    }

    auto DescriptorWriter::write_image(uint32_t binding, VkDescriptorImageInfo *image_info) -> DescriptorWriter & {
        assert(m_set_layout.m_bindings.count(binding) == 1 && "Layout does not contain specified binding");

        auto &binding_description = m_set_layout.m_bindings[binding];

        assert(
            binding_description.descriptorCount == 1 && "Binding single descriptor info, but binding expects multiple");

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorType = binding_description.descriptorType;
        write.dstBinding = binding;
        write.pImageInfo = image_info;
        write.descriptorCount = 1;

        m_writes.push_back(write);
        return *this;
    }

    auto DescriptorWriter::build(VkDescriptorSet &set) -> bool {
        bool success = m_pool.allocate_descriptor_set(m_set_layout.get_layout(), set);
        if (!success) {
            return false;
        }
        overwrite(set);
        return true;
    }

    auto DescriptorWriter::overwrite(VkDescriptorSet &set) -> void {
        for (auto &write: m_writes) {
            write.dstSet = set;
        }
        ::vkUpdateDescriptorSets(m_pool.m_device, static_cast<uint32_t>(m_writes.size()), m_writes.data(), 0, nullptr);
    }
} // namespace batleth