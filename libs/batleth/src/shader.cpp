#include "batleth/shader.hpp"
#include "batleth/shader_compiler.hpp"
#include "federation/log.hpp"
#include <fstream>
#include <stdexcept>
#include <cstring>

namespace batleth {
    Shader::Shader(const Config &config)
        : m_device(config.device)
          , m_filepath(config.filepath)
          , m_stage(config.stage)
          , m_hot_reload_enabled(config.enable_hot_reload) {
        if (!std::filesystem::exists(m_filepath)) {
            throw std::runtime_error("Shader file does not exist: " + m_filepath.string());
        }

        // Load and create the shader module
        auto code = load_shader_code();
        create_shader_module(code);

        // Store the last write time for hot-reload detection
        m_last_write_time = std::filesystem::last_write_time(m_filepath);

        FED_INFO("Loaded shader: {}", m_filepath.string());
    }

    Shader::~Shader() {
        cleanup_shader_module();
    }

    Shader::Shader(Shader &&other) noexcept
        : m_device(other.m_device)
          , m_shader_module(other.m_shader_module)
          , m_filepath(std::move(other.m_filepath))
          , m_stage(other.m_stage)
          , m_hot_reload_enabled(other.m_hot_reload_enabled)
          , m_last_write_time(other.m_last_write_time)
          , m_reload_callback(std::move(other.m_reload_callback)) {
        other.m_device = VK_NULL_HANDLE;
        other.m_shader_module = VK_NULL_HANDLE;
    }

    auto Shader::operator=(Shader &&other) noexcept -> Shader & {
        if (this != &other) {
            cleanup_shader_module();

            m_device = other.m_device;
            m_shader_module = other.m_shader_module;
            m_filepath = std::move(other.m_filepath);
            m_stage = other.m_stage;
            m_hot_reload_enabled = other.m_hot_reload_enabled;
            m_last_write_time = other.m_last_write_time;
            m_reload_callback = std::move(other.m_reload_callback);

            other.m_device = VK_NULL_HANDLE;
            other.m_shader_module = VK_NULL_HANDLE;
        }
        return *this;
    }

    auto Shader::reload() -> bool {
        try {
            FED_INFO("Reloading shader: {}", m_filepath.string());

            // Load new shader code
            auto code = load_shader_code();

            // Create new shader module
            VkShaderModule new_module = VK_NULL_HANDLE;
            VkShaderModuleCreateInfo create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            create_info.codeSize = code.size();
            create_info.pCode = reinterpret_cast<const std::uint32_t *>(code.data());

            if (::vkCreateShaderModule(m_device, &create_info, nullptr, &new_module) != VK_SUCCESS) {
                FED_ERROR("Failed to reload shader: {}", m_filepath.string());
                return false;
            }

            // Cleanup old module
            cleanup_shader_module();

            // Replace with new module
            m_shader_module = new_module;
            m_last_write_time = std::filesystem::last_write_time(m_filepath);

            FED_INFO("Successfully reloaded shader: {}", m_filepath.string());

            // Invoke callback if set
            if (m_reload_callback) {
                m_reload_callback();
            }

            return true;
        } catch (const std::exception &e) {
            FED_ERROR("Exception while reloading shader: {}", e.what());
            return false;
        }
    }

    auto Shader::check_and_reload() -> bool {
        if (!m_hot_reload_enabled) {
            return false;
        }

        if (!std::filesystem::exists(m_filepath)) {
            return false;
        }

        auto current_write_time = std::filesystem::last_write_time(m_filepath);
        if (current_write_time > m_last_write_time) {
            return reload();
        }

        return false;
    }

    auto Shader::load_shader_code() -> std::vector<char> {
        auto extension = m_filepath.extension().string();

        // Check if this is a GLSL file that needs compilation
        bool is_glsl = (extension == ".vert" || extension == ".frag" ||
                        extension == ".comp" || extension == ".geom" ||
                        extension == ".tesc" || extension == ".tese" ||
                        extension == ".glsl");

        if (is_glsl) {
            // Compile GLSL to SPIR-V
            ShaderCompiler compiler;
            ShaderCompiler::CompileOptions opts;
            opts.stage = static_cast<ShaderCompiler::Stage>(m_stage);
            opts.optimization = m_hot_reload_enabled
                                    ? ShaderCompiler::OptimizationLevel::None
                                    : ShaderCompiler::OptimizationLevel::Performance;
            opts.generate_debug_info = true;

            auto result = compiler.compile_file(m_filepath, opts);

            if (!result.success) {
                throw std::runtime_error("Shader compilation failed: " + result.error_message);
            }

            // Convert SPIR-V uint32_t vector to char vector
            std::vector<char> buffer(result.spirv.size() * sizeof(std::uint32_t));
            std::memcpy(buffer.data(), result.spirv.data(), buffer.size());
            return buffer;
        }

        // Load pre-compiled SPIR-V
        std::ifstream file(m_filepath, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("Failed to open shader file: " + m_filepath.string());
        }

        auto file_size = static_cast<std::size_t>(file.tellg());
        std::vector<char> buffer(file_size);

        file.seekg(0);
        file.read(buffer.data(), static_cast<std::streamsize>(file_size));
        file.close();

        return buffer;
    }

    auto Shader::create_shader_module(const std::vector<char> &code) -> void {
        VkShaderModuleCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = code.size();
        create_info.pCode = reinterpret_cast<const std::uint32_t *>(code.data());

        if (::vkCreateShaderModule(m_device, &create_info, nullptr, &m_shader_module) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module for: " + m_filepath.string());
        }
    }

    auto Shader::cleanup_shader_module() -> void {
        if (m_shader_module != VK_NULL_HANDLE) {
            ::vkDestroyShaderModule(m_device, m_shader_module, nullptr);
            m_shader_module = VK_NULL_HANDLE;
        }
    }

    auto Shader::stage_to_vk_flags(Stage stage) -> VkShaderStageFlagBits {
        switch (stage) {
            case Stage::Vertex: return VK_SHADER_STAGE_VERTEX_BIT;
            case Stage::Fragment: return VK_SHADER_STAGE_FRAGMENT_BIT;
            case Stage::Compute: return VK_SHADER_STAGE_COMPUTE_BIT;
            case Stage::Geometry: return VK_SHADER_STAGE_GEOMETRY_BIT;
            case Stage::TessellationControl: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            case Stage::TessellationEvaluation: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            default: return VK_SHADER_STAGE_VERTEX_BIT;
        }
    }
} // namespace batleth