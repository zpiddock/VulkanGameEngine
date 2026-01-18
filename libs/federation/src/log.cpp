#include "federation/log.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace federation {
    namespace {
        // Enable ANSI colors on Windows once
        bool enable_windows_ansi_colors() {
#ifdef _WIN32
            auto h_out = ::GetStdHandle(STD_OUTPUT_HANDLE);
            if (h_out == INVALID_HANDLE_VALUE) {
                return false;
            }

            DWORD dw_mode = 0;
            if (!::GetConsoleMode(h_out, &dw_mode)) {
                return false;
            }

            dw_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            if (!::SetConsoleMode(h_out, dw_mode)) {
                return false;
            }
            return true;
#else
            return true;
#endif
        }

        bool colors_enabled = enable_windows_ansi_colors();
    }

    auto Logger::set_level(LogLevel level) -> void {
        s_current_level = level;
    }

    auto Logger::get_level() -> LogLevel {
        return s_current_level;
    }

    auto Logger::log(LogLevel level, const std::string &message,
                     const std::source_location &location) -> void {
        if (level < s_current_level) {
            return;
        }

        // Get current time
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) % 1000;

        std::tm tm_buf;
#ifdef _WIN32
        ::localtime_s(&tm_buf, &time_t);
#else
        ::localtime_r(&time_t, &tm_buf);
#endif

        // Format: [TIMESTAMP] [LEVEL] [file:line] message
        std::ostringstream oss;
        oss << "[" << std::put_time(&tm_buf, "%H:%M:%S") << "."
                << std::setfill('0') << std::setw(3) << ms.count() << "] ";

        // Add color for terminal output if enabled
        if (colors_enabled) {
            std::cout << level_to_color(level);
        }

        oss << "[" << level_to_string(level) << "] ";
        oss << "[" << location.file_name() << ":" << location.line() << "] ";
        oss << message;

        std::cout << oss.str();

        // Reset color if enabled
        if (colors_enabled) {
            std::cout << "\033[0m";
        }

        std::cout << std::endl;

        // Fatal errors should terminate
        if (level == LogLevel::Fatal) {
            std::exit(EXIT_FAILURE);
        }
    }

    auto Logger::level_to_string(LogLevel level) -> std::string_view {
        switch (level) {
            case LogLevel::Trace: return "TRACE";
            case LogLevel::Debug: return "DEBUG";
            case LogLevel::Info: return "INFO";
            case LogLevel::Warn: return "WARN";
            case LogLevel::Error: return "ERROR";
            case LogLevel::Fatal: return "FATAL";
            default: return "UNKNOWN";
        }
    }

    auto Logger::level_to_color(LogLevel level) -> std::string_view {
        switch (level) {
            case LogLevel::Trace: return "\033[37m"; // White
            case LogLevel::Debug: return "\033[36m"; // Cyan
            case LogLevel::Info: return "\033[32m"; // Green
            case LogLevel::Warn: return "\033[33m"; // Yellow
            case LogLevel::Error: return "\033[31m"; // Red
            case LogLevel::Fatal: return "\033[35;1m"; // Bold Magenta
            default: return "\033[0m";
        }
    }
} // namespace federation