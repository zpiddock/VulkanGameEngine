#include "batleth/shader_compiler.hpp"
#include "batleth/shader_cache.hpp"
#include "federation/log.hpp"

#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <fstream>
#include <sstream>

namespace batleth {
    namespace {
        // Initialize glslang library (must be done once)
        struct GLSLangInitializer {
            GLSLangInitializer() {
                glslang::InitializeProcess();
            }

            ~GLSLangInitializer() {
                glslang::FinalizeProcess();
            }
        };

        // Global initializer
        GLSLangInitializer g_glslang_init;

        // Default TBuiltInResource configuration
        const TBuiltInResource default_builtin_resources = {
            .maxLights = 32,
            .maxClipPlanes = 6,
            .maxTextureUnits = 32,
            .maxTextureCoords = 32,
            .maxVertexAttribs = 64,
            .maxVertexUniformComponents = 4096,
            .maxVaryingFloats = 64,
            .maxVertexTextureImageUnits = 32,
            .maxCombinedTextureImageUnits = 80,
            .maxTextureImageUnits = 32,
            .maxFragmentUniformComponents = 4096,
            .maxDrawBuffers = 32,
            .maxVertexUniformVectors = 128,
            .maxVaryingVectors = 8,
            .maxFragmentUniformVectors = 16,
            .maxVertexOutputVectors = 16,
            .maxFragmentInputVectors = 15,
            .minProgramTexelOffset = -8,
            .maxProgramTexelOffset = 7,
            .maxClipDistances = 8,
            .maxComputeWorkGroupCountX = 65535,
            .maxComputeWorkGroupCountY = 65535,
            .maxComputeWorkGroupCountZ = 65535,
            .maxComputeWorkGroupSizeX = 1024,
            .maxComputeWorkGroupSizeY = 1024,
            .maxComputeWorkGroupSizeZ = 64,
            .maxComputeUniformComponents = 1024,
            .maxComputeTextureImageUnits = 16,
            .maxComputeImageUniforms = 8,
            .maxComputeAtomicCounters = 8,
            .maxComputeAtomicCounterBuffers = 1,
            .maxVaryingComponents = 60,
            .maxVertexOutputComponents = 64,
            .maxGeometryInputComponents = 64,
            .maxGeometryOutputComponents = 128,
            .maxFragmentInputComponents = 128,
            .maxImageUnits = 8,
            .maxCombinedImageUnitsAndFragmentOutputs = 8,
            .maxCombinedShaderOutputResources = 8,
            .maxImageSamples = 0,
            .maxVertexImageUniforms = 0,
            .maxTessControlImageUniforms = 0,
            .maxTessEvaluationImageUniforms = 0,
            .maxGeometryImageUniforms = 0,
            .maxFragmentImageUniforms = 8,
            .maxCombinedImageUniforms = 8,
            .maxGeometryTextureImageUnits = 16,
            .maxGeometryOutputVertices = 256,
            .maxGeometryTotalOutputComponents = 1024,
            .maxGeometryUniformComponents = 1024,
            .maxGeometryVaryingComponents = 64,
            .maxTessControlInputComponents = 128,
            .maxTessControlOutputComponents = 128,
            .maxTessControlTextureImageUnits = 16,
            .maxTessControlUniformComponents = 1024,
            .maxTessControlTotalOutputComponents = 4096,
            .maxTessEvaluationInputComponents = 128,
            .maxTessEvaluationOutputComponents = 128,
            .maxTessEvaluationTextureImageUnits = 16,
            .maxTessEvaluationUniformComponents = 1024,
            .maxTessPatchComponents = 120,
            .maxPatchVertices = 32,
            .maxTessGenLevel = 64,
            .maxViewports = 16,
            .maxVertexAtomicCounters = 0,
            .maxTessControlAtomicCounters = 0,
            .maxTessEvaluationAtomicCounters = 0,
            .maxGeometryAtomicCounters = 0,
            .maxFragmentAtomicCounters = 8,
            .maxCombinedAtomicCounters = 8,
            .maxAtomicCounterBindings = 1,
            .maxVertexAtomicCounterBuffers = 0,
            .maxTessControlAtomicCounterBuffers = 0,
            .maxTessEvaluationAtomicCounterBuffers = 0,
            .maxGeometryAtomicCounterBuffers = 0,
            .maxFragmentAtomicCounterBuffers = 1,
            .maxCombinedAtomicCounterBuffers = 1,
            .maxAtomicCounterBufferSize = 16384,
            .maxTransformFeedbackBuffers = 4,
            .maxTransformFeedbackInterleavedComponents = 64,
            .maxCullDistances = 8,
            .maxCombinedClipAndCullDistances = 8,
            .maxSamples = 4,
            .maxMeshOutputVerticesNV = 256,
            .maxMeshOutputPrimitivesNV = 512,
            .maxMeshWorkGroupSizeX_NV = 32,
            .maxMeshWorkGroupSizeY_NV = 1,
            .maxMeshWorkGroupSizeZ_NV = 1,
            .maxTaskWorkGroupSizeX_NV = 32,
            .maxTaskWorkGroupSizeY_NV = 1,
            .maxTaskWorkGroupSizeZ_NV = 1,
            .maxMeshViewCountNV = 4,
            .maxMeshOutputVerticesEXT = 256,
            .maxMeshOutputPrimitivesEXT = 256,
            .maxMeshWorkGroupSizeX_EXT = 128,
            .maxMeshWorkGroupSizeY_EXT = 128,
            .maxMeshWorkGroupSizeZ_EXT = 128,
            .maxTaskWorkGroupSizeX_EXT = 128,
            .maxTaskWorkGroupSizeY_EXT = 128,
            .maxTaskWorkGroupSizeZ_EXT = 128,
            .maxMeshViewCountEXT = 4,
            .maxDualSourceDrawBuffersEXT = 1,
            .limits = {
                .nonInductiveForLoops = true,
                .whileLoops = true,
                .doWhileLoops = true,
                .generalUniformIndexing = true,
                .generalAttributeMatrixVectorIndexing = true,
                .generalVaryingIndexing = true,
                .generalSamplerIndexing = true,
                .generalVariableIndexing = true,
                .generalConstantMatrixVectorIndexing = true,
            }
        };
    }

