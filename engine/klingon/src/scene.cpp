#include "klingon/scene.hpp"
#include "federation/log.hpp"

namespace klingon {
    Scene::Scene() {
        // Create camera automatically
        m_camera = std::make_unique<Camera>();

        // Initialize camera transform with default position
        m_camera_transform.translation = {0.f, 0.f, -2.5f}; // Start camera 2.5 units back
        m_camera_transform.rotation = {0.f, 0.f, 0.f};
        m_camera_transform.scale = {1.f, 1.f, 1.f};

        FED_DEBUG("Scene created: {}", m_name);
    }

    Scene::~Scene() {
        FED_DEBUG("Scene destroyed: {}", m_name);
    }

    auto Scene::get_camera() -> Camera & {
        return *m_camera;
    }

    auto Scene::get_camera() const -> const Camera & {
        return *m_camera;
    }

    auto Scene::get_camera_transform() -> Transform & {
        return m_camera_transform;
    }

    auto Scene::get_camera_transform() const -> const Transform & {
        return m_camera_transform;
    }

    auto Scene::add_game_object(GameObject &&obj) -> GameObject::id_t {
        auto id = obj.get_id();
        m_game_objects.emplace(id, std::move(obj));
        FED_DEBUG("Added game object {} to scene '{}'", id, m_name);
        return id;
    }

    auto Scene::remove_game_object(GameObject::id_t id) -> bool {
        auto it = m_game_objects.find(id);
        if (it != m_game_objects.end()) {
            m_game_objects.erase(it);
            FED_DEBUG("Removed game object {} from scene '{}'", id, m_name);
            return true;
        }
        FED_WARN("Attempted to remove non-existent game object {} from scene '{}'", id, m_name);
        return false;
    }

    auto Scene::get_game_object(GameObject::id_t id) -> GameObject * {
        auto it = m_game_objects.find(id);
        if (it != m_game_objects.end()) {
            return &it->second;
        }
        return nullptr;
    }

    auto Scene::get_game_objects() -> GameObject::Map & {
        return m_game_objects;
    }

    auto Scene::get_game_objects() const -> const GameObject::Map & {
        return m_game_objects;
    }

    auto Scene::set_ambient_light(const glm::vec4 &color) -> void {
        m_ambient_light = color;
    }

    auto Scene::get_ambient_light() const -> const glm::vec4 & {
        return m_ambient_light;
    }

    auto Scene::set_name(const std::string &name) -> void {
        m_name = name;
        FED_DEBUG("Scene renamed to '{}'", m_name);
    }

    auto Scene::get_name() const -> const std::string & {
        return m_name;
    }
} // namespace klingon
