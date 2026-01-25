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
#include <filesystem>

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

    // New ModelData-based loader

    AssetLoader::AssetLoader(const Config& config)
        : m_device(config.device)
        , m_texture_manager(config.texture_manager)
        , m_base_texture_path(config.base_texture_path) {
        FED_INFO("AssetLoader initialized (base_texture_path: {})", m_base_texture_path);
    }

    auto AssetLoader::load_model(const std::string& filepath) -> std::shared_ptr<ModelData> {
        FED_INFO("Loading model: {}", filepath);

        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(filepath,
            aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices |
            aiProcess_FlipUVs |
            aiProcess_GenNormals |
            aiProcess_CalcTangentSpace |
            aiProcess_GenUVCoords
        );

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            FED_ERROR("Failed to load model: {} - {}", filepath, importer.GetErrorString());
            return nullptr;
        }

        std::filesystem::path path(filepath);
        std::string model_dir = path.parent_path().string();

        auto model_data = process_assimp_scene(scene, model_dir);

        if (model_data) {
            FED_INFO("Successfully loaded model: {} ({} meshes, {} materials, {} nodes)",
                     filepath, model_data->meshes.size(), model_data->materials.size(), model_data->nodes.size());
        }

        return model_data;
    }

    auto AssetLoader::process_assimp_scene(const aiScene* scene, const std::string& model_dir) -> std::shared_ptr<ModelData> {
        auto model_data = std::make_shared<ModelData>();

        // Process materials first
        FED_TRACE("Processing {} materials", scene->mNumMaterials);
        for (uint32_t i = 0; i < scene->mNumMaterials; ++i) {
            model_data->materials.push_back(process_material(scene->mMaterials[i], model_dir));
        }

        // Add default material if none exist
        if (model_data->materials.empty()) {
            FED_WARN("No materials found, adding default material");
            model_data->materials.emplace_back();
        }

        // Process meshes
        FED_TRACE("Processing {} meshes", scene->mNumMeshes);
        for (uint32_t i = 0; i < scene->mNumMeshes; ++i) {
            MeshData mesh_data = process_mesh(scene->mMeshes[i]);
            auto mesh = std::make_shared<Mesh>(m_device, mesh_data);
            model_data->meshes.push_back(mesh);
        }

        // Process node hierarchy
        FED_TRACE("Processing node hierarchy");
        model_data->root_node_index = process_node(scene, scene->mRootNode, *model_data, UINT32_MAX);

        // Upload materials to GPU
        FED_TRACE("Uploading {} materials to GPU", model_data->materials.size());
        std::vector<MaterialGPU> gpu_materials;
        for (const auto& mat : model_data->materials) {
            gpu_materials.push_back(mat.gpu_data);
        }
        model_data->material_buffer_offset = m_texture_manager.upload_materials(gpu_materials);

        FED_TRACE("Materials uploaded starting at index {}", model_data->material_buffer_offset);

        return model_data;
    }

    auto AssetLoader::process_mesh(const aiMesh* assimp_mesh) -> MeshData {
        MeshData mesh_data;
        std::unordered_map<Vertex, uint32_t> unique_verts;

        for (uint32_t face_idx = 0; face_idx < assimp_mesh->mNumFaces; ++face_idx) {
            const aiFace& face = assimp_mesh->mFaces[face_idx];

            for (uint32_t vert_idx = 0; vert_idx < face.mNumIndices; ++vert_idx) {
                uint32_t index = face.mIndices[vert_idx];
                Vertex vertex{};

                // Position
                vertex.position = {
                    assimp_mesh->mVertices[index].x,
                    assimp_mesh->mVertices[index].y,
                    assimp_mesh->mVertices[index].z
                };

                // Normal
                if (assimp_mesh->HasNormals()) {
                    vertex.normal = {
                        assimp_mesh->mNormals[index].x,
                        assimp_mesh->mNormals[index].y,
                        assimp_mesh->mNormals[index].z
                    };
                }

                // Vertex color (or default to white)
                if (assimp_mesh->HasVertexColors(0)) {
                    vertex.color = {
                        assimp_mesh->mColors[0][index].r,
                        assimp_mesh->mColors[0][index].g,
                        assimp_mesh->mColors[0][index].b
                    };
                } else {
                    vertex.color = {1.0f, 1.0f, 1.0f};
                }

                // UV coordinates
                if (assimp_mesh->HasTextureCoords(0)) {
                    vertex.uv = {
                        assimp_mesh->mTextureCoords[0][index].x,
                        assimp_mesh->mTextureCoords[0][index].y
                    };
                }

                // Deduplicate vertices
                if (!unique_verts.contains(vertex)) {
                    unique_verts[vertex] = static_cast<uint32_t>(mesh_data.vertices.size());
                    mesh_data.vertices.push_back(vertex);
                }
                mesh_data.indices.push_back(unique_verts[vertex]);
            }
        }

        FED_TRACE("Processed mesh: {} vertices, {} indices", mesh_data.vertices.size(), mesh_data.indices.size());
        return mesh_data;
    }

    auto AssetLoader::process_material(const aiMaterial* assimp_material, const std::string& model_dir) -> Material {
        Material material;

        // Base color
        aiColor4D base_color;
        if (assimp_material->Get(AI_MATKEY_COLOR_DIFFUSE, base_color) == AI_SUCCESS) {
            material.gpu_data.base_color_factor = {base_color.r, base_color.g, base_color.b, base_color.a};
        }

        // PBR properties (if available)
        float metallic = 1.0f;
        float roughness = 0.5f;
        assimp_material->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
        assimp_material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
        material.gpu_data.metallic_factor = metallic;
        material.gpu_data.roughness_factor = roughness;

        // Load albedo/diffuse texture
        aiString texture_path;
        if (assimp_material->GetTexture(aiTextureType_DIFFUSE, 0, &texture_path) == AI_SUCCESS) {
            std::filesystem::path full_path = std::filesystem::path(model_dir) / texture_path.C_Str();
            material.albedo_texture_path = full_path.string();

            uint32_t tex_index = m_texture_manager.load_texture(
                material.albedo_texture_path,
                batleth::TextureType::Albedo,
                true  // generate mipmaps
            );

            material.gpu_data.albedo_texture_index = tex_index;
            material.set_has_albedo(true);
            FED_TRACE("Loaded albedo texture: {} (index {})", material.albedo_texture_path, tex_index);
        }

        // Load normal map
        if (assimp_material->GetTexture(aiTextureType_NORMALS, 0, &texture_path) == AI_SUCCESS) {
            std::filesystem::path full_path = std::filesystem::path(model_dir) / texture_path.C_Str();
            material.normal_texture_path = full_path.string();

            uint32_t tex_index = m_texture_manager.load_texture(
                material.normal_texture_path,
                batleth::TextureType::Normal,
                true
            );

            material.gpu_data.normal_texture_index = tex_index;
            material.set_has_normal(true);
            FED_TRACE("Loaded normal texture: {} (index {})", material.normal_texture_path, tex_index);
        }

        // Load metallic/roughness texture (typically combined in one texture)
        if (assimp_material->GetTexture(aiTextureType_UNKNOWN, 0, &texture_path) == AI_SUCCESS ||
            assimp_material->GetTexture(aiTextureType_METALNESS, 0, &texture_path) == AI_SUCCESS) {
            std::filesystem::path full_path = std::filesystem::path(model_dir) / texture_path.C_Str();
            material.pbr_texture_path = full_path.string();

            uint32_t tex_index = m_texture_manager.load_texture(
                material.pbr_texture_path,
                batleth::TextureType::MetallicRoughness,
                true
            );

            material.gpu_data.pbr_texture_index = tex_index;
            material.set_has_pbr(true);
            FED_TRACE("Loaded PBR texture: {} (index {})", material.pbr_texture_path, tex_index);
        }

        return material;
    }

    auto AssetLoader::process_node(const aiScene* scene, const aiNode* node, ModelData& model_data, uint32_t parent_index) -> uint32_t {
        ModelNode model_node;
        model_node.name = node->mName.C_Str();

        // Extract transform from Assimp node
        const aiMatrix4x4& ai_transform = node->mTransformation;
        aiVector3D position, scaling;
        aiQuaternion rotation;
        ai_transform.Decompose(scaling, rotation, position);

        model_node.transform.translation = {position.x, position.y, position.z};
        model_node.transform.scale = {scaling.x, scaling.y, scaling.z};

        // Convert quaternion to Euler angles (simplified - might need refinement)
        float pitch = std::atan2(2.0f * (rotation.y * rotation.z + rotation.w * rotation.x),
                                 rotation.w * rotation.w - rotation.x * rotation.x - rotation.y * rotation.y + rotation.z * rotation.z);
        float yaw = std::asin(-2.0f * (rotation.x * rotation.z - rotation.w * rotation.y));
        float roll = std::atan2(2.0f * (rotation.x * rotation.y + rotation.w * rotation.z),
                                rotation.w * rotation.w + rotation.x * rotation.x - rotation.y * rotation.y - rotation.z * rotation.z);

        model_node.transform.rotation = {pitch, yaw, roll};

        // Assign mesh (if node has a mesh)
        if (node->mNumMeshes > 0) {
            // For simplicity, use first mesh index
            // In production, you might want to handle multiple meshes per node differently
            uint32_t mesh_index = node->mMeshes[0];
            model_node.mesh_index = mesh_index;

            // Get material index from mesh
            const aiMesh* mesh = scene->mMeshes[mesh_index];
            model_node.material_index = mesh->mMaterialIndex;
        }

        // Add node to model data
        uint32_t node_index = static_cast<uint32_t>(model_data.nodes.size());
        model_data.nodes.push_back(model_node);

        // Process children recursively
        for (uint32_t i = 0; i < node->mNumChildren; ++i) {
            uint32_t child_index = process_node(scene, node->mChildren[i], model_data, node_index);
            model_data.nodes[node_index].children.push_back(child_index);
        }

        return node_index;
    }
} // klingon
