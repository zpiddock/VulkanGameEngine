#pragma once
#include "mesh.h"

#ifdef _WIN32
#ifdef KLINGON_EXPORTS
#define KLINGON_API __declspec(dllexport)
#else
#define KLINGON_API __declspec(dllimport)
#endif
#else
#define KLINGON_API
#endif

namespace klingon {
    class KLINGON_API AssetLoader {
    public:
        static auto load_mesh_from_obj(const std::string &filepath) -> MeshData;
    };
} // klingon
