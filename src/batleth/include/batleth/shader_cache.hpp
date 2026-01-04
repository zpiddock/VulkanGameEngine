#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>
#include <unordered_map>
#include <optional>

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
 * Disk-based cache for compiled SPIR-V shaders.
 * Stores compiled shaders with metadata (source hash, timestamp) to avoid recompilation.
 */
class BATLETH_API ShaderCache {
public:
    struct CacheEntry {
        std::vector<std::uint32_t> spirv;
        std::uint64_t source_hash;
        std::filesystem::file_time_type timestamp;
    };

    struct Config {
        std::filesystem::path cache_directory = "shader_cache";
        bool enable_validation = true;  // Validate cache entries
        bool enable_compression = false; // Future: compress cached SPIR-V
    };

    ShaderCache();
    explicit ShaderCache(const Config& config);
    ~ShaderCache() = default;

    ShaderCache(const ShaderCache&) = delete;
    ShaderCache& operator=(const ShaderCache&) = delete;
    ShaderCache(ShaderCache&&) = default;
    ShaderCache& operator=(ShaderCache&&) = default;

    /**
     * Look up a cached SPIR-V module.
     * @param source_path Path to the original GLSL source file
     * @return Cached SPIR-V if found and valid, nullopt otherwise
     */
    auto lookup(const std::filesystem::path& source_path) -> std::optional<std::vector<std::uint32_t>>;

    /**
     * Store a compiled SPIR-V module in the cache.
     * @param source_path Path to the original GLSL source file
     * @param spirv Compiled SPIR-V bytecode
     */
    auto store(const std::filesystem::path& source_path,
               const std::vector<std::uint32_t>& spirv) -> void;

    /**
     * Clear all cached entries.
     */
    auto clear() -> void;

    /**
     * Remove stale cache entries (where source file no longer exists).
     */
    auto prune() -> void;

private:
    auto get_cache_path(const std::filesystem::path& source_path) -> std::filesystem::path;
    auto compute_source_hash(const std::filesystem::path& source_path) -> std::uint64_t;
    auto is_cache_valid(const std::filesystem::path& source_path,
                        const CacheEntry& entry) -> bool;
    auto load_cache_entry(const std::filesystem::path& cache_path) -> std::optional<CacheEntry>;
    auto save_cache_entry(const std::filesystem::path& cache_path,
                          const CacheEntry& entry) -> void;

    Config m_config;
    std::filesystem::path m_cache_dir;
};

} // namespace batleth
