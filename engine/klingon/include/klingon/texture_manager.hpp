#pragma once

#include "batleth/texture.hpp"
#include "batleth/sampler.hpp"
#include "batleth/device.hpp"
#include "batleth/buffer.hpp"
#include "batleth/descriptors.hpp"
#include "material.hpp"
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

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
     * Manages bindless texture array and material buffer (SSBO)
     * Handles texture loading (stb_image, KTX2, DDS) and GPU material uploads
     */
    class KLINGON_API TextureManager {
    public:
        struct Config {
            batleth::Device& device;
            VmaAllocator allocator;
            uint32_t max_textures = 4096;
            uint32_t max_materials = 1024;
        };

        explicit TextureManager(const Config& config);
        ~TextureManager() = default;

        // Delete copy operations
        TextureManager(const TextureManager&) = delete;
        auto operator=(const TextureManager&) -> TextureManager& = delete;

        /**
         * Load texture from file (auto-detects format)
         * Returns texture index in bindless array
         * @param filepath Path to texture file
         * @param type Texture type (albedo/normal/pbr)
         * @param generate_mipmaps Auto-generate mipmaps (ignored for pre-compressed)
         * @return Texture index (handle for bindless access)
         */
        auto load_texture(
            const std::string& filepath,
            batleth::TextureType type,
            bool generate_mipmaps = true
        ) -> uint32_t;

        /**
         * Get default texture indices
         */
        [[nodiscard]] auto get_default_albedo_index() const -> uint32_t { return 0; }
        [[nodiscard]] auto get_default_normal_index() const -> uint32_t { return 1; }
        [[nodiscard]] auto get_default_pbr_index() const -> uint32_t { return 2; }

        /**
         * Material buffer management
         */
        auto upload_material(MaterialGPU& material) -> uint32_t;  // Returns material index
        auto update_material(uint32_t index, MaterialGPU& material) -> void;
        auto upload_materials(std::vector<MaterialGPU>& materials) -> uint32_t;  // Batch upload, returns starting index

        /**
         * Get descriptor set for bindless resources (Set 2)
         */
        [[nodiscard]] auto get_descriptor_set() const -> VkDescriptorSet { return m_descriptor_set; }

        /**
         * Get descriptor set layout for pipeline creation
         */
        [[nodiscard]] auto get_descriptor_layout() const -> VkDescriptorSetLayout;

        /**
         * Update descriptors (call after loading textures)
         */
        auto update_descriptors() -> void;

    private:
        auto load_stb_image(const std::string& filepath, batleth::TextureType type, bool gen_mips) -> uint32_t;
        auto load_ktx2(const std::string& filepath, batleth::TextureType type) -> uint32_t;
        auto load_dds(const std::string& filepath, batleth::TextureType type) -> uint32_t;

        auto create_default_textures() -> void;
        auto create_material_buffer() -> void;
        auto create_descriptor_set_layout() -> void;
        auto create_descriptor_pool() -> void;
        auto create_descriptor_set() -> void;

        batleth::Device& m_device;
        VmaAllocator m_allocator;

        // Bindless texture storage (separate arrays per type)
        std::vector<std::unique_ptr<batleth::Texture>> m_albedo_textures;
        std::vector<std::unique_ptr<batleth::Texture>> m_normal_textures;
        std::vector<std::unique_ptr<batleth::Texture>> m_pbr_textures;

        // Texture path -> index cache (prevent duplicate loads)
        std::unordered_map<std::string, uint32_t> m_albedo_cache;
        std::unordered_map<std::string, uint32_t> m_normal_cache;
        std::unordered_map<std::string, uint32_t> m_pbr_cache;

        // Material buffer (SSBO)
        std::unique_ptr<batleth::Buffer> m_material_buffer;  // GPU buffer
        std::vector<MaterialGPU> m_material_data;            // CPU-side copy
        uint32_t m_material_count = 0;
        uint32_t m_max_materials;

        // Shared sampler
        std::unique_ptr<batleth::Sampler> m_default_sampler;

        // Descriptor resources
        std::unique_ptr<batleth::DescriptorSetLayout> m_descriptor_layout;
        std::unique_ptr<batleth::DescriptorPool> m_descriptor_pool;
        VkDescriptorSet m_descriptor_set = VK_NULL_HANDLE;

        uint32_t m_max_textures;
        bool m_descriptors_dirty = true;
    };
} // namespace klingon
