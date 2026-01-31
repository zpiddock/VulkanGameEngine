#pragma once

#include <glm/glm.hpp>
#include <string>
#include <cstdint>

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
     * GPU-side material data (uploaded to SSBO)
     * MUST match shader MaterialData layout exactly!
     * Uses std430 packing (tighter than std140)
     */
    struct MaterialGPU {
        alignas(16) glm::vec4 base_color_factor{1.0f, 1.0f, 1.0f, 1.0f};
        alignas(4) float metallic_factor = 1.0f;
        alignas(4) float roughness_factor = 1.0f;
        alignas(4) float normal_scale = 1.0f;
        alignas(4) uint32_t albedo_texture_index = 0;  // Bindless texture indices
        alignas(4) uint32_t normal_texture_index = 1;
        alignas(4) uint32_t pbr_texture_index = 2;
        alignas(4) uint32_t opacity_texture_index = 3;
        alignas(4) uint32_t material_flags = 0;  // bit 0: has_albedo, bit 1: has_normal, bit 2: has_pbr, bit 3: has_opacity
        alignas(4) uint32_t _padding[3]{0, 0, 0};  // Pad to 64 bytes (std430 array alignment)
    };
    static_assert(sizeof(MaterialGPU) == 64, "MaterialGPU must be 64 bytes for std430 array alignment");

    /**
     * CPU-side material with texture paths and GPU data
     */
    struct KLINGON_API Material {
        // GPU data (uploaded to SSBO)
        MaterialGPU gpu_data;

        // CPU-only: Texture paths (for loading/reloading)
        std::string albedo_texture_path;
        std::string normal_texture_path;
        std::string pbr_texture_path;
        std::string opacity_texture_path;

        Material() {
            gpu_data.material_flags = 0; // No textures by default
        }

        // Helper to set texture flags
        auto set_has_albedo(bool has) -> void {
            if (has) gpu_data.material_flags |= 1u;
            else gpu_data.material_flags &= ~1u;
        }

        auto set_has_normal(bool has) -> void {
            if (has) gpu_data.material_flags |= 2u;
            else gpu_data.material_flags &= ~2u;
        }

        auto set_has_pbr(bool has) -> void {
            if (has) gpu_data.material_flags |= 4u;
            else gpu_data.material_flags &= ~4u;
        }

        auto set_has_opacity(bool has) -> void {
            if (has) gpu_data.material_flags |= 8u;
            else gpu_data.material_flags &= ~8u;
        }

        // NO serialize() - Material is runtime-only, part of ModelData
    };
} // namespace klingon
