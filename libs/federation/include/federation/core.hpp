#pragma once

// DLL export/import macros for Windows
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
 * Core initialization and shutdown for the Federation library.
 * This provides foundational utilities for the engine.
 */
    class FEDERATION_API Core {
    public:
        Core() = default;

        ~Core() = default;

        Core(const Core &) = delete;

        Core &operator=(const Core &) = delete;

        Core(Core &&) = default;

        Core &operator=(Core &&) = default;

        auto initialize() -> bool;

        auto shutdown() -> void;
    };
} // namespace federation