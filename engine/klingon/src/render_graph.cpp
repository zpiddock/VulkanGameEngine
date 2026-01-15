#include "klingon/render_graph.hpp"
#include "klingon/renderer.hpp"
#include "batleth/device.hpp"
#include "federation/log.hpp"

#include <algorithm>
#include <queue>
#include <stdexcept>

namespace klingon {

// ============================================================================
// RenderGraph Implementation
// ============================================================================

RenderGraph::RenderGraph(Renderer& renderer)
    : m_renderer(renderer) {
    m_builder = std::make_unique<RenderGraphBuilder>(renderer.get_device_ref());

    // Reserve handle 0 for backbuffer
    m_backbuffer_handle = 0;

    FED_INFO("RenderGraph created");
}

RenderGraph::~RenderGraph() = default;

auto RenderGraph::begin_build() -> RenderGraphBuilder& {
    m_builder->clear();
    m_compiled.reset();
    m_needs_recompile = true;

    // Pre-register backbuffer as external resource
    ExternalResource backbuffer_external{};
    backbuffer_external.handle = m_backbuffer_handle;
    backbuffer_external.type = batleth::ResourceType::Image;
    backbuffer_external.initial_state = {
        .stage_mask = VK_PIPELINE_STAGE_2_NONE,
        .access_mask = VK_ACCESS_2_NONE,
        .layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .queue_family = VK_QUEUE_FAMILY_IGNORED
    };
    backbuffer_external.final_state = {
        .stage_mask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        .access_mask = VK_ACCESS_2_NONE,
        .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .queue_family = VK_QUEUE_FAMILY_IGNORED
    };

    m_builder->import_external("backbuffer", backbuffer_external);

    return *m_builder;
}

auto RenderGraph::compile() -> bool {
    if (!m_builder) {
        FED_ERROR("Cannot compile: no builder available");
        return false;
    }

    try {
        m_compiled = std::make_unique<CompiledRenderGraph>(
            m_renderer.get_device_ref(),
            *m_builder,
            m_renderer.get_instance(),
            m_renderer.get_physical_device()
        );

        m_needs_recompile = false;
        FED_INFO("RenderGraph compiled successfully with {} passes", m_compiled->get_pass_count());
        return true;
    }
    catch (const std::exception& e) {
        FED_ERROR("Failed to compile render graph: {}", e.what());
        return false;
    }
}

auto RenderGraph::execute(VkCommandBuffer cmd, std::uint32_t frame_index, float delta_time) -> void {
    if (!m_compiled) {
        FED_ERROR("Cannot execute: graph not compiled");
        return;
    }

    if (m_needs_recompile) {
        FED_WARN("Executing render graph that needs recompilation");
    }

    // Update backbuffer in compiled graph
    m_compiled->update_external(m_backbuffer_handle, m_backbuffer);

    m_compiled->execute(cmd, frame_index, delta_time, m_backbuffer.extent);
}

auto RenderGraph::set_backbuffer(
    VkImage image,
    VkImageView view,
    VkFormat format,
    VkExtent2D extent
) -> void {
    m_backbuffer.handle = m_backbuffer_handle;
    m_backbuffer.type = batleth::ResourceType::Image;
    m_backbuffer.image = image;
    m_backbuffer.view = view;
    m_backbuffer.format = format;
    m_backbuffer.extent = extent;
    m_backbuffer.initial_state = {
        .stage_mask = VK_PIPELINE_STAGE_2_NONE,
        .access_mask = VK_ACCESS_2_NONE,
        .layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .queue_family = VK_QUEUE_FAMILY_IGNORED
    };
    m_backbuffer.final_state = {
        .stage_mask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        .access_mask = VK_ACCESS_2_NONE,
        .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .queue_family = VK_QUEUE_FAMILY_IGNORED
    };
}

auto RenderGraph::get_backbuffer_handle() const -> batleth::ResourceHandle {
    return m_backbuffer_handle;
}

auto RenderGraph::is_compiled() const -> bool {
    return m_compiled != nullptr && !m_needs_recompile;
}

auto RenderGraph::get_pass_count() const -> std::size_t {
    return m_compiled ? m_compiled->get_pass_count() : 0;
}

auto RenderGraph::invalidate() -> void {
    m_needs_recompile = true;
}

auto RenderGraph::get_render_extent() const -> VkExtent2D {
    return m_backbuffer.extent;
}

// ============================================================================
// RenderGraphBuilder Implementation
// ============================================================================

RenderGraphBuilder::RenderGraphBuilder(batleth::Device& device)
    : m_device(device) {
}

RenderGraphBuilder::~RenderGraphBuilder() = default;

auto RenderGraphBuilder::create_image(
    const std::string& name,
    const batleth::ImageResourceDesc& desc
) -> batleth::ResourceHandle {
    batleth::ResourceHandle handle = m_next_handle++;

    batleth::ResourceDesc resource{};
    resource.name = name;
    resource.type = batleth::ResourceType::Image;
    resource.desc = desc;

    m_resources.push_back(resource);

    FED_DEBUG("Created image resource '{}' with handle {}", name, handle);
    return handle;
}

auto RenderGraphBuilder::create_buffer(
    const std::string& name,
    const batleth::BufferResourceDesc& desc
) -> batleth::ResourceHandle {
    batleth::ResourceHandle handle = m_next_handle++;

    batleth::ResourceDesc resource{};
    resource.name = name;
    resource.type = batleth::ResourceType::Buffer;
    resource.desc = desc;

    m_resources.push_back(resource);

    FED_DEBUG("Created buffer resource '{}' with handle {}", name, handle);
    return handle;
}

auto RenderGraphBuilder::import_external(
    const std::string& name,
    const ExternalResource& external
) -> batleth::ResourceHandle {
    batleth::ResourceHandle handle = external.handle;

    // Ensure handle is allocated
    if (handle >= m_next_handle) {
        m_next_handle = handle + 1;
    }

    // Add placeholder resource desc
    while (m_resources.size() <= handle) {
        batleth::ResourceDesc placeholder{};
        placeholder.name = "placeholder";
        m_resources.push_back(placeholder);
    }

    batleth::ResourceDesc resource{};
    resource.name = name;
    resource.type = external.type;

    if (external.type == batleth::ResourceType::Image) {
        batleth::ImageResourceDesc image_desc{};
        image_desc.format = external.format;
        image_desc.extent = {external.extent.width, external.extent.height, 1};
        image_desc.is_transient = false;
        resource.desc = image_desc;
    } else {
        batleth::BufferResourceDesc buffer_desc{};
        buffer_desc.size = external.size;
        buffer_desc.is_transient = false;
        resource.desc = buffer_desc;
    }

    m_resources[handle] = resource;
    m_externals[handle] = external;

    FED_DEBUG("Imported external resource '{}' with handle {}", name, handle);
    return handle;
}

auto RenderGraphBuilder::add_graphics_pass(
    const std::string& name,
    batleth::PassExecuteCallback execute
) -> RenderGraphBuilder& {
    batleth::PassDefinition pass{};
    pass.config.name = name;
    pass.config.type = batleth::PassType::Graphics;
    pass.config.queue = batleth::QueueType::Graphics;
    pass.execute = std::move(execute);
    pass.index = static_cast<std::uint32_t>(m_passes.size());

    m_passes.push_back(std::move(pass));
    m_current_pass = &m_passes.back();

    FED_DEBUG("Added graphics pass '{}'", name);
    return *this;
}

auto RenderGraphBuilder::add_compute_pass(
    const std::string& name,
    batleth::PassExecuteCallback execute
) -> RenderGraphBuilder& {
    batleth::PassDefinition pass{};
    pass.config.name = name;
    pass.config.type = batleth::PassType::Compute;
    pass.config.queue = batleth::QueueType::Compute;
    pass.execute = std::move(execute);
    pass.index = static_cast<std::uint32_t>(m_passes.size());

    m_passes.push_back(std::move(pass));
    m_current_pass = &m_passes.back();

    FED_DEBUG("Added compute pass '{}'", name);
    return *this;
}

auto RenderGraphBuilder::add_transfer_pass(
    const std::string& name,
    batleth::PassExecuteCallback execute
) -> RenderGraphBuilder& {
    batleth::PassDefinition pass{};
    pass.config.name = name;
    pass.config.type = batleth::PassType::Transfer;
    pass.config.queue = batleth::QueueType::Transfer;
    pass.execute = std::move(execute);
    pass.index = static_cast<std::uint32_t>(m_passes.size());

    m_passes.push_back(std::move(pass));
    m_current_pass = &m_passes.back();

    FED_DEBUG("Added transfer pass '{}'", name);
    return *this;
}

auto RenderGraphBuilder::read(
    batleth::ResourceHandle handle,
    batleth::ResourceUsage usage
) -> RenderGraphBuilder& {
    if (!m_current_pass) {
        FED_ERROR("No current pass - call add_*_pass() first");
        return *this;
    }

    batleth::ResourceAccess access{};
    access.handle = handle;
    access.usage = usage;

    m_current_pass->config.reads.push_back(access);
    return *this;
}

auto RenderGraphBuilder::write(
    batleth::ResourceHandle handle,
    batleth::ResourceUsage usage
) -> RenderGraphBuilder& {
    if (!m_current_pass) {
        FED_ERROR("No current pass - call add_*_pass() first");
        return *this;
    }

    batleth::ResourceAccess access{};
    access.handle = handle;
    access.usage = usage;

    m_current_pass->config.writes.push_back(access);
    return *this;
}

auto RenderGraphBuilder::set_color_attachment(
    std::uint32_t index,
    batleth::ResourceHandle handle,
    VkAttachmentLoadOp load_op,
    VkClearColorValue clear_value
) -> RenderGraphBuilder& {
    if (!m_current_pass) {
        FED_ERROR("No current pass - call add_*_pass() first");
        return *this;
    }

    // Ensure vector is large enough
    while (m_current_pass->config.color_attachments.size() <= index) {
        m_current_pass->config.color_attachments.push_back({});
    }

    batleth::ColorAttachmentConfig& config = m_current_pass->config.color_attachments[index];
    config.handle = handle;
    config.load_op = load_op;
    config.clear_value = clear_value;

    // Also add as write dependency
    write(handle, batleth::ResourceUsage::ColorAttachment);

    return *this;
}

auto RenderGraphBuilder::set_depth_attachment(
    batleth::ResourceHandle handle,
    VkAttachmentLoadOp load_op,
    VkClearDepthStencilValue clear_value
) -> RenderGraphBuilder& {
    if (!m_current_pass) {
        FED_ERROR("No current pass - call add_*_pass() first");
        return *this;
    }

    m_current_pass->config.depth_attachment.handle = handle;
    m_current_pass->config.depth_attachment.load_op = load_op;
    m_current_pass->config.depth_attachment.clear_value = clear_value;
    m_current_pass->config.has_depth_attachment = true;

    // Add as write dependency
    write(handle, batleth::ResourceUsage::DepthStencilReadWrite);

    return *this;
}

auto RenderGraphBuilder::set_queue(batleth::QueueType queue) -> RenderGraphBuilder& {
    if (!m_current_pass) {
        FED_ERROR("No current pass - call add_*_pass() first");
        return *this;
    }

    m_current_pass->config.queue = queue;
    return *this;
}

auto RenderGraphBuilder::clear() -> void {
    m_resources.clear();
    m_passes.clear();
    m_externals.clear();
    m_current_pass = nullptr;
    m_next_handle = 0;
}

auto RenderGraphBuilder::get_resources() const -> const std::vector<batleth::ResourceDesc>& {
    return m_resources;
}

auto RenderGraphBuilder::get_passes() -> std::vector<batleth::PassDefinition>& {
    return m_passes;
}

auto RenderGraphBuilder::get_external_resources() const -> const std::unordered_map<batleth::ResourceHandle, ExternalResource>& {
    return m_externals;
}

// ============================================================================
// CompiledRenderGraph Implementation
// ============================================================================

CompiledRenderGraph::CompiledRenderGraph(
    batleth::Device& device,
    RenderGraphBuilder& builder,
    VkInstance instance,
    VkPhysicalDevice physical_device
)
    : m_device(device) {

    // Copy resources and passes from builder
    m_resources = builder.get_resources();
    m_passes = builder.get_passes();
    m_externals = builder.get_external_resources();

    // Initialize physical resources vector
    m_physical_resources.resize(m_resources.size());

    // Create transient allocator
    batleth::TransientAllocator::Config alloc_config{};
    alloc_config.instance = instance;
    alloc_config.physical_device = physical_device;
    alloc_config.device = device.get_logical_device();

    m_allocator = std::make_unique<batleth::TransientAllocator>(alloc_config);
    m_barrier_batcher = std::make_unique<batleth::BarrierBatcher>();

    // Compile the graph
    topological_sort();
    compute_lifetimes();
    allocate_resources();
    compute_barriers();

    FED_INFO("CompiledRenderGraph created with {} passes", m_passes.size());
}

CompiledRenderGraph::~CompiledRenderGraph() = default;

auto CompiledRenderGraph::topological_sort() -> void {
    const std::size_t n = m_passes.size();
    if (n == 0) return;

    // Build adjacency list based on resource dependencies
    std::vector<std::vector<std::uint32_t>> adj(n);
    std::vector<std::uint32_t> in_degree(n, 0);

    // Map resources to the last pass that wrote them
    std::unordered_map<batleth::ResourceHandle, std::uint32_t> last_writer;

    for (std::uint32_t i = 0; i < n; ++i) {
        auto& pass = m_passes[i];

        // Check reads - depend on last writer
        for (const auto& access : pass.config.reads) {
            auto it = last_writer.find(access.handle);
            if (it != last_writer.end() && it->second != i) {
                adj[it->second].push_back(i);
                in_degree[i]++;
                pass.dependencies.push_back(it->second);
            }
        }

        // Check color attachments as reads if load_op is LOAD
        for (const auto& attachment : pass.config.color_attachments) {
            if (attachment.load_op == VK_ATTACHMENT_LOAD_OP_LOAD) {
                auto it = last_writer.find(attachment.handle);
                if (it != last_writer.end() && it->second != i) {
                    adj[it->second].push_back(i);
                    in_degree[i]++;
                    pass.dependencies.push_back(it->second);
                }
            }
        }

        // Update last writer for writes
        for (const auto& access : pass.config.writes) {
            // Check for WAW dependency
            auto it = last_writer.find(access.handle);
            if (it != last_writer.end() && it->second != i) {
                adj[it->second].push_back(i);
                in_degree[i]++;
                pass.dependencies.push_back(it->second);
            }
            last_writer[access.handle] = i;
        }
    }

    // Kahn's algorithm for topological sort
    std::queue<std::uint32_t> queue;
    for (std::uint32_t i = 0; i < n; ++i) {
        if (in_degree[i] == 0) {
            queue.push(i);
        }
    }

    std::vector<std::uint32_t> order;
    order.reserve(n);

    while (!queue.empty()) {
        std::uint32_t u = queue.front();
        queue.pop();
        order.push_back(u);

        for (std::uint32_t v : adj[u]) {
            if (--in_degree[v] == 0) {
                queue.push(v);
            }
        }
    }

    if (order.size() != n) {
        FED_ERROR("Render graph has a cycle - topological sort failed");
        throw std::runtime_error("Render graph contains a cycle");
    }

    // Reorder passes
    std::vector<batleth::PassDefinition> sorted_passes(n);
    for (std::uint32_t i = 0; i < n; ++i) {
        sorted_passes[i] = std::move(m_passes[order[i]]);
        sorted_passes[i].topological_order = i;
    }
    m_passes = std::move(sorted_passes);

    FED_DEBUG("Topological sort complete");
}

auto CompiledRenderGraph::compute_lifetimes() -> void {
    for (const auto& pass : m_passes) {
        std::uint32_t pass_idx = pass.topological_order;

        // Update lifetimes for read resources
        for (const auto& access : pass.config.reads) {
            auto& lifetime = m_lifetimes[access.handle];
            if (lifetime.first_pass > pass_idx || m_lifetimes.find(access.handle) == m_lifetimes.end()) {
                lifetime.first_pass = pass_idx;
            }
            lifetime.last_pass = std::max(lifetime.last_pass, pass_idx);
        }

        // Update lifetimes for write resources
        for (const auto& access : pass.config.writes) {
            auto& lifetime = m_lifetimes[access.handle];
            if (lifetime.first_pass > pass_idx || m_lifetimes.find(access.handle) == m_lifetimes.end()) {
                lifetime.first_pass = pass_idx;
            }
            lifetime.last_pass = std::max(lifetime.last_pass, pass_idx);
        }

        // Update for attachments
        for (const auto& attachment : pass.config.color_attachments) {
            if (attachment.handle != batleth::INVALID_RESOURCE) {
                auto& lifetime = m_lifetimes[attachment.handle];
                if (lifetime.first_pass > pass_idx || m_lifetimes.find(attachment.handle) == m_lifetimes.end()) {
                    lifetime.first_pass = pass_idx;
                }
                lifetime.last_pass = std::max(lifetime.last_pass, pass_idx);
            }
        }

        if (pass.config.has_depth_attachment) {
            auto& lifetime = m_lifetimes[pass.config.depth_attachment.handle];
            if (lifetime.first_pass > pass_idx || m_lifetimes.find(pass.config.depth_attachment.handle) == m_lifetimes.end()) {
                lifetime.first_pass = pass_idx;
            }
            lifetime.last_pass = std::max(lifetime.last_pass, pass_idx);
        }
    }

    FED_DEBUG("Computed lifetimes for {} resources", m_lifetimes.size());
}

auto CompiledRenderGraph::allocate_resources() -> void {
    for (batleth::ResourceHandle handle = 0; handle < m_resources.size(); ++handle) {
        const auto& resource = m_resources[handle];

        // Skip external resources
        if (m_externals.find(handle) != m_externals.end()) {
            continue;
        }

        // Skip placeholder resources
        if (resource.name == "placeholder") {
            continue;
        }

        auto lifetime_it = m_lifetimes.find(handle);
        if (lifetime_it == m_lifetimes.end()) {
            FED_WARN("Resource '{}' has no lifetime - may be unused", resource.name);
            continue;
        }

        const auto& lifetime = lifetime_it->second;

        if (resource.type == batleth::ResourceType::Image) {
            const auto& desc = resource.get_image_desc();
            auto physical = m_allocator->allocate_image(desc, lifetime);

            m_physical_resources[handle].type = batleth::ResourceType::Image;
            m_physical_resources[handle].resource = physical;

            FED_DEBUG("Allocated image '{}' (handle {})", resource.name, handle);
        }
        else {
            const auto& desc = resource.get_buffer_desc();
            auto physical = m_allocator->allocate_buffer(desc, lifetime);

            m_physical_resources[handle].type = batleth::ResourceType::Buffer;
            m_physical_resources[handle].resource = physical;

            FED_DEBUG("Allocated buffer '{}' (handle {})", resource.name, handle);
        }
    }
}

auto CompiledRenderGraph::compute_barriers() -> void {
    m_pre_pass_barriers.resize(m_passes.size());

    // Initialize resource states
    for (const auto& [handle, external] : m_externals) {
        m_resource_states[handle] = external.initial_state;
    }

    // For transient resources, start undefined
    for (batleth::ResourceHandle handle = 0; handle < m_resources.size(); ++handle) {
        if (m_externals.find(handle) == m_externals.end()) {
            m_resource_states[handle] = {
                .stage_mask = VK_PIPELINE_STAGE_2_NONE,
                .access_mask = VK_ACCESS_2_NONE,
                .layout = VK_IMAGE_LAYOUT_UNDEFINED,
                .queue_family = VK_QUEUE_FAMILY_IGNORED
            };
        }
    }

    // Compute barriers for each pass
    for (std::size_t i = 0; i < m_passes.size(); ++i) {
        const auto& pass = m_passes[i];
        auto& barriers = m_pre_pass_barriers[i];

        // Process reads
        for (const auto& access : pass.config.reads) {
            auto required_state = batleth::compute_resource_state(access);
            auto& current_state = m_resource_states[access.handle];

            if (batleth::needs_barrier(current_state, required_state)) {
                barriers.push_back({
                    .resource = access.handle,
                    .before = current_state,
                    .after = required_state
                });
                current_state = required_state;
            }
        }

        // Process attachments
        for (const auto& attachment : pass.config.color_attachments) {
            if (attachment.handle == batleth::INVALID_RESOURCE) continue;

            batleth::ResourceState required_state{
                .stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
                              (attachment.load_op == VK_ATTACHMENT_LOAD_OP_LOAD ? VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT : 0),
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .queue_family = VK_QUEUE_FAMILY_IGNORED
            };

            auto& current_state = m_resource_states[attachment.handle];
            if (batleth::needs_barrier(current_state, required_state)) {
                barriers.push_back({
                    .resource = attachment.handle,
                    .before = current_state,
                    .after = required_state
                });
            }
            current_state = required_state;
        }

        if (pass.config.has_depth_attachment) {
            auto handle = pass.config.depth_attachment.handle;
            batleth::ResourceState required_state{
                .stage_mask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                .access_mask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .queue_family = VK_QUEUE_FAMILY_IGNORED
            };

            auto& current_state = m_resource_states[handle];
            if (batleth::needs_barrier(current_state, required_state)) {
                barriers.push_back({
                    .resource = handle,
                    .before = current_state,
                    .after = required_state
                });
            }
            current_state = required_state;
        }

        // Process writes (update state after pass)
        for (const auto& access : pass.config.writes) {
            auto new_state = batleth::compute_resource_state(access);
            m_resource_states[access.handle] = new_state;
        }
    }

    // Compute final barriers for external resources
    for (const auto& [handle, external] : m_externals) {
        auto& current_state = m_resource_states[handle];
        if (batleth::needs_barrier(current_state, external.final_state)) {
            m_final_barriers.push_back({
                .resource = handle,
                .before = current_state,
                .after = external.final_state
            });
        }
    }

    FED_DEBUG("Computed barriers: {} passes, {} final barriers",
              m_pre_pass_barriers.size(), m_final_barriers.size());
}

auto CompiledRenderGraph::execute(
    VkCommandBuffer cmd,
    std::uint32_t frame_index,
    float delta_time,
    VkExtent2D extent
) -> void {
    for (std::size_t i = 0; i < m_passes.size(); ++i) {
        const auto& pass = m_passes[i];
        const auto& barriers = m_pre_pass_barriers[i];

        // Insert pre-pass barriers
        if (!barriers.empty()) {
            m_barrier_batcher->clear();
            for (const auto& barrier : barriers) {
                auto& resource = m_physical_resources[barrier.resource];

                // Check if this is an external resource
                auto ext_it = m_externals.find(barrier.resource);

                if (resource.is_image() || (ext_it != m_externals.end() && ext_it->second.type == batleth::ResourceType::Image)) {
                    VkImage image = VK_NULL_HANDLE;
                    VkFormat format = VK_FORMAT_UNDEFINED;

                    if (ext_it != m_externals.end()) {
                        image = ext_it->second.image;
                        format = ext_it->second.format;
                    } else {
                        const auto& physical = resource.get_image();
                        image = physical.image;
                        format = physical.format;
                    }

                    m_barrier_batcher->add_image_barrier(
                        image,
                        barrier.before,
                        barrier.after,
                        batleth::format_to_aspect_mask(format)
                    );
                }
                else {
                    VkBuffer buffer = VK_NULL_HANDLE;
                    if (ext_it != m_externals.end()) {
                        buffer = ext_it->second.buffer;
                    } else {
                        buffer = resource.get_buffer().buffer;
                    }

                    m_barrier_batcher->add_buffer_barrier(
                        buffer,
                        barrier.before,
                        barrier.after
                    );
                }
            }
            m_barrier_batcher->flush(cmd);
        }

        // Execute pass
        if (pass.config.type == batleth::PassType::Graphics) {
            begin_graphics_pass(cmd, pass, extent);
        }

        batleth::PassExecutionContext ctx{};
        ctx.command_buffer = cmd;
        ctx.frame_index = frame_index;
        ctx.delta_time = delta_time;
        ctx.render_extent = extent;
        ctx.config = &pass.config;
        ctx.graph = this;

        pass.execute(ctx);

        if (pass.config.type == batleth::PassType::Graphics) {
            end_graphics_pass(cmd);
        }
    }

    // Insert final barriers
    if (!m_final_barriers.empty()) {
        m_barrier_batcher->clear();
        for (const auto& barrier : m_final_barriers) {
            auto ext_it = m_externals.find(barrier.resource);
            if (ext_it != m_externals.end() && ext_it->second.type == batleth::ResourceType::Image) {
                m_barrier_batcher->add_image_barrier(
                    ext_it->second.image,
                    barrier.before,
                    barrier.after,
                    batleth::format_to_aspect_mask(ext_it->second.format)
                );
            }
        }
        m_barrier_batcher->flush(cmd);
    }
}

auto CompiledRenderGraph::update_external(
    batleth::ResourceHandle handle,
    const ExternalResource& external
) -> void {
    m_externals[handle] = external;
}

auto CompiledRenderGraph::get_pass_count() const -> std::size_t {
    return m_passes.size();
}

auto CompiledRenderGraph::get_image(batleth::ResourceHandle handle) const -> VkImage {
    auto ext_it = m_externals.find(handle);
    if (ext_it != m_externals.end()) {
        return ext_it->second.image;
    }

    if (handle < m_physical_resources.size() && m_physical_resources[handle].is_image()) {
        return m_physical_resources[handle].get_image().image;
    }

    return VK_NULL_HANDLE;
}

auto CompiledRenderGraph::get_image_view(batleth::ResourceHandle handle) const -> VkImageView {
    auto ext_it = m_externals.find(handle);
    if (ext_it != m_externals.end()) {
        return ext_it->second.view;
    }

    if (handle < m_physical_resources.size() && m_physical_resources[handle].is_image()) {
        return m_physical_resources[handle].get_image().view;
    }

    return VK_NULL_HANDLE;
}

auto CompiledRenderGraph::get_buffer(batleth::ResourceHandle handle) const -> VkBuffer {
    auto ext_it = m_externals.find(handle);
    if (ext_it != m_externals.end()) {
        return ext_it->second.buffer;
    }

    if (handle < m_physical_resources.size() && m_physical_resources[handle].is_buffer()) {
        return m_physical_resources[handle].get_buffer().buffer;
    }

    return VK_NULL_HANDLE;
}

auto CompiledRenderGraph::get_image_format(batleth::ResourceHandle handle) const -> VkFormat {
    auto ext_it = m_externals.find(handle);
    if (ext_it != m_externals.end()) {
        return ext_it->second.format;
    }

    if (handle < m_physical_resources.size() && m_physical_resources[handle].is_image()) {
        return m_physical_resources[handle].get_image().format;
    }

    return VK_FORMAT_UNDEFINED;
}

auto CompiledRenderGraph::get_image_extent(batleth::ResourceHandle handle) const -> VkExtent3D {
    auto ext_it = m_externals.find(handle);
    if (ext_it != m_externals.end()) {
        return {ext_it->second.extent.width, ext_it->second.extent.height, 1};
    }

    if (handle < m_physical_resources.size() && m_physical_resources[handle].is_image()) {
        return m_physical_resources[handle].get_image().extent;
    }

    return {0, 0, 0};
}

auto CompiledRenderGraph::begin_graphics_pass(
    VkCommandBuffer cmd,
    const batleth::PassDefinition& pass,
    VkExtent2D extent
) -> void {
    // Build color attachments
    std::vector<VkRenderingAttachmentInfo> color_attachments;
    for (const auto& attachment : pass.config.color_attachments) {
        if (attachment.handle == batleth::INVALID_RESOURCE) continue;

        VkRenderingAttachmentInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        info.imageView = get_image_view(attachment.handle);
        info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        info.loadOp = attachment.load_op;
        info.storeOp = attachment.store_op;
        info.clearValue.color = attachment.clear_value;

        color_attachments.push_back(info);
    }

    // Build depth attachment
    VkRenderingAttachmentInfo depth_attachment{};
    VkRenderingAttachmentInfo* depth_ptr = nullptr;

    if (pass.config.has_depth_attachment) {
        depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_attachment.imageView = get_image_view(pass.config.depth_attachment.handle);
        depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_attachment.loadOp = pass.config.depth_attachment.load_op;
        depth_attachment.storeOp = pass.config.depth_attachment.store_op;
        depth_attachment.clearValue.depthStencil = pass.config.depth_attachment.clear_value;
        depth_ptr = &depth_attachment;
    }

    // Begin rendering
    VkRenderingInfo rendering_info{};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea.offset = {0, 0};
    rendering_info.renderArea.extent = extent;
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = static_cast<std::uint32_t>(color_attachments.size());
    rendering_info.pColorAttachments = color_attachments.empty() ? nullptr : color_attachments.data();
    rendering_info.pDepthAttachment = depth_ptr;

    ::vkCmdBeginRendering(cmd, &rendering_info);

    // Set viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    ::vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    ::vkCmdSetScissor(cmd, 0, 1, &scissor);
}

auto CompiledRenderGraph::end_graphics_pass(VkCommandBuffer cmd) -> void {
    ::vkCmdEndRendering(cmd);
}

// ============================================================================
// PassExecutionContext resource access implementation
// ============================================================================

} // namespace klingon

