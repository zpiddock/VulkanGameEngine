#pragma once

#include "klingon/scene.hpp"
#include <optional>
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

    class KLINGON_API RayPicker {
    public:
        /**
         * Cast a ray into the scene and find the closest intersected object.
         * @param scene The scene to check against.
         * @param uv Normalized screen coordinates (0..1).
         * @return ID of the closest intersected object, or std::nullopt.
         */
        static auto pick_object(const Scene& scene, const glm::vec2& uv) -> std::optional<GameObject::id_t>;

        /**
         * Cast a ray into the scene and find the closest intersected object.
         * @param scene The scene to check against.
         * @param ray_origin World space ray origin.
         * @param ray_dir World space ray direction (normalized).
         * @return ID of the closest intersected object, or std::nullopt.
         */
        static auto pick_object(const Scene& scene, const glm::vec3& ray_origin, const glm::vec3& ray_dir) -> std::optional<GameObject::id_t>;

    private:
        static auto ray_aabb_intersect(
            const glm::vec3& ray_origin,
            const glm::vec3& ray_dir,
            const glm::vec3& aabb_min,
            const glm::vec3& aabb_max,
            float& t
        ) -> bool;

        static auto ray_sphere_intersect(
            const glm::vec3& ray_origin,
            const glm::vec3& ray_dir,
            const glm::vec3& sphere_center,
            float sphere_radius,
            float& t
        ) -> bool;
    };

} // namespace klingon
