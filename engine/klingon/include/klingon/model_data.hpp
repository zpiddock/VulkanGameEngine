#pragma once

#include "klingon/model/mesh.h"
#include "klingon/material.hpp"
#include "klingon/transform.hpp"
#include <vector>
#include <memory>
#include <string>
#include <cstdint>

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
    /**
     * Scene graph node for hierarchical transforms
     */
    struct KLINGON_API ModelNode {
        std::string name;
        Transform transform;                    // Local transform
        uint32_t mesh_index = UINT32_MAX;      // Index into ModelData::meshes (UINT32_MAX = no mesh)
        uint32_t material_index = 0;           // Index into ModelData::materials
        std::vector<uint32_t> children;        // Child node indices

        // NO serialize() - ModelNode is runtime-only
    };

    /**
     * Complete model with meshes, materials, and hierarchy
     * Replaces single Mesh in GameObject
     */
    struct KLINGON_API ModelData {
        std::vector<std::shared_ptr<Mesh>> meshes;
        std::vector<Material> materials;
        std::vector<ModelNode> nodes;
        uint32_t root_node_index = 0;

        // Material buffer indices (set when uploaded to GPU)
        uint32_t material_buffer_offset = 0;  // Starting index in global material buffer

        /**
         * Get accumulated world transform matrix for a node (traverses hierarchy)
         * @param node_index Index of the node
         * @param model_root_matrix Root transform matrix of the entire model (from GameObject)
         * @return Combined world transform matrix
         */
        auto get_node_world_matrix(uint32_t node_index, const glm::mat4& model_root_matrix) const -> glm::mat4;

        // NO serialize() - ModelData is runtime-only, loaded from source file
        // The source file path is stored in GameObject::model_filepath
    };
} // namespace klingon
