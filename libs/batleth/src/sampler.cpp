#include "batleth/sampler.hpp"
#include "federation/log.hpp"
#include <stdexcept>

namespace batleth {

Sampler::Sampler(const Config& config)
    : m_device(config.device) {

    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = config.mag_filter;
    sampler_info.minFilter = config.min_filter;
    sampler_info.mipmapMode = config.mipmap_mode;
    sampler_info.addressModeU = config.address_mode_u;
    sampler_info.addressModeV = config.address_mode_v;
    sampler_info.addressModeW = config.address_mode_w;
    sampler_info.mipLodBias = config.mip_lod_bias;
    sampler_info.anisotropyEnable = config.anisotropy_enable ? VK_TRUE : VK_FALSE;
    sampler_info.maxAnisotropy = config.max_anisotropy;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.minLod = config.min_lod;
    sampler_info.maxLod = config.max_lod;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;

    if (::vkCreateSampler(m_device, &sampler_info, nullptr, &m_sampler) != VK_SUCCESS) {
        FED_FATAL("Failed to create VkSampler");
        throw std::runtime_error("Failed to create VkSampler");
    }

    FED_TRACE("Created VkSampler (anisotropy: {}, max_lod: {})",
                         config.anisotropy_enable, config.max_lod);
}

Sampler::~Sampler() {
    if (m_sampler != VK_NULL_HANDLE) {
        ::vkDestroySampler(m_device, m_sampler, nullptr);
        FED_TRACE("Destroyed VkSampler");
    }
}

Sampler::Sampler(Sampler&& other) noexcept
    : m_device(other.m_device)
    , m_sampler(other.m_sampler) {

    other.m_device = VK_NULL_HANDLE;
    other.m_sampler = VK_NULL_HANDLE;
}

auto Sampler::operator=(Sampler&& other) noexcept -> Sampler& {
    if (this != &other) {
        // Clean up existing
        if (m_sampler != VK_NULL_HANDLE) {
            ::vkDestroySampler(m_device, m_sampler, nullptr);
        }

        // Move
        m_device = other.m_device;
        m_sampler = other.m_sampler;

        // Clear moved-from
        other.m_device = VK_NULL_HANDLE;
        other.m_sampler = VK_NULL_HANDLE;
    }
    return *this;
}

} // namespace batleth
