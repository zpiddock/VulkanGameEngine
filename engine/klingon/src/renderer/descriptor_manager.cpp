#include "klingon/renderer/descriptor_manager.hpp"
#include <federation/log.hpp>

namespace klingon {

    DescriptorManager::DescriptorManager(const DescriptorManagerConfig& config)
        : m_device(config.device)
        , m_config(config)
    {
        // Initialize dirty flags
        m_global_dirty.fill(true);
        m_per_pass_dirty.fill(true);

        FED_DEBUG("DescriptorManager created");
    }

    void DescriptorManager::create_pools() {
        // Create global pool (primarily UBOs)
        m_global_pool = batleth::DescriptorPool::Builder(m_device.get_logical_device())
            .set_max_sets(m_config.max_global_sets)
            .add_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_config.max_uniform_buffers)
            .add_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8)  // For any samplers in global
            .build();

        // Create per-pass pool (storage buffers, samplers for Forward+)
        m_per_pass_pool = batleth::DescriptorPool::Builder(m_device.get_logical_device())
            .set_max_sets(m_config.max_per_pass_sets)
            .add_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_config.max_sampled_images)
            .add_pool_size(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_config.max_storage_buffers)
            .add_pool_size(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_config.max_storage_images)
            .build();

        FED_DEBUG("DescriptorManager pools created: global={} sets, per_pass={} sets",
                  m_config.max_global_sets, m_config.max_per_pass_sets);
    }

    void DescriptorManager::allocate_per_frame_sets() {
        // Allocate global sets if layout is registered
        if (m_global_layout && !m_global_sets_allocated) {
            for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
                if (!m_global_pool->allocate_descriptor_set(
                        m_global_layout->get_layout(), m_global_sets[i])) {
                    FED_ERROR("Failed to allocate global descriptor set for frame {}", i);
                }
            }
            m_global_sets_allocated = true;
            FED_DEBUG("Allocated {} global descriptor sets", MAX_FRAMES_IN_FLIGHT);
        }

        // Allocate per-pass sets if layout is registered
        if (m_per_pass_layout && !m_per_pass_sets_allocated) {
            for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
                if (!m_per_pass_pool->allocate_descriptor_set(
                        m_per_pass_layout->get_layout(), m_per_pass_sets[i])) {
                    FED_ERROR("Failed to allocate per-pass descriptor set for frame {}", i);
                }
            }
            m_per_pass_sets_allocated = true;
            FED_DEBUG("Allocated {} per-pass descriptor sets", MAX_FRAMES_IN_FLIGHT);
        }
    }

    void DescriptorManager::register_global_layout(std::unique_ptr<batleth::DescriptorSetLayout> layout) {
        m_global_layout = std::move(layout);

        // Create pools if not yet created
        if (!m_global_pool) {
            create_pools();
        }

        // Allocate sets now that we have the layout
        allocate_per_frame_sets();

        FED_DEBUG("Registered global descriptor set layout");
    }

    void DescriptorManager::register_per_pass_layout(std::unique_ptr<batleth::DescriptorSetLayout> layout) {
        m_per_pass_layout = std::move(layout);

        // Create pools if not yet created
        if (!m_per_pass_pool) {
            create_pools();
        }

        // Allocate sets now that we have the layout
        allocate_per_frame_sets();

        FED_DEBUG("Registered per-pass descriptor set layout");
    }

    void DescriptorManager::set_bindless_layout(VkDescriptorSetLayout layout) {
        m_bindless_layout = layout;
        FED_DEBUG("Set bindless descriptor set layout (external)");
    }

    auto DescriptorManager::get_global_layout() const -> VkDescriptorSetLayout {
        return m_global_layout ? m_global_layout->get_layout() : VK_NULL_HANDLE;
    }

    auto DescriptorManager::get_per_pass_layout() const -> VkDescriptorSetLayout {
        return m_per_pass_layout ? m_per_pass_layout->get_layout() : VK_NULL_HANDLE;
    }

    auto DescriptorManager::get_bindless_layout() const -> VkDescriptorSetLayout {
        return m_bindless_layout;
    }

    auto DescriptorManager::get_all_layouts() const -> std::array<VkDescriptorSetLayout, 3> {
        return {
            get_global_layout(),
            get_per_pass_layout(),
            get_bindless_layout()
        };
    }

    auto DescriptorManager::get_global_set(uint32_t frame_index) const -> VkDescriptorSet {
        if (frame_index >= MAX_FRAMES_IN_FLIGHT) {
            FED_ERROR("Invalid frame index {} (max {})", frame_index, MAX_FRAMES_IN_FLIGHT - 1);
            return VK_NULL_HANDLE;
        }
        return m_global_sets[frame_index];
    }

    auto DescriptorManager::get_per_pass_set(uint32_t frame_index) const -> VkDescriptorSet {
        if (frame_index >= MAX_FRAMES_IN_FLIGHT) {
            FED_ERROR("Invalid frame index {} (max {})", frame_index, MAX_FRAMES_IN_FLIGHT - 1);
            return VK_NULL_HANDLE;
        }
        return m_per_pass_sets[frame_index];
    }

    auto DescriptorManager::allocate_per_pass_set() -> std::optional<VkDescriptorSet> {
        if (!m_per_pass_layout || !m_per_pass_pool) {
            FED_ERROR("Cannot allocate per-pass set: layout or pool not initialized");
            return std::nullopt;
        }

        VkDescriptorSet set = VK_NULL_HANDLE;
        if (m_per_pass_pool->allocate_descriptor_set(m_per_pass_layout->get_layout(), set)) {
            return set;
        }

        FED_WARN("Per-pass descriptor pool exhausted");
        return std::nullopt;
    }

    void DescriptorManager::update_global_set(uint32_t frame_index, VkDescriptorBufferInfo* buffer_info) {
        if (frame_index >= MAX_FRAMES_IN_FLIGHT) {
            FED_ERROR("Invalid frame index {}", frame_index);
            return;
        }

        if (!m_global_layout || !m_global_pool) {
            FED_ERROR("Cannot update global set: not initialized");
            return;
        }

        // Always update for now - dirty tracking can be refined later
        batleth::DescriptorWriter(*m_global_layout, *m_global_pool)
            .write_buffer(0, buffer_info)
            .overwrite(m_global_sets[frame_index]);

        m_global_dirty[frame_index] = false;
    }

    void DescriptorManager::update_per_pass_set(
        uint32_t frame_index,
        VkDescriptorImageInfo* depth_image_info,
        VkDescriptorBufferInfo* light_grid_info,
        VkDescriptorBufferInfo* light_count_info
    ) {
        if (frame_index >= MAX_FRAMES_IN_FLIGHT) {
            FED_ERROR("Invalid frame index {}", frame_index);
            return;
        }

        if (!m_per_pass_layout || !m_per_pass_pool) {
            FED_ERROR("Cannot update per-pass set: not initialized");
            return;
        }

        // Update descriptor set with Forward+ resources
        batleth::DescriptorWriter(*m_per_pass_layout, *m_per_pass_pool)
            .write_image(0, depth_image_info)     // Binding 0: Depth texture
            .write_buffer(1, light_grid_info)     // Binding 1: Light grid SSBO
            .write_buffer(2, light_count_info)    // Binding 2: Light count SSBO
            .overwrite(m_per_pass_sets[frame_index]);

        m_per_pass_dirty[frame_index] = false;
    }

    void DescriptorManager::mark_global_dirty(uint32_t frame_index) {
        if (frame_index < MAX_FRAMES_IN_FLIGHT) {
            m_global_dirty[frame_index] = true;
        }
    }

    void DescriptorManager::mark_per_pass_dirty(uint32_t frame_index) {
        if (frame_index < MAX_FRAMES_IN_FLIGHT) {
            m_per_pass_dirty[frame_index] = true;
        }
    }

    void DescriptorManager::mark_all_dirty() {
        m_global_dirty.fill(true);
        m_per_pass_dirty.fill(true);
    }

    void DescriptorManager::begin_frame([[maybe_unused]] uint32_t frame_index) {
        // Currently no per-frame reset needed since we use fixed sets
        // This hook exists for future use (e.g., if we add dynamic allocation)
    }

} // namespace klingon