// Implement PassExecutionContext methods that need CompiledRenderGraph
namespace batleth {

auto PassExecutionContext::get_image(ResourceHandle handle) const -> VkImage {
    if (graph) {
        return static_cast<const klingon::CompiledRenderGraph*>(graph)->get_image(handle);
    }
    return VK_NULL_HANDLE;
}

auto PassExecutionContext::get_image_view(ResourceHandle handle) const -> VkImageView {
    if (graph) {
        return static_cast<const klingon::CompiledRenderGraph*>(graph)->get_image_view(handle);
    }
    return VK_NULL_HANDLE;
}

auto PassExecutionContext::get_buffer(ResourceHandle handle) const -> VkBuffer {
    if (graph) {
        return static_cast<const klingon::CompiledRenderGraph*>(graph)->get_buffer(handle);
    }
    return VK_NULL_HANDLE;
}

auto PassExecutionContext::get_image_format(ResourceHandle handle) const -> VkFormat {
    if (graph) {
        return static_cast<const klingon::CompiledRenderGraph*>(graph)->get_image_format(handle);
    }
    return VK_FORMAT_UNDEFINED;
}

auto PassExecutionContext::get_image_extent(ResourceHandle handle) const -> VkExtent3D {
    if (graph) {
        return static_cast<const klingon::CompiledRenderGraph*>(graph)->get_image_extent(handle);
    }
    return {0, 0, 0};
}

} // namespace batleth
