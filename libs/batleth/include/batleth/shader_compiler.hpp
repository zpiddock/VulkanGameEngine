#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>
#include <memory>

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
    // Forward declaration
    class ShaderCache;

    /**
 * Compiles GLSL shader source code to SPIR-V bytecode at runtime.
 * Uses GLSLang (built from source) for compilation.
 * Supports automatic caching to improve startup times.
 */
    class BATLETH_API ShaderCompiler {
    public:
        enum class Stage {
            Vertex,
            Fragment,
            Compute,
            Geometry,
            TessellationControl,
            TessellationEvaluation
        };

        enum class OptimizationLevel {
            None, // No optimization
            Size, // Optimize for size
            Performance // Optimize for performance
        };

        struct CompileOptions {
            Stage stage = Stage::Vertex;
            OptimizationLevel optimization = OptimizationLevel::Performance;
            bool generate_debug_info = false;
            bool use_cache = true; // Enable shader caching
            std::vector<std::string> include_paths;
            std::vector<std::pair<std::string, std::string> > macro_definitions;
        };

        struct CompileResult {
            bool success = false;
            std::vector<std::uint32_t> spirv;
            std::string error_message;
            std::vector<std::string> warnings;
        };

        ShaderCompiler();

        ~ShaderCompiler();

        /**
     * Compile GLSL source code to SPIR-V.
     * @param source GLSL source code
     * @param source_name Name for error messages (e.g., filename)
     * @param options Compilation options
     * @return Compilation result with SPIR-V or error message
     */
        auto compile(const std::string &source,
                     const std::string &source_name,
                     const CompileOptions &options) -> CompileResult;

        /**
     * Compile GLSL file to SPIR-V.
     * @param filepath Path to GLSL source file
     * @param options Compilation options
     * @return Compilation result with SPIR-V or error message
     */
        auto compile_file(const std::filesystem::path &filepath,
                          const CompileOptions &options) -> CompileResult;

        /**
     * Get the shader cache instance.
     */
        auto get_cache() -> ShaderCache &;

        /**
     * Clear all cached shaders.
     */
        auto clear_cache() -> void;

    private:
        static auto stage_to_glslang_stage(Stage stage) -> int;

        std::unique_ptr<ShaderCache> m_cache;
    };
} // namespace batleth