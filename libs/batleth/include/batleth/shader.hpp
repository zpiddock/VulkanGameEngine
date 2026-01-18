#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <filesystem>
#include <functional>

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
 * RAII wrapper for VkShaderModule with hot-reload support.
 * Monitors shader files for changes and automatically reloads them.
 */
    class BATLETH_API Shader {
    public:
        enum class Stage {
            Vertex,
            Fragment,
            Compute,
            Geometry,
            TessellationControl,
            TessellationEvaluation
        };

        using ReloadCallback = std::function<void()>;

        struct Config {
            VkDevice device = VK_NULL_HANDLE;
            std::filesystem::path filepath; // Can be .glsl or .spv
            Stage stage = Stage::Vertex;
            bool enable_hot_reload = true;
            bool optimize = true; // Optimize GLSL during compilation
        };

        explicit Shader(const Config &config);

        ~Shader();

        Shader(const Shader &) = delete;

        Shader &operator=(const Shader &) = delete;

        Shader(Shader &&) noexcept;

        Shader &operator=(Shader &&) noexcept;

        auto get_module() const -> VkShaderModule { return m_shader_module; }
        auto get_stage() const -> VkShaderStageFlagBits { return stage_to_vk_flags(m_stage); }
        auto get_filepath() const -> const std::filesystem::path & { return m_filepath; }

        /**
     * Manually reload the shader from disk.
     * Returns true if reload was successful.
     */
        auto reload() -> bool;

        /**
     * Check if the shader file has been modified and reload if necessary.
     * Returns true if shader was reloaded.
     */
        auto check_and_reload() -> bool;

        /**
     * Set a callback to be invoked when the shader is reloaded.
     * Useful for recreating pipelines that use this shader.
     */
        auto set_reload_callback(ReloadCallback callback) -> void { m_reload_callback = std::move(callback); }

    private:
        auto load_shader_code() -> std::vector<char>;

        auto create_shader_module(const std::vector<char> &code) -> void;

        auto cleanup_shader_module() -> void;

        static auto stage_to_vk_flags(Stage stage) -> VkShaderStageFlagBits;

        VkDevice m_device = VK_NULL_HANDLE;
        VkShaderModule m_shader_module = VK_NULL_HANDLE;
        std::filesystem::path m_filepath;
        Stage m_stage;
        bool m_hot_reload_enabled;
        std::filesystem::file_time_type m_last_write_time;
        ReloadCallback m_reload_callback;
    };
} // namespace batleth