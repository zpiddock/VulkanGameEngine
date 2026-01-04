#include "batleth/shader_cache.hpp"
#include "federation/log.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>

namespace batleth {

namespace {
    // Simple hash function for file contents
    auto hash_bytes(const std::string& data) -> std::uint64_t {
        std::uint64_t hash = 0xcbf29ce484222325ULL; // FNV-1a offset basis
        const std::uint64_t prime = 0x100000001b3ULL; // FNV-1a prime

        for (char c : data) {
            hash ^= static_cast<std::uint64_t>(c);
            hash *= prime;
        }

        return hash;
    }

    // Cache file format version
    constexpr std::uint32_t CACHE_VERSION = 1;
    constexpr std::uint32_t CACHE_MAGIC = 0x53505652; // "SPVR" (SPIR-V Cache)
}

ShaderCache::ShaderCache()
    : ShaderCache(Config{}) {
}

ShaderCache::ShaderCache(const Config& config)
    : m_config(config)
    , m_cache_dir(config.cache_directory) {

    // Create cache directory if it doesn't exist
    if (!std::filesystem::exists(m_cache_dir)) {
        try {
            std::filesystem::create_directories(m_cache_dir);
            FED_INFO("Created shader cache directory: {}", m_cache_dir.string());
        } catch (const std::exception& e) {
            FED_ERROR("Failed to create cache directory: {}", e.what());
        }
    }
}

auto ShaderCache::lookup(const std::filesystem::path& source_path) -> std::optional<std::vector<std::uint32_t>> {
    if (!std::filesystem::exists(source_path)) {
        return std::nullopt;
    }

    auto cache_path = get_cache_path(source_path);
    if (!std::filesystem::exists(cache_path)) {
        FED_DEBUG("Cache miss for {}: no cache file", source_path.string());
        return std::nullopt;
    }

    auto entry = load_cache_entry(cache_path);
    if (!entry) {
        FED_DEBUG("Cache miss for {}: failed to load", source_path.string());
        return std::nullopt;
    }

    if (m_config.enable_validation && !is_cache_valid(source_path, *entry)) {
        FED_DEBUG("Cache miss for {}: validation failed", source_path.string());
        return std::nullopt;
    }

    FED_INFO("Cache hit for {}", source_path.string());
    return entry->spirv;
}

auto ShaderCache::store(const std::filesystem::path& source_path,
                        const std::vector<std::uint32_t>& spirv) -> void {
    if (!std::filesystem::exists(source_path)) {
        FED_WARN("Cannot cache shader: source file does not exist: {}", source_path.string());
        return;
    }

    CacheEntry entry;
    entry.spirv = spirv;
    entry.source_hash = compute_source_hash(source_path);
    entry.timestamp = std::filesystem::last_write_time(source_path);

    auto cache_path = get_cache_path(source_path);
    save_cache_entry(cache_path, entry);

    FED_DEBUG("Cached shader: {} -> {}", source_path.string(), cache_path.string());
}

auto ShaderCache::clear() -> void {
    if (!std::filesystem::exists(m_cache_dir)) {
        return;
    }

    try {
        std::uintmax_t removed = 0;
        for (const auto& entry : std::filesystem::directory_iterator(m_cache_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".spvcache") {
                std::filesystem::remove(entry);
                ++removed;
            }
        }
        FED_INFO("Cleared {} cached shaders", removed);
    } catch (const std::exception& e) {
        FED_ERROR("Failed to clear cache: {}", e.what());
    }
}

auto ShaderCache::prune() -> void {
    if (!std::filesystem::exists(m_cache_dir)) {
        return;
    }

    try {
        std::uintmax_t pruned = 0;
        for (const auto& entry : std::filesystem::directory_iterator(m_cache_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".spvcache") {
                // Try to load and validate the entry
                auto cache_entry = load_cache_entry(entry.path());
                if (!cache_entry) {
                    std::filesystem::remove(entry);
                    ++pruned;
                }
            }
        }
        FED_INFO("Pruned {} stale cache entries", pruned);
    } catch (const std::exception& e) {
        FED_ERROR("Failed to prune cache: {}", e.what());
    }
}

auto ShaderCache::get_cache_path(const std::filesystem::path& source_path) -> std::filesystem::path {
    // Create a cache filename based on the source path
    // Use absolute path hash to handle same-named files in different directories
    auto abs_path = std::filesystem::absolute(source_path);
    auto path_str = abs_path.string();
    auto path_hash = hash_bytes(path_str);

    // Format: <original_stem>_<hash>.spvcache
    std::ostringstream filename;
    filename << source_path.stem().string()
             << "_" << std::hex << std::setw(16) << std::setfill('0') << path_hash
             << ".spvcache";

    return m_cache_dir / filename.str();
}

auto ShaderCache::compute_source_hash(const std::filesystem::path& source_path) -> std::uint64_t {
    std::ifstream file(source_path, std::ios::binary);
    if (!file.is_open()) {
        return 0;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return hash_bytes(buffer.str());
}

auto ShaderCache::is_cache_valid(const std::filesystem::path& source_path,
                                 const CacheEntry& entry) -> bool {
    // Check if source file has been modified
    auto current_timestamp = std::filesystem::last_write_time(source_path);
    if (current_timestamp > entry.timestamp) {
        FED_DEBUG("Cache invalid: source file modified");
        return false;
    }

    // Verify source hash matches
    auto current_hash = compute_source_hash(source_path);
    if (current_hash != entry.source_hash) {
        FED_DEBUG("Cache invalid: source hash mismatch");
        return false;
    }

    return true;
}

auto ShaderCache::load_cache_entry(const std::filesystem::path& cache_path) -> std::optional<CacheEntry> {
    std::ifstream file(cache_path, std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }

    // Read header
    std::uint32_t magic, version;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    file.read(reinterpret_cast<char*>(&version), sizeof(version));

    if (magic != CACHE_MAGIC || version != CACHE_VERSION) {
        FED_WARN("Invalid cache file: {}", cache_path.string());
        return std::nullopt;
    }

    CacheEntry entry;

    // Read metadata
    file.read(reinterpret_cast<char*>(&entry.source_hash), sizeof(entry.source_hash));

    std::int64_t timestamp_duration;
    file.read(reinterpret_cast<char*>(&timestamp_duration), sizeof(timestamp_duration));
    entry.timestamp = std::filesystem::file_time_type(
        std::filesystem::file_time_type::duration(timestamp_duration)
    );

    // Read SPIR-V size
    std::uint64_t spirv_size;
    file.read(reinterpret_cast<char*>(&spirv_size), sizeof(spirv_size));

    // Read SPIR-V data
    entry.spirv.resize(spirv_size);
    file.read(reinterpret_cast<char*>(entry.spirv.data()), spirv_size * sizeof(std::uint32_t));

    if (!file) {
        FED_WARN("Failed to read cache file: {}", cache_path.string());
        return std::nullopt;
    }

    return entry;
}

auto ShaderCache::save_cache_entry(const std::filesystem::path& cache_path,
                                   const CacheEntry& entry) -> void {
    std::ofstream file(cache_path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        FED_ERROR("Failed to write cache file: {}", cache_path.string());
        return;
    }

    // Write header
    std::uint32_t magic = CACHE_MAGIC;
    std::uint32_t version = CACHE_VERSION;
    file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));

    // Write metadata
    file.write(reinterpret_cast<const char*>(&entry.source_hash), sizeof(entry.source_hash));

    std::int64_t timestamp_duration = entry.timestamp.time_since_epoch().count();
    file.write(reinterpret_cast<const char*>(&timestamp_duration), sizeof(timestamp_duration));

    // Write SPIR-V size
    std::uint64_t spirv_size = entry.spirv.size();
    file.write(reinterpret_cast<const char*>(&spirv_size), sizeof(spirv_size));

    // Write SPIR-V data
    file.write(reinterpret_cast<const char*>(entry.spirv.data()),
               spirv_size * sizeof(std::uint32_t));

    if (!file) {
        FED_ERROR("Failed to write cache data: {}", cache_path.string());
    }
}

} // namespace batleth
