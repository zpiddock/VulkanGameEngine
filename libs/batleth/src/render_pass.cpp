#include "batleth/render_pass.hpp"
#include <stdexcept>
#include <array>

#include "federation/log.hpp"

namespace batleth {

RenderPass::RenderPass(const Config& config) : m_device(config.device) {
    VkAttachmentDescription color_attachment{};
    color_attachment.format = config.color_format;
    color_attachment.samples = config.samples;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    if (::vkCreateRenderPass(m_device, &render_pass_info, nullptr, &m_render_pass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }
}

RenderPass::~RenderPass() {
    FED_DEBUG("Destroying renderpass");
    if (m_render_pass != VK_NULL_HANDLE) {
        ::vkDestroyRenderPass(m_device, m_render_pass, nullptr);
    }
    FED_DEBUG("Renderpass destroyed successfully");
}

RenderPass::RenderPass(RenderPass&& other) noexcept
    : m_device(other.m_device)
    , m_render_pass(other.m_render_pass) {
    other.m_device = VK_NULL_HANDLE;
    other.m_render_pass = VK_NULL_HANDLE;
}

auto RenderPass::operator=(RenderPass&& other) noexcept -> RenderPass& {
    if (this != &other) {
        if (m_render_pass != VK_NULL_HANDLE) {
            ::vkDestroyRenderPass(m_device, m_render_pass, nullptr);
        }

        m_device = other.m_device;
        m_render_pass = other.m_render_pass;

        other.m_device = VK_NULL_HANDLE;
        other.m_render_pass = VK_NULL_HANDLE;
    }
    return *this;
}

} // namespace batleth
