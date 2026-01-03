#include "batleth/framebuffer.hpp"
#include <stdexcept>

namespace batleth {

Framebuffer::Framebuffer(const Config& config) : m_device(config.device) {
    m_framebuffers.resize(config.image_views.size());

    for (std::size_t i = 0; i < config.image_views.size(); ++i) {
        VkImageView attachments[] = {config.image_views[i]};

        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = config.render_pass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = config.width;
        framebuffer_info.height = config.height;
        framebuffer_info.layers = 1;

        if (::vkCreateFramebuffer(m_device, &framebuffer_info, nullptr, &m_framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
}

Framebuffer::~Framebuffer() {
    for (auto framebuffer : m_framebuffers) {
        ::vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }
}

Framebuffer::Framebuffer(Framebuffer&& other) noexcept
    : m_device(other.m_device)
    , m_framebuffers(std::move(other.m_framebuffers)) {
    other.m_device = VK_NULL_HANDLE;
}

auto Framebuffer::operator=(Framebuffer&& other) noexcept -> Framebuffer& {
    if (this != &other) {
        for (auto framebuffer : m_framebuffers) {
            ::vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        }

        m_device = other.m_device;
        m_framebuffers = std::move(other.m_framebuffers);

        other.m_device = VK_NULL_HANDLE;
    }
    return *this;
}

} // namespace batleth
