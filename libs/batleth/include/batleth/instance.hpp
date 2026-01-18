#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

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
 * RAII wrapper for VkInstance.
 * Manages Vulkan instance creation and destruction with modern C++ semantics.
 */
    class BATLETH_API Instance {
    public:
        struct Config {
            std::string application_name = "Vulkan Application";
            std::uint32_t application_version = VK_MAKE_VERSION(1, 0, 0);
            std::string engine_name = "Klingon Engine";
            std::uint32_t engine_version = VK_MAKE_VERSION(0, 1, 0);
            std::vector<const char *> extensions;
            std::vector<const char *> validation_layers;
            bool enable_validation = true;
        };

        explicit Instance(const Config &config);

        ~Instance();

        Instance(const Instance &) = delete;

        Instance &operator=(const Instance &) = delete;

        Instance(Instance &&) noexcept;

        Instance &operator=(Instance &&) noexcept;

        auto get_handle() const -> VkInstance { return m_instance; }
        auto get_validation_enabled() const -> bool { return m_validation_enabled; }

    private:
        VkInstance m_instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_debug_messenger = VK_NULL_HANDLE;
        bool m_validation_enabled = false;

        auto setup_debug_messenger() -> void;
    };
} // namespace batleth