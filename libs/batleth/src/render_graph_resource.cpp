#include "batleth/render_graph_resource.hpp"

namespace batleth {

auto usage_to_stage_mask(ResourceUsage usage) -> VkPipelineStageFlags2 {
    switch (usage) {
        case ResourceUsage::SampledImage:
            return VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                   VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                   VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

        case ResourceUsage::StorageImageRead:
        case ResourceUsage::StorageImageWrite:
        case ResourceUsage::StorageImageReadWrite:
            return VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                   VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

        case ResourceUsage::UniformBuffer:
            return VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                   VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                   VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

        case ResourceUsage::StorageBufferRead:
        case ResourceUsage::StorageBufferWrite:
        case ResourceUsage::StorageBufferReadWrite:
            return VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                   VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                   VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

        case ResourceUsage::VertexBuffer:
            return VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;

        case ResourceUsage::IndexBuffer:
            return VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;

        case ResourceUsage::IndirectBuffer:
            return VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;

        case ResourceUsage::TransferSource:
            return VK_PIPELINE_STAGE_2_TRANSFER_BIT;

        case ResourceUsage::TransferDestination:
            return VK_PIPELINE_STAGE_2_TRANSFER_BIT;

        case ResourceUsage::ColorAttachment:
            return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

        case ResourceUsage::DepthStencilRead:
            return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                   VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;

        case ResourceUsage::DepthStencilWrite:
        case ResourceUsage::DepthStencilReadWrite:
            return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                   VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;

        case ResourceUsage::InputAttachment:
            return VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    }

    return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
}

auto usage_to_access_mask(ResourceUsage usage) -> VkAccessFlags2 {
    switch (usage) {
        case ResourceUsage::SampledImage:
            return VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;

        case ResourceUsage::StorageImageRead:
            return VK_ACCESS_2_SHADER_STORAGE_READ_BIT;

        case ResourceUsage::StorageImageWrite:
            return VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

        case ResourceUsage::StorageImageReadWrite:
            return VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                   VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

        case ResourceUsage::UniformBuffer:
            return VK_ACCESS_2_UNIFORM_READ_BIT;

        case ResourceUsage::StorageBufferRead:
            return VK_ACCESS_2_SHADER_STORAGE_READ_BIT;

        case ResourceUsage::StorageBufferWrite:
            return VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

        case ResourceUsage::StorageBufferReadWrite:
            return VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                   VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

        case ResourceUsage::VertexBuffer:
            return VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;

        case ResourceUsage::IndexBuffer:
            return VK_ACCESS_2_INDEX_READ_BIT;

        case ResourceUsage::IndirectBuffer:
            return VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;

        case ResourceUsage::TransferSource:
            return VK_ACCESS_2_TRANSFER_READ_BIT;

        case ResourceUsage::TransferDestination:
            return VK_ACCESS_2_TRANSFER_WRITE_BIT;

        case ResourceUsage::ColorAttachment:
            return VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
                   VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

        case ResourceUsage::DepthStencilRead:
            return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

        case ResourceUsage::DepthStencilWrite:
            return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        case ResourceUsage::DepthStencilReadWrite:
            return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                   VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        case ResourceUsage::InputAttachment:
            return VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT;
    }

    return VK_ACCESS_2_NONE;
}

auto usage_to_image_layout(ResourceUsage usage) -> VkImageLayout {
    switch (usage) {
        case ResourceUsage::SampledImage:
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        case ResourceUsage::StorageImageRead:
        case ResourceUsage::StorageImageWrite:
        case ResourceUsage::StorageImageReadWrite:
            return VK_IMAGE_LAYOUT_GENERAL;

        case ResourceUsage::TransferSource:
            return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        case ResourceUsage::TransferDestination:
            return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        case ResourceUsage::ColorAttachment:
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        case ResourceUsage::DepthStencilRead:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

        case ResourceUsage::DepthStencilWrite:
        case ResourceUsage::DepthStencilReadWrite:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        case ResourceUsage::InputAttachment:
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // Buffer usages don't have image layouts
        case ResourceUsage::UniformBuffer:
        case ResourceUsage::StorageBufferRead:
        case ResourceUsage::StorageBufferWrite:
        case ResourceUsage::StorageBufferReadWrite:
        case ResourceUsage::VertexBuffer:
        case ResourceUsage::IndexBuffer:
        case ResourceUsage::IndirectBuffer:
            return VK_IMAGE_LAYOUT_UNDEFINED;
    }

    return VK_IMAGE_LAYOUT_UNDEFINED;
}

auto is_write_usage(ResourceUsage usage) -> bool {
    switch (usage) {
        case ResourceUsage::StorageImageWrite:
        case ResourceUsage::StorageImageReadWrite:
        case ResourceUsage::StorageBufferWrite:
        case ResourceUsage::StorageBufferReadWrite:
        case ResourceUsage::TransferDestination:
        case ResourceUsage::ColorAttachment:
        case ResourceUsage::DepthStencilWrite:
        case ResourceUsage::DepthStencilReadWrite:
            return true;

        default:
            return false;
    }
}

auto is_read_usage(ResourceUsage usage) -> bool {
    switch (usage) {
        case ResourceUsage::SampledImage:
        case ResourceUsage::StorageImageRead:
        case ResourceUsage::StorageImageReadWrite:
        case ResourceUsage::UniformBuffer:
        case ResourceUsage::StorageBufferRead:
        case ResourceUsage::StorageBufferReadWrite:
        case ResourceUsage::VertexBuffer:
        case ResourceUsage::IndexBuffer:
        case ResourceUsage::IndirectBuffer:
        case ResourceUsage::TransferSource:
        case ResourceUsage::DepthStencilRead:
        case ResourceUsage::DepthStencilReadWrite:
        case ResourceUsage::InputAttachment:
        case ResourceUsage::ColorAttachment:  // Color attachment reads for blending
            return true;

        default:
            return false;
    }
}

auto usage_to_state(ResourceUsage usage, std::uint32_t queue_family) -> ResourceState {
    return ResourceState{
        .stage_mask = usage_to_stage_mask(usage),
        .access_mask = usage_to_access_mask(usage),
        .layout = usage_to_image_layout(usage),
        .queue_family = queue_family
    };
}

auto format_to_aspect_mask(VkFormat format) -> VkImageAspectFlags {
    switch (format) {
        // Depth-only formats
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
            return VK_IMAGE_ASPECT_DEPTH_BIT;

        // Depth-stencil formats
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

        // Stencil-only format
        case VK_FORMAT_S8_UINT:
            return VK_IMAGE_ASPECT_STENCIL_BIT;

        // All other formats are color
        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

} // namespace batleth
