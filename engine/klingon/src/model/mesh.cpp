#include "klingon/model/mesh.h"
#include "batleth/device.hpp"
#include "federation/log.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <cassert>
#include <cstring>
#include <unordered_map>

#include "klingon/model/asset_loader.hpp"

// Hash function for Vertex
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
    // Vertex binding and attribute descriptions
    auto Vertex::get_binding_descriptions() -> std::vector<VkVertexInputBindingDescription> {
        std::vector<VkVertexInputBindingDescription> binding_descriptions(1);
        binding_descriptions[0].binding = 0;
        binding_descriptions[0].stride = sizeof(Vertex);
        binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return binding_descriptions;
    }

    auto Vertex::get_attribute_descriptions() -> std::vector<VkVertexInputAttributeDescription> {
        std::vector<VkVertexInputAttributeDescription> attribute_descriptions{};

        // Position
        attribute_descriptions.push_back({
            0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)
        });

        // Color
        attribute_descriptions.push_back({
            1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)
        });

        // Normal
        attribute_descriptions.push_back({
            2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)
        });

        // UV
        attribute_descriptions.push_back({
            3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)
        });

        return attribute_descriptions;
    }

    // MeshData implementation
    auto MeshData::load_from_file(const std::string &filepath) -> void {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn;
        std::string err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str())) {
            throw std::runtime_error("Failed to load OBJ file: " + filepath + "\n" + warn + err);
        }

        if (!warn.empty()) {
            FED_WARN("OBJ loader warning: {}", warn);
        }

        vertices.clear();
        indices.clear();

        std::unordered_map<Vertex, uint32_t> unique_vertices{};

        for (const auto &shape: shapes) {
            for (const auto &index: shape.mesh.indices) {
                Vertex vertex{};

                if (index.vertex_index >= 0) {
                    vertex.position = {
                        attrib.vertices[3 * index.vertex_index + 0],
                        attrib.vertices[3 * index.vertex_index + 1],
                        attrib.vertices[3 * index.vertex_index + 2]
                    };

                    // Use vertex colors if available, otherwise default to white
                    if (attrib.colors.size() > 0) {
                        vertex.color = {
                            attrib.colors[3 * index.vertex_index + 0],
                            attrib.colors[3 * index.vertex_index + 1],
                            attrib.colors[3 * index.vertex_index + 2]
                        };
                    }
                }

                if (index.normal_index >= 0) {
                    vertex.normal = {
                        attrib.normals[3 * index.normal_index + 0],
                        attrib.normals[3 * index.normal_index + 1],
                        attrib.normals[3 * index.normal_index + 2]
                    };
                }

                if (index.texcoord_index >= 0) {
                    vertex.uv = {
                        attrib.texcoords[2 * index.texcoord_index + 0],
                        attrib.texcoords[2 * index.texcoord_index + 1]
                    };
                }

                // Deduplicate vertices
                if (!unique_vertices.contains(vertex)) {
                    unique_vertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(unique_vertices[vertex]);
            }
        }

        FED_INFO("Loaded mesh from {}: {} vertices, {} indices", filepath, vertices.size(), indices.size());
    }

    // Mesh implementation
    Mesh::Mesh(batleth::Device &device, const MeshData &mesh_data)
        : m_device(device) {
        create_vertex_buffer(mesh_data.vertices);
        create_index_buffer(mesh_data.indices);

        // Calculate AABB
        if (mesh_data.vertices.empty()) {
            return;
        }

        m_aabb.min = mesh_data.vertices[0].position;
        m_aabb.max = mesh_data.vertices[0].position;

        for (const auto& vertex : mesh_data.vertices) {
            m_aabb.min = glm::min(m_aabb.min, vertex.position);
            m_aabb.max = glm::max(m_aabb.max, vertex.position);
        }
    }

    Mesh::~Mesh() {
        if (m_vertex_buffer != VK_NULL_HANDLE) {
            ::vkDestroyBuffer(m_device.get_logical_device(), m_vertex_buffer, nullptr);
        }
        if (m_vertex_buffer_memory != VK_NULL_HANDLE) {
            ::vkFreeMemory(m_device.get_logical_device(), m_vertex_buffer_memory, nullptr);
        }
        if (m_index_buffer != VK_NULL_HANDLE) {
            ::vkDestroyBuffer(m_device.get_logical_device(), m_index_buffer, nullptr);
        }
        if (m_index_buffer_memory != VK_NULL_HANDLE) {
            ::vkFreeMemory(m_device.get_logical_device(), m_index_buffer_memory, nullptr);
        }
    }

    auto Mesh::bind(VkCommandBuffer command_buffer) -> void {
        VkBuffer buffers[] = {m_vertex_buffer};
        VkDeviceSize offsets[] = {0};
        ::vkCmdBindVertexBuffers(command_buffer, 0, 1, buffers, offsets);

        if (m_has_index_buffer) {
            ::vkCmdBindIndexBuffer(command_buffer, m_index_buffer, 0, VK_INDEX_TYPE_UINT32);
        }
    }

    auto Mesh::draw(VkCommandBuffer command_buffer) -> void {
        if (m_has_index_buffer) {
            ::vkCmdDrawIndexed(command_buffer, m_index_count, 1, 0, 0, 0);
        } else {
            ::vkCmdDraw(command_buffer, m_vertex_count, 1, 0, 0);
        }
    }

    auto Mesh::create_from_file(batleth::Device &device, const std::string &filepath)
        -> std::unique_ptr<Mesh> {
        MeshData data{};
        // data.load_from_file(filepath);
        data = AssetLoader::load_mesh_from_obj(filepath);
        return std::make_unique<Mesh>(device, data);
    }

    auto Mesh::create_vertex_buffer(const std::vector<Vertex> &vertices) -> void {
        m_vertex_count = static_cast<uint32_t>(vertices.size());
        assert(m_vertex_count >= 3 && "Vertex count must be at least 3");

        VkDeviceSize buffer_size = sizeof(vertices[0]) * m_vertex_count;

        // Create staging buffer
        VkBuffer staging_buffer;
        VkDeviceMemory staging_buffer_memory;
        m_device.create_buffer(
            buffer_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            staging_buffer,
            staging_buffer_memory
        );

        // Copy vertex data to staging buffer
        void *data;
        ::vkMapMemory(m_device.get_logical_device(), staging_buffer_memory, 0, buffer_size, 0, &data);
        std::memcpy(data, vertices.data(), static_cast<size_t>(buffer_size));
        ::vkUnmapMemory(m_device.get_logical_device(), staging_buffer_memory);

        // Create device local vertex buffer
        m_device.create_buffer(
            buffer_size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_vertex_buffer,
            m_vertex_buffer_memory
        );

        // Copy from staging to device local buffer
        m_device.copy_buffer(staging_buffer, m_vertex_buffer, buffer_size);

        // Cleanup staging buffer
        ::vkDestroyBuffer(m_device.get_logical_device(), staging_buffer, nullptr);
        ::vkFreeMemory(m_device.get_logical_device(), staging_buffer_memory, nullptr);
    }

    auto Mesh::create_index_buffer(const std::vector<uint32_t> &indices) -> void {
        m_index_count = static_cast<uint32_t>(indices.size());
        m_has_index_buffer = m_index_count > 0;

        if (!m_has_index_buffer) {
            return;
        }

        VkDeviceSize buffer_size = sizeof(indices[0]) * m_index_count;

        // Create staging buffer
        VkBuffer staging_buffer;
        VkDeviceMemory staging_buffer_memory;
        m_device.create_buffer(
            buffer_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            staging_buffer,
            staging_buffer_memory
        );

        // Copy index data to staging buffer
        void *data;
        ::vkMapMemory(m_device.get_logical_device(), staging_buffer_memory, 0, buffer_size, 0, &data);
        std::memcpy(data, indices.data(), static_cast<size_t>(buffer_size));
        ::vkUnmapMemory(m_device.get_logical_device(), staging_buffer_memory);

        // Create device local index buffer
        m_device.create_buffer(
            buffer_size,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_index_buffer,
            m_index_buffer_memory
        );

        // Copy from staging to device local buffer
        m_device.copy_buffer(staging_buffer, m_index_buffer, buffer_size);

        // Cleanup staging buffer
        ::vkDestroyBuffer(m_device.get_logical_device(), staging_buffer, nullptr);
        ::vkFreeMemory(m_device.get_logical_device(), staging_buffer_memory, nullptr);
    }
} // namespace klingon
