#pragma once

#include <iostream>
#include <string_view>
#include <source_location>
#include <format>

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

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

/**
 * Zero-cost logging framework.
 * When logging is disabled, all calls are optimized away at compile-time.
 */
class FEDERATION_API Logger {
public:
    static auto set_level(LogLevel level) -> void;
    static auto get_level() -> LogLevel;

    // These are called from macros that pass source_location
    template<typename... Args>
    static auto log_trace(const std::source_location& location, const std::string& fmt, const Args&... args) -> void {
        if constexpr (sizeof...(Args) > 0) {
            log(LogLevel::Trace, std::vformat(fmt, std::make_format_args(args...)), location);
        } else {
            log(LogLevel::Trace, fmt, location);
        }
    }

    template<typename... Args>
    static auto log_debug(const std::source_location& location, const std::string& fmt, const Args&... args) -> void {
        if constexpr (sizeof...(Args) > 0) {
            log(LogLevel::Debug, std::vformat(fmt, std::make_format_args(args...)), location);
        } else {
            log(LogLevel::Debug, fmt, location);
        }
    }

    template<typename... Args>
    static auto log_info(const std::source_location& location, const std::string& fmt, const Args&... args) -> void {
        if constexpr (sizeof...(Args) > 0) {
            log(LogLevel::Info, std::vformat(fmt, std::make_format_args(args...)), location);
        } else {
            log(LogLevel::Info, fmt, location);
        }
    }

    template<typename... Args>
    static auto log_warn(const std::source_location& location, const std::string& fmt, const Args&... args) -> void {
        if constexpr (sizeof...(Args) > 0) {
            log(LogLevel::Warn, std::vformat(fmt, std::make_format_args(args...)), location);
        } else {
            log(LogLevel::Warn, fmt, location);
        }
    }

    template<typename... Args>
    static auto log_error(const std::source_location& location, const std::string& fmt, const Args&... args) -> void {
        if constexpr (sizeof...(Args) > 0) {
            log(LogLevel::Error, std::vformat(fmt, std::make_format_args(args...)), location);
        } else {
            log(LogLevel::Error, fmt, location);
        }
    }

    template<typename... Args>
    static auto log_fatal(const std::source_location& location, const std::string& fmt, const Args&... args) -> void {
        if constexpr (sizeof...(Args) > 0) {
            log(LogLevel::Fatal, std::vformat(fmt, std::make_format_args(args...)), location);
        } else {
            log(LogLevel::Fatal, fmt, location);
        }
    }

private:
    static auto log(LogLevel level, const std::string& message,
                   const std::source_location& location) -> void;
    static auto level_to_string(LogLevel level) -> std::string_view;
    static auto level_to_color(LogLevel level) -> std::string_view;

    inline static LogLevel s_current_level = LogLevel::Info;
};

} // namespace federation

// Conditional logging macros for zero-cost when disabled
#ifndef FEDERATION_DISABLE_LOGGING
    #define FED_TRACE(...) ::federation::Logger::log_trace(::std::source_location::current(), __VA_ARGS__)
    #define FED_DEBUG(...) ::federation::Logger::log_debug(::std::source_location::current(), __VA_ARGS__)
    #define FED_INFO(...)  ::federation::Logger::log_info(::std::source_location::current(), __VA_ARGS__)
    #define FED_WARN(...)  ::federation::Logger::log_warn(::std::source_location::current(), __VA_ARGS__)
    #define FED_ERROR(...) ::federation::Logger::log_error(::std::source_location::current(), __VA_ARGS__)
    #define FED_FATAL(...) ::federation::Logger::log_fatal(::std::source_location::current(), __VA_ARGS__)
#else
    // Zero-cost when disabled - these become no-ops that the compiler optimizes away
    #define FED_TRACE(...) ((void)0)
    #define FED_DEBUG(...) ((void)0)
    #define FED_INFO(...)  ((void)0)
    #define FED_WARN(...)  ((void)0)
    #define FED_ERROR(...) ((void)0)
    #define FED_FATAL(...) ((void)0)
#endif
