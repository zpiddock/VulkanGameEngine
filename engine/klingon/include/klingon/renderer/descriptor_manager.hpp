#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <array>
#include <vector>
#include <optional>
#include <cstdint>

#include "batleth/device.hpp"
#include "batleth/descriptors.hpp"
#include "batleth/buffer.hpp"
#include "frame_context.hpp"

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
     * Fixed descriptor set indices.
     * These MUST be consistent across all shaders and pipelines.
     * NEVER skip sets, NEVER reorder.
     */
    enum class SetIndex : uint32_t {
        Global = 0,     // Camera matrices, lights, time, ambient
        PerPass = 1,    // Pass-specific data (Forward+ grid, shadow maps, etc.)
        Bindless = 2    // Texture arrays, material SSBO
    };

    /**
     * Configuration for DescriptorManager
     */
    struct DescriptorManagerConfig {
        batleth::Device& device;
        uint32_t max_global_sets = MAX_FRAMES_IN_FLIGHT * 2;
        uint32_t max_per_pass_sets = MAX_FRAMES_IN_FLIGHT * 4;
        uint32_t max_uniform_buffers = 32;
        uint32_t max_storage_buffers = 32;
        uint32_t max_sampled_images = 64;
        uint32_t max_storage_images = 16;
    };

    /**
     * Centralized descriptor set management with fixed layouts.
     *
     * Design principles:
     * - Set 0 = Global (camera, lights) - updated once per frame
     * - Set 1 = Per-pass (Forward+, shadows) - updated per render pass
     * - Set 2 = Bindless (textures, materials) - updated on resource load
     *
     * Features:
     * - Pre-allocated descriptor pools
     * - Per-frame descriptor sets for Global/PerPass
     * - Dirty tracking to avoid redundant updates
     * - Type-safe set allocation and updates
     */
    class KLINGON_API DescriptorManager {
    public:
        explicit DescriptorManager(const DescriptorManagerConfig& config);
        ~DescriptorManager() = default;

        // Non-copyable, non-movable
        DescriptorManager(const DescriptorManager&) = delete;
        DescriptorManager& operator=(const DescriptorManager&) = delete;

        /**
         * Initialize layouts for a specific set.
         * Must be called before allocating or using that set.
         */
        void register_global_layout(std::unique_ptr<batleth::DescriptorSetLayout> layout);
        void register_per_pass_layout(std::unique_ptr<batleth::DescriptorSetLayout> layout);
        void set_bindless_layout(VkDescriptorSetLayout layout); // External ownership (from TextureManager)

        /**
         * Get layout handles for pipeline creation.
         */
        [[nodiscard]] auto get_global_layout() const -> VkDescriptorSetLayout;
        [[nodiscard]] auto get_per_pass_layout() const -> VkDescriptorSetLayout;
        [[nodiscard]] auto get_bindless_layout() const -> VkDescriptorSetLayout;

        /**
         * Get all layouts as an array for pipeline layout creation.
         * Returns layouts in order: [Global, PerPass, Bindless]
         * Any unregistered layout returns VK_NULL_HANDLE in that position.
         */
        [[nodiscard]] auto get_all_layouts() const -> std::array<VkDescriptorSetLayout, 3>;

        /**
         * Get descriptor sets for a specific frame.
         * These are pre-allocated and reused each frame.
         */
        [[nodiscard]] auto get_global_set(uint32_t frame_index) const -> VkDescriptorSet;
        [[nodiscard]] auto get_per_pass_set(uint32_t frame_index) const -> VkDescriptorSet;

        /**
         * Allocate a new per-pass descriptor set (for passes needing unique sets).
         * Returns nullopt if pool is exhausted.
         */
        [[nodiscard]] auto allocate_per_pass_set() -> std::optional<VkDescriptorSet>;

        /**
         * Update global descriptor set with UBO.
         * Only performs the update if the set is marked dirty.
         */
        void update_global_set(uint32_t frame_index, VkDescriptorBufferInfo* buffer_info);

        /**
         * Update per-pass descriptor set with Forward+ resources.
         */
        void update_per_pass_set(
            uint32_t frame_index,
            VkDescriptorImageInfo* depth_image_info,
            VkDescriptorBufferInfo* light_grid_info,
            VkDescriptorBufferInfo* light_count_info
        );

        /**
         * Mark a set as dirty, forcing update on next use.
         */
        void mark_global_dirty(uint32_t frame_index);
        void mark_per_pass_dirty(uint32_t frame_index);
        void mark_all_dirty();

        /**
         * Reset per-frame allocations (call at start of frame).
         * This doesn't deallocate, just resets the "extra sets" counter.
         */
        void begin_frame(uint32_t frame_index);

        /**
         * Check if layouts are registered.
         */
        [[nodiscard]] auto has_global_layout() const -> bool { return m_global_layout != nullptr; }
        [[nodiscard]] auto has_per_pass_layout() const -> bool { return m_per_pass_layout != nullptr; }
        [[nodiscard]] auto has_bindless_layout() const -> bool { return m_bindless_layout != VK_NULL_HANDLE; }

    private:
        void create_pools();
        void allocate_per_frame_sets();

        batleth::Device& m_device;
        DescriptorManagerConfig m_config;

        // Layouts (owned for Global/PerPass, external for Bindless)
        std::unique_ptr<batleth::DescriptorSetLayout> m_global_layout;
        std::unique_ptr<batleth::DescriptorSetLayout> m_per_pass_layout;
        VkDescriptorSetLayout m_bindless_layout = VK_NULL_HANDLE;  // External ownership

        // Descriptor pools
        std::unique_ptr<batleth::DescriptorPool> m_global_pool;
        std::unique_ptr<batleth::DescriptorPool> m_per_pass_pool;

        // Pre-allocated per-frame descriptor sets
        std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> m_global_sets{};
        std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> m_per_pass_sets{};

        // Dirty flags to track when updates are needed
        std::array<bool, MAX_FRAMES_IN_FLIGHT> m_global_dirty{};
        std::array<bool, MAX_FRAMES_IN_FLIGHT> m_per_pass_dirty{};

        // Track if sets have been allocated
        bool m_global_sets_allocated = false;
        bool m_per_pass_sets_allocated = false;
    };

} // namespace klingon
