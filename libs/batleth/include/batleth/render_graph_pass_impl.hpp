#pragma once

// This header provides inline implementations for PassExecutionContext methods
// It must be included AFTER klingon/render_graph.hpp to resolve circular dependencies

#include "render_graph_pass.hpp"

namespace klingon {
    class CompiledRenderGraph;
}

namespace batleth {
    inline auto PassExecutionContext::get_image(ResourceHandle handle) const -> VkImage {
        if (graph) {
            return graph->get_image(handle);
        }
        return VK_NULL_HANDLE;
    }

    inline auto PassExecutionContext::get_image_view(ResourceHandle handle) const -> VkImageView {
        if (graph) {
            return graph->get_image_view(handle);
        }
        return VK_NULL_HANDLE;
    }

    inline auto PassExecutionContext::get_buffer(ResourceHandle handle) const -> VkBuffer {
        if (graph) {
            return graph->get_buffer(handle);
        }
        return VK_NULL_HANDLE;
    }

    inline auto PassExecutionContext::get_image_format(ResourceHandle handle) const -> VkFormat {
        if (graph) {
            return graph->get_image_format(handle);
        }
        return VK_FORMAT_UNDEFINED;
    }

    inline auto PassExecutionContext::get_image_extent(ResourceHandle handle) const -> VkExtent3D {
        if (graph) {
            return graph->get_image_extent(handle);
        }
        return {0, 0, 0};
    }
} // namespace batleth