    ShaderCompiler::ShaderCompiler()
        : m_cache(std::make_unique<ShaderCache>()) {
    }

    ShaderCompiler::~ShaderCompiler() = default;

    auto ShaderCompiler::compile(const std::string &source,
                                 const std::string &source_name,
                                 const CompileOptions &options) -> CompileResult {
        CompileResult result;

        // Convert stage
        EShLanguage stage = static_cast<EShLanguage>(stage_to_glslang_stage(options.stage));

        // Create shader
        glslang::TShader shader(stage);
        const char *source_cstr = source.c_str();
        shader.setStrings(&source_cstr, 1);

        // Set environment
        shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 130);
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
        shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_6);

        // Add macro definitions
        std::string preamble;
        for (const auto &[name, value]: options.macro_definitions) {
            preamble += "#define " + name + " " + value + "\n";
        }
        if (!preamble.empty()) {
            shader.setPreamble(preamble.c_str());
        }

        // Parse
        FED_DEBUG("Compiling shader: {}", source_name);
        const int default_version = 450;
        EShMessages messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);

        if (options.generate_debug_info) {
            messages = static_cast<EShMessages>(messages | EShMsgDebugInfo);
        }

        if (!shader.parse(&default_builtin_resources, default_version, false, messages)) {
            result.success = false;
            result.error_message = shader.getInfoLog();
            FED_ERROR("Shader compilation failed for {}: {}", source_name, result.error_message);
            return result;
        }

        // Link
        glslang::TProgram program;
        program.addShader(&shader);

        if (!program.link(messages)) {
            result.success = false;
            result.error_message = program.getInfoLog();
            FED_ERROR("Shader linking failed for {}: {}", source_name, result.error_message);
            return result;
        }

        // Get warnings
        if (shader.getInfoLog() && strlen(shader.getInfoLog()) > 0) {
            result.warnings.push_back(shader.getInfoLog());
            FED_WARN("Shader compilation warnings for {}: {}", source_name, shader.getInfoLog());
        }

        // Generate SPIR-V
        glslang::SpvOptions spv_options;
        spv_options.generateDebugInfo = options.generate_debug_info;
        spv_options.disableOptimizer = (options.optimization == OptimizationLevel::None);
        spv_options.optimizeSize = (options.optimization == OptimizationLevel::Size);
        spv_options.validate = true;

        spv::SpvBuildLogger logger;
        glslang::GlslangToSpv(*program.getIntermediate(stage), result.spirv, &logger, &spv_options);

        if (!logger.getAllMessages().empty()) {
            FED_WARN("SPIR-V generation messages: {}", logger.getAllMessages());
        }

        result.success = true;
        FED_DEBUG("Successfully compiled {} to SPIR-V ({} bytes)",
                  source_name, result.spirv.size() * sizeof(std::uint32_t));

        return result;
    }

    auto ShaderCompiler::compile_file(const std::filesystem::path &filepath,
                                      const CompileOptions &options) -> CompileResult {
        CompileResult result;

        // Try cache first if enabled
        if (options.use_cache && m_cache) {
            auto cached_spirv = m_cache->lookup(filepath);
            if (cached_spirv) {
                result.success = true;
                result.spirv = std::move(*cached_spirv);
                return result;
            }
        }

        // Read source file
        std::ifstream file(filepath);
        if (!file.is_open()) {
            result.success = false;
            result.error_message = "Failed to open file: " + filepath.string();
            FED_ERROR("{}", result.error_message);
            return result;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string source = buffer.str();

        // Compile with filename for better error messages
        result = compile(source, filepath.string(), options);

        // Store in cache if compilation succeeded and caching is enabled
        if (result.success && options.use_cache && m_cache) {
            m_cache->store(filepath, result.spirv);
        }

        return result;
    }

    auto ShaderCompiler::get_cache() -> ShaderCache & {
        return *m_cache;
    }

    auto ShaderCompiler::clear_cache() -> void {
        if (m_cache) {
            m_cache->clear();
        }
    }

    auto ShaderCompiler::stage_to_glslang_stage(Stage stage) -> int {
        switch (stage) {
            case Stage::Vertex: return static_cast<int>(EShLangVertex);
            case Stage::Fragment: return static_cast<int>(EShLangFragment);
            case Stage::Compute: return static_cast<int>(EShLangCompute);
            case Stage::Geometry: return static_cast<int>(EShLangGeometry);
            case Stage::TessellationControl: return static_cast<int>(EShLangTessControl);
            case Stage::TessellationEvaluation: return static_cast<int>(EShLangTessEvaluation);
            default: return static_cast<int>(EShLangVertex);
        }
    }
} // namespace batleth