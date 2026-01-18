#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <unordered_map>
#include <vector>

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
    class BATLETH_API DescriptorSetLayout {
    public:
        class BATLETH_API Builder {
        public:
            explicit Builder(VkDevice device) : m_device(device) {
            }

            Builder &add_binding(
                uint32_t binding,
                VkDescriptorType descriptor_type,
                VkShaderStageFlags stage_flags,
                uint32_t count = 1);

            std::unique_ptr<DescriptorSetLayout> build() const;

        private:
            VkDevice m_device;
            std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> m_bindings{};
        };

        DescriptorSetLayout(VkDevice device, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings);

        ~DescriptorSetLayout();

        DescriptorSetLayout(const DescriptorSetLayout &) = delete;

        DescriptorSetLayout &operator=(const DescriptorSetLayout &) = delete;

        [[nodiscard]] auto get_layout() const -> VkDescriptorSetLayout { return m_layout; }

    private:
        VkDevice m_device;
        VkDescriptorSetLayout m_layout;
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> m_bindings;

        friend class DescriptorWriter;
    };

    class BATLETH_API DescriptorPool {
    public:
        class BATLETH_API Builder {
        public:
            explicit Builder(VkDevice device) : m_device(device) {
            }

            Builder &add_pool_size(VkDescriptorType descriptor_type, uint32_t count);

            Builder &set_pool_flags(VkDescriptorPoolCreateFlags flags);

            Builder &set_max_sets(uint32_t count);

            std::unique_ptr<DescriptorPool> build() const;

        private:
            VkDevice m_device;
            std::vector<VkDescriptorPoolSize> m_pool_sizes{};
            uint32_t m_max_sets = 1000;
            VkDescriptorPoolCreateFlags m_pool_flags = 0;
        };

        DescriptorPool(
            VkDevice device,
            uint32_t max_sets,
            VkDescriptorPoolCreateFlags pool_flags,
            const std::vector<VkDescriptorPoolSize> &pool_sizes);

        ~DescriptorPool();

        DescriptorPool(const DescriptorPool &) = delete;

        DescriptorPool &operator=(const DescriptorPool &) = delete;

        auto allocate_descriptor_set(VkDescriptorSetLayout layout, VkDescriptorSet &descriptor) const -> bool;

        auto free_descriptors(std::vector<VkDescriptorSet> &descriptors) const -> void;

        auto reset_pool() -> void;

        [[nodiscard]] auto get_pool() const -> VkDescriptorPool { return m_pool; }

    private:
        VkDevice m_device;
        VkDescriptorPool m_pool;

        friend class DescriptorWriter;
    };

    class BATLETH_API DescriptorWriter {
    public:
        DescriptorWriter(DescriptorSetLayout &set_layout, DescriptorPool &pool);

        DescriptorWriter &write_buffer(uint32_t binding, VkDescriptorBufferInfo *buffer_info);

        DescriptorWriter &write_image(uint32_t binding, VkDescriptorImageInfo *image_info);

        auto build(VkDescriptorSet &set) -> bool;

        auto overwrite(VkDescriptorSet &set) -> void;

    private:
        DescriptorSetLayout &m_set_layout;
        DescriptorPool &m_pool;
        std::vector<VkWriteDescriptorSet> m_writes;
    };
} // namespace batleth