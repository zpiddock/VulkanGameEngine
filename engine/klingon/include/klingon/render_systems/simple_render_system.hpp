#pragma once

#include <memory>
#include <vulkan/vulkan.h>

#include "batleth/device.hpp"
#include "batleth/pipeline.hpp"
#include "klingon/frame_info.hpp"
#include "klingon/render_system_interface.hpp"

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
     * Render mode for filtering opaque vs transparent geometry
     */
    enum class RenderMode {
        All,              // Render everything (default for backward compat)
        OpaqueOnly,       // Skip transparent meshes
        TransparentOnly   // Only render transparent meshes (sorted back-to-front)
    };

    /**
     * Render system for standard mesh rendering with lighting.
     * Uses push constants for per-object data and UBO for global scene data.
     */
    class KLINGON_API SimpleRenderSystem : public IRenderSystem {
    public:
        SimpleRenderSystem(
            batleth::Device &device,
            VkFormat swapchain_format,
            VkDescriptorSetLayout global_set_layout,
            VkDescriptorSetLayout forward_plus_set_layout = VK_NULL_HANDLE,  // Optional Forward+ layout
            VkDescriptorSetLayout texture_set_layout = VK_NULL_HANDLE,       // Bindless texture layout (Set 2)
            bool use_forward_plus = false
        );

        ~SimpleRenderSystem() override;

        SimpleRenderSystem(const SimpleRenderSystem &) = delete;

        SimpleRenderSystem &operator=(const SimpleRenderSystem &) = delete;

        // IRenderSystem interface
        auto render(FrameInfo &frame_info) -> void override;

        // Render with filtering mode (for opaque/transparent separation)
        auto render(FrameInfo &frame_info, RenderMode mode) -> void;

        auto on_swapchain_recreate(VkFormat format) -> void override;

        // Forward+ specific
        auto set_forward_plus_resources(VkDescriptorSet forward_plus_descriptor_set,
                                        uint32_t tile_count_x, uint32_t tile_count_y,
                                        uint32_t tile_size, uint32_t max_lights_per_tile) -> void;

    private:
        struct PushConstantData {
            glm::mat4 model_matrix{1.f};
            glm::mat4 normal_matrix{1.f};
            uint32_t material_index{0};        // Index into material buffer
            uint32_t _padding{0};              // Padding for std140 alignment (uvec2 requires 8-byte alignment)
            // Forward+ tile information (only used if Forward+ enabled)
            glm::uvec2 tile_count{0, 0};
            uint32_t tile_size{0};
            uint32_t max_lights_per_tile{0};
        };

        auto create_pipeline(VkFormat swapchain_format, VkDescriptorSetLayout global_set_layout) -> void;

        // Helper methods for transparency rendering
        auto is_material_transparent(const Material& material) const -> bool;
        auto render_mesh(GameObject& obj, size_t mesh_idx, FrameInfo& frame_info) -> void;

        batleth::Device &m_device;
        VkDescriptorSetLayout m_global_set_layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_forward_plus_set_layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_texture_set_layout = VK_NULL_HANDLE;
        VkFormat m_swapchain_format = VK_FORMAT_UNDEFINED;
        bool m_use_forward_plus = true;
        std::vector<std::unique_ptr<batleth::Shader> > m_shaders;
        std::unique_ptr<batleth::Pipeline> m_pipeline;

        // Forward+ state (set per-frame)
        VkDescriptorSet m_forward_plus_descriptor_set = VK_NULL_HANDLE;
        uint32_t m_tile_count_x = 0;
        uint32_t m_tile_count_y = 0;
        uint32_t m_tile_size = 0;
        uint32_t m_max_lights_per_tile = 0;
    };
} // namespace klingon
