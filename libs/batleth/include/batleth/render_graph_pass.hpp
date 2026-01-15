#pragma once

#include "render_graph_resource.hpp"
#include <vulkan/vulkan.h>
#include <functional>
#include <vector>
#include <string>
#include <cstdint>

#ifdef _WIN32
    #ifdef BATLETH_EXPORTS
        #define BATLETH_API __declspec(dllexport)
    #else
        #define BATLETH_API __declspec(dllimport)
    #endif
#else
    #define BATLETH_API
#endif

namespace klingon {
class CompiledRenderGraph;
}

namespace batleth {

// Pass type enumeration
enum class PassType {
    Graphics,
    Compute,
    Transfer
};

// Color attachment configuration
struct BATLETH_API ColorAttachmentConfig {
    ResourceHandle handle = INVALID_RESOURCE;
    VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkAttachmentStoreOp store_op = VK_ATTACHMENT_STORE_OP_STORE;
    VkClearColorValue clear_value = {{0.0f, 0.0f, 0.0f, 1.0f}};
};

// Depth attachment configuration
struct BATLETH_API DepthAttachmentConfig {
    ResourceHandle handle = INVALID_RESOURCE;
    VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkAttachmentStoreOp store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    VkAttachmentLoadOp stencil_load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp stencil_store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    VkClearDepthStencilValue clear_value = {1.0f, 0};
};

// Resource access declaration within a pass
struct BATLETH_API ResourceAccess {
    ResourceHandle handle = INVALID_RESOURCE;
    ResourceUsage usage = ResourceUsage::SampledImage;
    VkPipelineStageFlags2 stage_override = 0;  // 0 = use default from usage
};

// Pass configuration
struct BATLETH_API PassConfig {
    std::string name;
    PassType type = PassType::Graphics;
    QueueType queue = QueueType::Graphics;

    // Resource access declarations
    std::vector<ResourceAccess> reads;
    std::vector<ResourceAccess> writes;

    // Graphics pass attachments
    std::vector<ColorAttachmentConfig> color_attachments;
    DepthAttachmentConfig depth_attachment;
    bool has_depth_attachment = false;

    // Rendering configuration
    VkRect2D render_area = {{0, 0}, {0, 0}};  // (0,0) = use full extent

    // Viewport/scissor (empty = dynamic)
    VkViewport viewport = {};
    VkRect2D scissor = {};
};

// Execution context passed to pass callbacks
struct BATLETH_API PassExecutionContext {
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    std::uint32_t frame_index = 0;
    float delta_time = 0.0f;
    VkExtent2D render_extent = {0, 0};

    // Pass information
    const PassConfig* config = nullptr;

    // Internal pointer to compiled graph for resource lookup
    const klingon::CompiledRenderGraph* graph = nullptr;

    // Resource access methods
    [[nodiscard]] auto get_image(ResourceHandle handle) const -> VkImage;
    [[nodiscard]] auto get_image_view(ResourceHandle handle) const -> VkImageView;
    [[nodiscard]] auto get_buffer(ResourceHandle handle) const -> VkBuffer;
    [[nodiscard]] auto get_image_format(ResourceHandle handle) const -> VkFormat;
    [[nodiscard]] auto get_image_extent(ResourceHandle handle) const -> VkExtent3D;
};

// Pass execution callback type
using PassExecuteCallback = std::function<void(const PassExecutionContext&)>;

// Complete pass definition
struct BATLETH_API PassDefinition {
    PassConfig config;
    PassExecuteCallback execute;

    // Computed during compilation
    std::uint32_t index = ~0u;  // Original index in declaration order
    std::uint32_t topological_order = ~0u;
    std::vector<std::uint32_t> dependencies;  // Indices of passes this depends on
    std::vector<std::uint32_t> dependents;    // Indices of passes that depend on this
};

// Barrier information for pass transitions
struct BATLETH_API PassBarrier {
    ResourceHandle resource = INVALID_RESOURCE;
    ResourceState before;
    ResourceState after;
    bool is_release = false;  // True for queue family release barrier
    bool is_acquire = false;  // True for queue family acquire barrier
};

// Compute stage and access masks for a resource access
BATLETH_API auto compute_resource_state(
    const ResourceAccess& access,
    std::uint32_t queue_family = VK_QUEUE_FAMILY_IGNORED
) -> ResourceState;

// Check if two resource accesses have a dependency (RAW, WAW, WAR)
BATLETH_API auto has_dependency(
    const ResourceAccess& first,
    const ResourceAccess& second
) -> bool;

} // namespace batleth
