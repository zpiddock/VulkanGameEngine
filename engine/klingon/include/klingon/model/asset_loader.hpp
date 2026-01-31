#pragma once
#include "mesh.h"
#include "klingon/model_data.hpp"
#include "klingon/texture_manager.hpp"
#include "batleth/device.hpp"
#include <string>
#include <memory>

#include "assimp/LogStream.hpp"
#include "assimp/Importer.hpp"
#include "assimp/Logger.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include "federation/log.hpp"

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
        struct Config {
            batleth::Device& device;
            TextureManager& texture_manager;
            std::string base_texture_path = "assets/textures/";
        };

        explicit AssetLoader(const Config& config);
        ~AssetLoader() = default;

        // Delete copy
        AssetLoader(const AssetLoader&) = delete;
        auto operator=(const AssetLoader&) -> AssetLoader& = delete;

        /**
         * Legacy method - loads just mesh data
         * @deprecated Use load_model() instead
         */
        static auto load_mesh_from_obj(const std::string& filepath) -> MeshData;

        /**
         * Load complete ModelData with materials, textures, and hierarchy
         * Supports OBJ, FBX, glTF via Assimp
         * @param filepath Path to model file
         * @return Complete ModelData with uploaded materials
         */
        auto load_model(const std::string& filepath) -> std::shared_ptr<ModelData>;

    private:
        auto process_assimp_scene(const aiScene* scene, const std::string& model_dir) -> std::shared_ptr<ModelData>;
        auto process_mesh(const aiMesh* assimp_mesh) -> MeshData;
        auto process_material(const aiMaterial* assimp_material, const std::string& model_dir) -> Material;
        auto process_node(const aiScene* scene, const aiNode* node, ModelData& model_data, uint32_t parent_index) -> uint32_t;

        batleth::Device& m_device;
        TextureManager& m_texture_manager;
        std::string m_base_texture_path;
    };

    class KLINGON_API AssetLoaderLogStream : public Assimp::LogStream {

        public:
        explicit AssetLoaderLogStream(Assimp::Logger::ErrorSeverity severity) : m_severity(severity) {}

        auto write(const char* message) -> void override {
            std::string msg(message);
            if (!msg.empty() && msg.back() == '\n') msg.pop_back();

            switch (m_severity) {
                case Assimp::Logger::ErrorSeverity::Err:
                    FED_ERROR("[Assimp] {}", message);
                    break;
                case Assimp::Logger::ErrorSeverity::Warn:
                    FED_WARN("[Assimp] {}", message);
                    break;
                case Assimp::Logger::ErrorSeverity::Info:
                    FED_INFO("[Assimp] {}", message);
                    break;
                case Assimp::Logger::ErrorSeverity::Debugging:
                    FED_DEBUG("[Assimp] {}", message);
                    break;
            }

        }
    private:
        Assimp::Logger::ErrorSeverity m_severity;
    };
} // klingon
