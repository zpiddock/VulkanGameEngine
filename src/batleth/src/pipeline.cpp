#include "batleth/pipeline.hpp"
#include "federation/log.hpp"
#include <stdexcept>
#include <fstream>
#include <array>

namespace batleth {

Pipeline::Pipeline(const Config& config) : m_device(config.device), m_config(config) {
    FED_INFO("Creating graphics pipeline with {} shader stages", config.shader_stages.size());

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 0;
    pipeline_layout_info.pSetLayouts = nullptr;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = nullptr;

    if (::vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr, &m_pipeline_layout) != VK_SUCCESS) {
        FED_FATAL("Failed to create pipeline layout");
        throw std::runtime_error("Failed to create pipeline layout");
    }

    create_pipeline();
    FED_DEBUG("Graphics pipeline created successfully");
}

Pipeline::~Pipeline() {
    cleanup();

    if (m_pipeline_layout != VK_NULL_HANDLE) {
        ::vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
    }
}

Pipeline::Pipeline(Pipeline&& other) noexcept
    : m_device(other.m_device)
    , m_pipeline(other.m_pipeline)
    , m_pipeline_layout(other.m_pipeline_layout)
    , m_config(std::move(other.m_config)) {
    other.m_device = VK_NULL_HANDLE;
    other.m_pipeline = VK_NULL_HANDLE;
    other.m_pipeline_layout = VK_NULL_HANDLE;
}

auto Pipeline::operator=(Pipeline&& other) noexcept -> Pipeline& {
    if (this != &other) {
        cleanup();

        if (m_pipeline_layout != VK_NULL_HANDLE) {
            ::vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
        }

        m_device = other.m_device;
        m_pipeline = other.m_pipeline;
        m_pipeline_layout = other.m_pipeline_layout;
        m_config = std::move(other.m_config);

        other.m_device = VK_NULL_HANDLE;
        other.m_pipeline = VK_NULL_HANDLE;
        other.m_pipeline_layout = VK_NULL_HANDLE;
    }
    return *this;
}

auto Pipeline::reload(const std::vector<ShaderStage>& new_shader_stages) -> void {
    FED_INFO("Hot-reloading pipeline with {} new shader stages", new_shader_stages.size());

    m_config.shader_stages = new_shader_stages;

    // Wait for device to be idle before recreating pipeline
    ::vkDeviceWaitIdle(m_device);

    cleanup();
    create_pipeline();

    FED_INFO("Pipeline hot-reloaded successfully");
}

auto Pipeline::load_shader_from_file(const std::string& filepath) -> std::vector<std::uint32_t> {
    FED_DEBUG("Loading shader from file: {}", filepath);

    std::ifstream file(filepath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        FED_ERROR("Failed to open shader file: {}", filepath);
        throw std::runtime_error("Failed to open shader file: " + filepath);
    }

    auto file_size = static_cast<std::size_t>(file.tellg());
    std::vector<std::uint32_t> buffer(file_size / sizeof(std::uint32_t));

    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), file_size);
    file.close();

    return buffer;
}

auto Pipeline::create_shader_module(const std::vector<std::uint32_t>& code) -> VkShaderModule {
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size() * sizeof(std::uint32_t);
    create_info.pCode = code.data();

    VkShaderModule shader_module;
    if (::vkCreateShaderModule(m_device, &create_info, nullptr, &shader_module) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }

    return shader_module;
}

auto Pipeline::create_pipeline() -> void {
    std::vector<VkShaderModule> shader_modules;
    std::vector<VkPipelineShaderStageCreateInfo> shader_stage_infos;

    // Create shader modules and stage infos
    for (const auto& stage : m_config.shader_stages) {
        auto shader_module = create_shader_module(stage.spirv_code);
        shader_modules.push_back(shader_module);

        VkPipelineShaderStageCreateInfo shader_stage_info{};
        shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_info.stage = stage.stage;
        shader_stage_info.module = shader_module;
        shader_stage_info.pName = "main";

        shader_stage_infos.push_back(shader_stage_info);
    }

    // Vertex input state (empty for now)
    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 0;
    vertex_input_info.pVertexBindingDescriptions = nullptr;
    vertex_input_info.vertexAttributeDescriptionCount = 0;
    vertex_input_info.pVertexAttributeDescriptions = nullptr;

    // Input assembly state
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = m_config.topology;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_config.viewport_extent.width);
    viewport.height = static_cast<float>(m_config.viewport_extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_config.viewport_extent;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    // Rasterization state
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = m_config.polygon_mode;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = m_config.cull_mode;
    rasterizer.frontFace = m_config.front_face;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling state
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Color blending
    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = static_cast<std::uint32_t>(shader_stage_infos.size());
    pipeline_info.pStages = shader_stage_infos.data();
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = nullptr;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = nullptr;
    pipeline_info.layout = m_pipeline_layout;
    pipeline_info.renderPass = m_config.render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    if (::vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_pipeline) != VK_SUCCESS) {
        // Cleanup shader modules before throwing
        for (auto module : shader_modules) {
            ::vkDestroyShaderModule(m_device, module, nullptr);
        }
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    // Cleanup shader modules
    for (auto module : shader_modules) {
        ::vkDestroyShaderModule(m_device, module, nullptr);
    }
}

auto Pipeline::cleanup() -> void {
    if (m_pipeline != VK_NULL_HANDLE) {
        ::vkDestroyPipeline(m_device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
}

} // namespace batleth
