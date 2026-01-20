#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <fstream>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/optional.hpp>

#ifdef _WIN32
    #ifdef FEDERATION_EXPORTS
        #define FEDERATION_API __declspec(dllexport)
    #else
        #define FEDERATION_API __declspec(dllimport)
    #endif
#else
    #define FEDERATION_API
#endif

namespace federation {

/**
 * ConfigManager - Generic configuration loader/saver with Cereal serialization
 *
 * Usage:
 *   auto config = ConfigManager::load<MyConfig>("config.json");
 *   ConfigManager::save(config, "config.json");
 */
class FEDERATION_API ConfigManager {
public:
    /**
     * Load configuration from JSON file.
     * If file doesn't exist or is invalid, returns default-constructed T.
     * Automatically creates config file with defaults if missing.
     */
    template<typename T>
    static auto load(const std::filesystem::path& filepath) -> T {
        T config{};  // Default construct

        if (!std::filesystem::exists(filepath)) {
            log_info("Config file not found: {}, creating with defaults", filepath.string());
            save(config, filepath);
            return config;
        }

        try {
            std::ifstream file(filepath);
            if (!file.is_open()) {
                log_error("Failed to open config file: {}", filepath.string());
                return config;
            }

            cereal::JSONInputArchive archive(file);
            archive(config);
            log_info("Loaded config from: {}", filepath.string());

        } catch (const std::exception& e) {
            log_error("Failed to parse config file: {} - {}", filepath.string(), e.what());
            log_info("Using default configuration");
        }

        return config;
    }

    /**
     * Save configuration to JSON file with pretty printing.
     */
    template<typename T>
    static auto save(const T& config, const std::filesystem::path& filepath) -> bool {
        try {
            // Create parent directories if they don't exist
            if (auto parent = filepath.parent_path(); !parent.empty()) {
                std::filesystem::create_directories(parent);
            }

            std::ofstream file(filepath);
            if (!file.is_open()) {
                log_error("Failed to create config file: {}", filepath.string());
                return false;
            }

            cereal::JSONOutputArchive archive(file,
                cereal::JSONOutputArchive::Options::Default());
            archive(cereal::make_nvp("config", config));

            log_info("Saved config to: {}", filepath.string());
            return true;

        } catch (const std::exception& e) {
            log_error("Failed to save config file: {} - {}", filepath.string(), e.what());
            return false;
        }
    }

    /**
     * Validate config file exists and is parseable.
     */
    template<typename T>
    static auto validate(const std::filesystem::path& filepath) -> bool {
        if (!std::filesystem::exists(filepath)) {
            return false;
        }

        try {
            T config{};
            std::ifstream file(filepath);
            cereal::JSONInputArchive archive(file);
            archive(config);
            return true;
        } catch (...) {
            return false;
        }
    }

private:
    // Inline logging helpers (will use federation::log)
    template<typename... Args>
    static auto log_info(const char* fmt, Args&&... args) -> void {
        // Implemented in config_manager.cpp
        log_info_impl(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static auto log_error(const char* fmt, Args&&... args) -> void {
        // Implemented in config_manager.cpp
        log_error_impl(fmt, std::forward<Args>(args)...);
    }

    // Implementation functions (defined in .cpp)
    FEDERATION_API static auto log_info_impl(const char* msg) -> void;
    FEDERATION_API static auto log_info_impl(const char* fmt, const std::string& arg) -> void;
    FEDERATION_API static auto log_error_impl(const char* fmt, const std::string& arg) -> void;
    FEDERATION_API static auto log_error_impl(const char* fmt, const std::string& arg1, const char* arg2) -> void;
};

} // namespace federation
