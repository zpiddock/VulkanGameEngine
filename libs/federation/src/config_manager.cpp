#include "federation/config_manager.hpp"
#include "federation/log.hpp"
#include <format>

namespace federation {

auto ConfigManager::log_info_impl(const char* msg) -> void {
    FED_INFO(msg);
}

auto ConfigManager::log_info_impl(const char* fmt, const std::string& arg) -> void {
    FED_INFO(std::vformat(fmt, std::make_format_args(arg)).c_str());
}

auto ConfigManager::log_error_impl(const char* fmt, const std::string& arg) -> void {
    FED_ERROR(std::vformat(fmt, std::make_format_args(arg)).c_str());
}

auto ConfigManager::log_error_impl(const char* fmt, const std::string& arg1, const char* arg2) -> void {
    FED_ERROR(std::vformat(fmt, std::make_format_args(arg1, arg2)).c_str());
}

} // namespace federation
