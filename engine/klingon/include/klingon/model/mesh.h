#pragma once

#include <vector>
#include <string>
#include <memory>
#include <vulkan/vulkan_core.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#ifdef _WIN32
#ifdef KLINGON_EXPORTS
#define KLINGON_API __declspec(dllexport)
#else
#define KLINGON_API __declspec(dllimport)
#endif
#else
#define KLINGON_API
#endif

// Forward declarations
namespace batleth {
    class Device;
    class CommandBuffer;
}

namespace klingon {
    /**
     * Vertex structure for mesh data
     */
    struct Vertex {
        glm::vec3 position{};
        glm::vec3 color{1.f, 1.f, 1.f};
        glm::vec3 normal{};
        glm::vec2 uv{};

        static auto get_binding_descriptions() -> std::vector<VkVertexInputBindingDescription>;

        static auto get_attribute_descriptions() -> std::vector<VkVertexInputAttributeDescription>;

        bool operator==(const Vertex &other) const {
            return position == other.position
                   && normal == other.normal
                   && uv == other.uv
                   && color == other.color;
        }
    };

    /**
     * Mesh data container
     */
    struct MeshData {
        std::vector<Vertex> vertices{};
        std::vector<uint32_t> indices{};

        /**
         * Load mesh data from OBJ file using tinyobjloader
         * @param filepath Path to the .obj file
         */
        auto load_from_file(const std::string &filepath) -> void;
    };

    /**
     * GPU mesh representation with vertex and index buffers
     */
    class KLINGON_API Mesh {
    public:
        Mesh(batleth::Device &device, const MeshData &mesh_data);

        ~Mesh();

        Mesh(const Mesh &) = delete;

        Mesh &operator=(const Mesh &) = delete;

        Mesh(Mesh &&) = default;

        Mesh &operator=(Mesh &&) = default;

        /**
         * Bind the mesh buffers to a command buffer
         */
        auto bind(VkCommandBuffer command_buffer) -> void;

        /**
         * Draw the mesh
         */
        auto draw(VkCommandBuffer command_buffer) -> void;

        /**
         * Create a mesh from an OBJ file
         * @param device Vulkan device
         * @param filepath Path to the .obj file
         * @return Unique pointer to the created mesh
         */
        static auto create_from_file(batleth::Device &device, const std::string &filepath)
            -> std::unique_ptr<Mesh>;

    private:
        auto create_vertex_buffer(const std::vector<Vertex> &vertices) -> void;

        auto create_index_buffer(const std::vector<uint32_t> &indices) -> void;

        batleth::Device &m_device;

        VkBuffer m_vertex_buffer = VK_NULL_HANDLE;
        VkDeviceMemory m_vertex_buffer_memory = VK_NULL_HANDLE;
        uint32_t m_vertex_count = 0;

        bool m_has_index_buffer = false;
        VkBuffer m_index_buffer = VK_NULL_HANDLE;
        VkDeviceMemory m_index_buffer_memory = VK_NULL_HANDLE;
        uint32_t m_index_count = 0;
    };
} // namespace klingon
