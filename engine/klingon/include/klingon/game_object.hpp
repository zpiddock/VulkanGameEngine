#pragma once

#include "klingon/transform.hpp"
#include "klingon/model/mesh.h"
#include <memory>
#include <unordered_map>

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

namespace klingon {

/**
 * Point light component
 */
struct PointLightComponent {
    float light_intensity = 1.0f;
};

/**
 * Game object representing an entity in the scene
 * Can have a mesh, transform, color, and optional point light
 */
class KLINGON_API GameObject {
public:
    using id_t = unsigned int;
    using Map = std::unordered_map<id_t, GameObject>;

    /**
     * Create a new game object with unique ID
     */
    static auto create_game_object() -> GameObject;

    /**
     * Create a point light game object
     * @param intensity Light intensity
     * @param radius Light radius (visual representation)
     * @param color Light color
     */
    static auto create_point_light(
        float intensity = 10.f,
        float radius = 0.05f,
        glm::vec3 color = glm::vec3(1.f)
    ) -> GameObject;

    GameObject(const GameObject&) = delete;
    GameObject& operator=(const GameObject&) = delete;
    GameObject(GameObject&&) = default;
    GameObject& operator=(GameObject&&) = default;

    auto get_id() const -> id_t { return m_id; }

    // Public members for easy access
    glm::vec3 color{1.f, 1.f, 1.f};
    Transform transform{};
    std::shared_ptr<Mesh> model{};
    std::unique_ptr<PointLightComponent> point_light = nullptr;

private:
    GameObject(id_t obj_id) : m_id(obj_id) {}

    id_t m_id;
};

} // namespace klingon
