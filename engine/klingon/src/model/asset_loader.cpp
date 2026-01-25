//
// Created by Admin on 18/01/2026.
//


#define GLM_ENABLE_EXPERIMENTAL
#include "klingon/model/asset_loader.hpp"

#include <glm/gtx/hash.hpp>

#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include "federation/log.hpp"
#include "klingon/model/mesh.h"

namespace std {
    template<>
    struct hash<klingon::Vertex> {
        size_t operator()(const klingon::Vertex &vertex) const noexcept {
            size_t h1 = hash<glm::vec3>()(vertex.position);
            size_t h2 = hash<glm::vec3>()(vertex.color);
            size_t h3 = hash<glm::vec3>()(vertex.normal);
            size_t h4 = hash<glm::vec2>()(vertex.uv);

            // Combine hashes
            size_t seed = 0;
            seed ^= h1 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h3 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h4 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };
}

namespace klingon {
    auto AssetLoader::load_mesh_from_obj(const std::string &filepath) -> MeshData {
        MeshData data{};

        Assimp::Importer importer;
        const auto scene = importer.ReadFile(filepath,
                                             ::aiProcess_Triangulate |
                                             ::aiProcess_JoinIdenticalVertices |
                                             ::aiProcess_FlipUVs |
                                             ::aiProcess_GenNormals
        );

        if (scene == nullptr) {
            FED_INFO("Failed to import asset {}", filepath);
            FED_DEBUG("Error: {}", importer.GetErrorString());
            return data;
        }

        if (!scene->HasMeshes()) {
            FED_WARN("No meshes in asset {}", filepath);
            return data;
        }

        // TODO: Parse meshes
        if (!scene->HasMeshes()) {
            FED_WARN("No meshes in asset {}", filepath);
            return data;
        }

        std::unordered_map<Vertex, uint32_t> unique_verts{};

        for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
            const auto mesh = scene->mMeshes[meshIndex];

            for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
                const auto face = mesh->mFaces[faceIndex];

                for (uint32_t vertIndex = 0; vertIndex < face.mNumIndices; ++vertIndex) {
                    uint32_t index = face.mIndices[vertIndex];
                    Vertex vertex{};

                    vertex.position = {
                        mesh->mVertices[index].x,
                        mesh->mVertices[index].y,
                        mesh->mVertices[index].z
                    };

                    if (mesh->HasNormals()) {
                        vertex.normal = {
                            mesh->mNormals[index].x,
                            mesh->mNormals[index].y,
                            mesh->mNormals[index].z
                        };
                    }

                    if (mesh->HasVertexColors(0)) {
                        vertex.color = {
                            mesh->mColors[0][index].r,
                            mesh->mColors[0][index].g,
                            mesh->mColors[0][index].b
                        };
                    }
                    if (mesh->HasTextureCoords(0)) {
                        vertex.uv = {
                            mesh->mTextureCoords[0][index].x,
                            mesh->mTextureCoords[0][index].y
                        };
                    }

                    if (!unique_verts.contains(vertex)) {
                        unique_verts[vertex] =
                                static_cast<uint32_t>(data.vertices.size());
                        data.vertices.push_back(vertex);
                    }
                    data.indices.push_back(unique_verts[vertex]);
                }
            }
        }

        FED_DEBUG("Loaded mesh from {}: {} vertices, {} indices", filepath, data.vertices.size(), data.indices.size());

        return data;
    }
} // klingon
