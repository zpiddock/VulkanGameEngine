#pragma once

#include "batleth/render_graph_resource.hpp"
#include "batleth/render_graph_pass.hpp"
#include "batleth/barrier_batcher.hpp"
#include "batleth/transient_allocator.hpp"

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#ifdef _WIN32
    #ifdef KLINGON_EXPORTS
        #define KLINGON_API __declspec(dllexport)
    #else
        #define KLINGON_API __declspec(dllimport)
    #endif
#else
    #define KLINGON_API
#endif

namespace batleth {
class Device;
}

namespace klingon {

class Renderer;

// Forward declarations
class RenderGraphBuilder;
class CompiledRenderGraph;

/**
 * External resource descriptor for swapchain, persistent textures, etc.
 * These are resources not managed by the render graph but used by it.
 */
struct KLINGON_API ExternalResource {
    batleth::ResourceHandle handle = batleth::INVALID_RESOURCE;
    batleth::ResourceType type = batleth::ResourceType::Image;

    // For images
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent = {0, 0};

    // For buffers
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceSize size = 0;

    // Initial/final states for barrier generation
    batleth::ResourceState initial_state;
    batleth::ResourceState final_state;
};

/**
 * High-level render graph API.
 *
 * Usage:
 * 1. Create a RenderGraph
 * 2. Call begin_build() to get a builder
 * 3. Define resources and passes using the builder
 * 4. Call compile() to optimize and prepare for execution
 * 5. Call execute() each frame
 */
class KLINGON_API RenderGraph {
public:
    explicit RenderGraph(Renderer& renderer);
    ~RenderGraph();

    RenderGraph(const RenderGraph&) = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;

    /**
     * Begin building a new graph. Clears any existing graph.
     * @return Reference to the builder for chaining
     */
    auto begin_build() -> RenderGraphBuilder&;

    /**
     * Compile the graph for execution.
     * Performs topological sort, allocates resources, computes barriers.
     * @return true if compilation succeeded
     */
    auto compile() -> bool;

    /**
     * Execute the render graph.
     * @param cmd Command buffer to record into
     * @param frame_index Current frame index
     * @param delta_time Time since last frame
     */
    auto execute(VkCommandBuffer cmd, std::uint32_t frame_index, float delta_time = 0.0f) -> void;

    /**
     * Set the backbuffer (swapchain image) for this frame.
     * Must be called before execute() each frame.
     */
    auto set_backbuffer(
        VkImage image,
        VkImageView view,
        VkFormat format,
        VkExtent2D extent
    ) -> void;

    /**
     * Get the handle for the backbuffer resource.
     */
    [[nodiscard]] auto get_backbuffer_handle() const -> batleth::ResourceHandle;

    /**
     * Check if the graph has been compiled.
     */
    [[nodiscard]] auto is_compiled() const -> bool;

    /**
     * Get the number of passes in the compiled graph.
     */
    [[nodiscard]] auto get_pass_count() const -> std::size_t;

    /**
     * Mark the graph as needing recompilation.
     * Call this when swapchain is recreated.
     */
    auto invalidate() -> void;

    /**
     * Get the render extent (from backbuffer).
     */
    [[nodiscard]] auto get_render_extent() const -> VkExtent2D;

private:
    Renderer& m_renderer;
    std::unique_ptr<RenderGraphBuilder> m_builder;
    std::unique_ptr<CompiledRenderGraph> m_compiled;

    // External resources
    std::unordered_map<std::string, ExternalResource> m_external_resources;
    batleth::ResourceHandle m_backbuffer_handle = batleth::INVALID_RESOURCE;
    ExternalResource m_backbuffer;

    bool m_needs_recompile = false;
};

/**
 * Builder interface for declarative graph construction.
 * Uses fluent API for ease of use.
 */
class KLINGON_API RenderGraphBuilder {
public:
    explicit RenderGraphBuilder(batleth::Device& device);
    ~RenderGraphBuilder();

    RenderGraphBuilder(const RenderGraphBuilder&) = delete;
    RenderGraphBuilder& operator=(const RenderGraphBuilder&) = delete;

    /**
     * Create a transient image resource.
     */
    auto create_image(
        const std::string& name,
        const batleth::ImageResourceDesc& desc
    ) -> batleth::ResourceHandle;

    /**
     * Create a transient buffer resource.
     */
    auto create_buffer(
        const std::string& name,
        const batleth::BufferResourceDesc& desc
    ) -> batleth::ResourceHandle;

    /**
     * Import an external resource (e.g., swapchain, persistent texture).
     */
    auto import_external(
        const std::string& name,
        const ExternalResource& external
    ) -> batleth::ResourceHandle;

    /**
     * Add a graphics pass.
     */
    auto add_graphics_pass(
        const std::string& name,
        batleth::PassExecuteCallback execute
    ) -> RenderGraphBuilder&;

    /**
     * Add a compute pass.
     */
    auto add_compute_pass(
        const std::string& name,
        batleth::PassExecuteCallback execute
    ) -> RenderGraphBuilder&;

    /**
     * Add a transfer pass.
     */
    auto add_transfer_pass(
        const std::string& name,
        batleth::PassExecuteCallback execute
    ) -> RenderGraphBuilder&;

