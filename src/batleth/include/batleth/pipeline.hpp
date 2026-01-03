#pragma once

#include <vulkan/vulkan.h>
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

namespace batleth {

/**
 * RAII wrapper for VkPipeline and VkPipelineLayout.
 * Supports hot-reloading by allowing recreation from updated shaders.
 */
class BATLETH_API Pipeline {
public:
    struct ShaderStage {
        std::vector<std::uint32_t> spirv_code;
        VkShaderStageFlagBits stage;
    };

    struct Config {
        VkDevice device = VK_NULL_HANDLE;
        VkRenderPass render_pass = VK_NULL_HANDLE;
        VkExtent2D viewport_extent = {1280, 720};
        std::vector<ShaderStage> shader_stages;
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
        VkCullModeFlags cull_mode = VK_CULL_MODE_BACK_BIT;
        VkFrontFace front_face = VK_FRONT_FACE_CLOCKWISE;
    };

    explicit Pipeline(const Config& config);
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&) noexcept;
    Pipeline& operator=(Pipeline&&) noexcept;

    auto get_handle() const -> VkPipeline { return m_pipeline; }
    auto get_layout() const -> VkPipelineLayout { return m_pipeline_layout; }

    /**
     * Recreates the pipeline with new shader stages.
     * Used for hot-reloading shaders.
     */
    auto reload(const std::vector<ShaderStage>& new_shader_stages) -> void;

    /**
     * Helper to load SPIR-V code from a file.
     */
    static auto load_shader_from_file(const std::string& filepath) -> std::vector<std::uint32_t>;

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
    Config m_config;

    auto create_shader_module(const std::vector<std::uint32_t>& code) -> VkShaderModule;
    auto create_pipeline() -> void;
    auto cleanup() -> void;
};

} // namespace batleth
