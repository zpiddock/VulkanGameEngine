#pragma once

#include <vulkan/vulkan.h>

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
     * RAII wrapper for VkSampler
     */
    class BATLETH_API Sampler {
    public:
        struct Config {
            VkDevice device = VK_NULL_HANDLE;
            VkFilter mag_filter = VK_FILTER_LINEAR;
            VkFilter min_filter = VK_FILTER_LINEAR;
            VkSamplerMipmapMode mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            VkSamplerAddressMode address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            VkSamplerAddressMode address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            VkSamplerAddressMode address_mode_w = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            float mip_lod_bias = 0.0f;
            bool anisotropy_enable = true;
            float max_anisotropy = 16.0f;
            float min_lod = 0.0f;
            float max_lod = VK_LOD_CLAMP_NONE;
        };

        explicit Sampler(const Config& config);
        ~Sampler();

        // Delete copy operations
        Sampler(const Sampler&) = delete;
        auto operator=(const Sampler&) -> Sampler& = delete;

        // Move operations
        Sampler(Sampler&& other) noexcept;
        auto operator=(Sampler&& other) noexcept -> Sampler&;

        [[nodiscard]] auto get_handle() const -> VkSampler { return m_sampler; }

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkSampler m_sampler = VK_NULL_HANDLE;
    };
} // namespace batleth
