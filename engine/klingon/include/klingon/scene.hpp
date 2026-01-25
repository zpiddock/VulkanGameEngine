#pragma once

#include <filesystem>
#include <string>
#include <memory>
#include <unordered_map>

#include <glm/glm.hpp>

#include "game_object.hpp"
#include "camera.hpp"
#include "transform.hpp"
#include "model/asset_loader.hpp"

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
 * Scene encapsulates all renderable state for a game level or environment.
 * Owns game objects and camera, providing a high-level abstraction for rendering.
 */
    class KLINGON_API Scene {
    public:
        Scene();

        ~Scene();

        Scene(const Scene &) = delete;

        Scene &operator=(const Scene &) = delete;

        Scene(Scene &&) = default;

        Scene &operator=(Scene &&) = default;

        // Camera management (Scene owns the camera)
        auto get_camera() -> Camera &;

        auto get_camera() const -> const Camera &;

        // Camera transform (used by engine to update camera view matrix)
        auto get_camera_transform() -> Transform &;

        auto get_camera_transform() const -> const Transform &;

        // GameObject management (Scene owns game objects)
        auto add_game_object(GameObject &&obj) -> GameObject::id_t;

        auto remove_game_object(GameObject::id_t id) -> bool;

        auto get_game_object(GameObject::id_t id) -> GameObject *;

        auto get_game_objects() -> GameObject::Map &;

        auto get_game_objects() const -> const GameObject::Map &;

        // Lighting configuration
        auto set_ambient_light(const glm::vec4 &color) -> void;

        auto get_ambient_light() const -> const glm::vec4 &;

        // Scene metadata
        auto set_name(const std::string &name) -> void;

        auto get_name() const -> const std::string &;

        /**
         * Reload all model resources from disk
         * Called after deserializing a scene to rebuild GPU resources
         * @param asset_loader Asset loader to use for loading models
         */
        auto reload_all_resources(AssetLoader& asset_loader) -> void;

        template <class Archive>
        void serialize(Archive& ar) {
            ar(
                m_name,
                m_game_objects,
                m_camera,
                m_camera_transform,
                m_ambient_light
            );
        }

    private:
        std::string m_name = "Untitled Scene";
        GameObject::Map m_game_objects;
        std::unique_ptr<Camera> m_camera;
        Transform m_camera_transform;
        glm::vec4 m_ambient_light = {1.f, 1.f, 1.f, 0.02f};
    };
} // namespace klingon