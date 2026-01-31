#pragma once

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

    class FEDERATION_API VirtualFileSystem {

    public:
        VirtualFileSystem() = default;
        ~VirtualFileSystem() = default;

        VirtualFileSystem(const VirtualFileSystem&) = delete;
        VirtualFileSystem& operator=(const VirtualFileSystem&) = delete;



    private:

    };
} // namespace federation