    // Pass configuration (fluent API - operates on current pass)

    /**
     * Declare a read dependency on a resource.
     */
    auto read(
        batleth::ResourceHandle handle,
        batleth::ResourceUsage usage
    ) -> RenderGraphBuilder&;

    /**
     * Declare a write dependency on a resource.
     */
    auto write(
        batleth::ResourceHandle handle,
        batleth::ResourceUsage usage
    ) -> RenderGraphBuilder&;

    /**
     * Set a color attachment for the current graphics pass.
     * @param index Attachment index (0-7)
     * @param handle Resource handle
     * @param load_op How to initialize the attachment
     * @param clear_value Clear color (if load_op is CLEAR)
     */
    auto set_color_attachment(
        std::uint32_t index,
        batleth::ResourceHandle handle,
        VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
        VkClearColorValue clear_value = {{0.0f, 0.0f, 0.0f, 1.0f}}
    ) -> RenderGraphBuilder&;

    /**
     * Set the depth attachment for the current graphics pass.
     */
    auto set_depth_attachment(
        batleth::ResourceHandle handle,
        VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
        VkClearDepthStencilValue clear_value = {1.0f, 0}
    ) -> RenderGraphBuilder&;

    /**
     * Set the queue for the current pass (for async compute).
     */
    auto set_queue(batleth::QueueType queue) -> RenderGraphBuilder&;

    /**
     * Clear the builder for reuse.
     */
    auto clear() -> void;

    // Internal access for compilation
    [[nodiscard]] auto get_resources() const -> const std::vector<batleth::ResourceDesc>&;
    [[nodiscard]] auto get_passes() -> std::vector<batleth::PassDefinition>&;
    [[nodiscard]] auto get_external_resources() const -> const std::unordered_map<batleth::ResourceHandle, ExternalResource>&;

private:
    batleth::Device& m_device;
    std::vector<batleth::ResourceDesc> m_resources;
    std::vector<batleth::PassDefinition> m_passes;
    std::unordered_map<batleth::ResourceHandle, ExternalResource> m_externals;
    batleth::PassDefinition* m_current_pass = nullptr;
    batleth::ResourceHandle m_next_handle = 0;
};

/**
 * Compiled render graph optimized for execution.
 * Contains pre-computed barriers and resource allocations.
 */
class KLINGON_API CompiledRenderGraph {
public:
    CompiledRenderGraph(
        batleth::Device& device,
        RenderGraphBuilder& builder,
        VkInstance instance,
        VkPhysicalDevice physical_device
    );
    ~CompiledRenderGraph();

    CompiledRenderGraph(const CompiledRenderGraph&) = delete;
    CompiledRenderGraph& operator=(const CompiledRenderGraph&) = delete;

    /**
     * Execute the compiled graph.
     */
    auto execute(
        VkCommandBuffer cmd,
        std::uint32_t frame_index,
        float delta_time,
        VkExtent2D extent
    ) -> void;

    /**
     * Update an external resource (e.g., swapchain image changed).
     */
    auto update_external(
        batleth::ResourceHandle handle,
        const ExternalResource& external
    ) -> void;

    /**
     * Get number of passes.
     */
    [[nodiscard]] auto get_pass_count() const -> std::size_t;

    // Resource access for pass callbacks
    [[nodiscard]] auto get_image(batleth::ResourceHandle handle) const -> VkImage;
    [[nodiscard]] auto get_image_view(batleth::ResourceHandle handle) const -> VkImageView;
    [[nodiscard]] auto get_buffer(batleth::ResourceHandle handle) const -> VkBuffer;
    [[nodiscard]] auto get_image_format(batleth::ResourceHandle handle) const -> VkFormat;
    [[nodiscard]] auto get_image_extent(batleth::ResourceHandle handle) const -> VkExtent3D;

private:
    auto topological_sort() -> void;
    auto compute_lifetimes() -> void;
    auto allocate_resources() -> void;
    auto compute_barriers() -> void;

    auto begin_graphics_pass(VkCommandBuffer cmd, const batleth::PassDefinition& pass, VkExtent2D extent) -> void;
    auto end_graphics_pass(VkCommandBuffer cmd) -> void;

    batleth::Device& m_device;
    std::unique_ptr<batleth::TransientAllocator> m_allocator;
    std::unique_ptr<batleth::BarrierBatcher> m_barrier_batcher;

    // Resource data
    std::vector<batleth::ResourceDesc> m_resources;
    std::vector<batleth::PhysicalResource> m_physical_resources;
    std::unordered_map<batleth::ResourceHandle, batleth::ResourceLifetime> m_lifetimes;

    // Pass data (in topological order)
    std::vector<batleth::PassDefinition> m_passes;

    // Pre-computed barriers per pass
    std::vector<std::vector<batleth::PassBarrier>> m_pre_pass_barriers;
    std::vector<batleth::PassBarrier> m_final_barriers;

    // External resource mapping
    std::unordered_map<batleth::ResourceHandle, ExternalResource> m_externals;

    // Resource state tracking
    std::unordered_map<batleth::ResourceHandle, batleth::ResourceState> m_resource_states;
};

} // namespace klingon
