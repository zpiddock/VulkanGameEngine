#include "klingon/picking/ray_picker.hpp"
#include "klingon/scene.hpp"
#include "klingon/game_object.hpp"
#include "klingon/model/mesh.h"
#include <limits>

namespace klingon {

    auto RayPicker::pick_object(const Scene& scene, const glm::vec2& uv) -> std::optional<GameObject::id_t> {
        auto ray_dir = scene.get_camera().get_ray_direction(uv);
        auto ray_origin = scene.get_camera().get_position();
        return pick_object(scene, ray_origin, ray_dir);
    }

    auto RayPicker::pick_object(const Scene& scene, const glm::vec3& ray_origin, const glm::vec3& ray_dir) -> std::optional<GameObject::id_t> {
        float min_dist = std::numeric_limits<float>::max();
        std::optional<GameObject::id_t> closest_obj;

        for (const auto& [id, obj] : scene.get_game_objects()) {
            float t = 0.0f;

            if (obj.model_data) {
                for (const auto& mesh : obj.model_data->meshes) {
                    // AABB intersection for models
                    glm::mat4 model_matrix = obj.transform.mat4();
                    glm::mat4 inv_model_matrix = glm::inverse(model_matrix);

                    glm::vec3 local_ray_origin = glm::vec3(inv_model_matrix * glm::vec4(ray_origin, 1.0f));
                    glm::vec3 local_ray_dir = glm::normalize(glm::vec3(inv_model_matrix * glm::vec4(ray_dir, 0.0f)));

                    const auto& aabb = mesh->get_aabb();
                    if (ray_aabb_intersect(local_ray_origin, local_ray_dir, aabb.min, aabb.max, t)) {
                        glm::vec3 local_hit_point = local_ray_origin + local_ray_dir * t;
                        glm::vec3 world_hit_point = glm::vec3(model_matrix * glm::vec4(local_hit_point, 1.0f));
                        float dist = glm::distance(ray_origin, world_hit_point);
                        if (dist < min_dist) {
                            min_dist = dist;
                            closest_obj = id;
                        }
                    }
                }
            } else if (obj.point_light) {
                // Sphere intersection for point lights
                float radius = 0.1f; // Visual radius of point light
                if (ray_sphere_intersect(ray_origin, ray_dir, obj.transform.translation, radius, t)) {
                    if (t < min_dist) {
                        min_dist = t;
                        closest_obj = id;
                    }
                }
            }
        }

        return closest_obj;
    }

    auto RayPicker::ray_aabb_intersect(
        const glm::vec3& ray_origin,
        const glm::vec3& ray_dir,
        const glm::vec3& aabb_min,
        const glm::vec3& aabb_max,
        float& t
    ) -> bool {
        glm::vec3 inv_dir = 1.0f / ray_dir;
        glm::vec3 t1 = (aabb_min - ray_origin) * inv_dir;
        glm::vec3 t2 = (aabb_max - ray_origin) * inv_dir;

        glm::vec3 tmin = glm::min(t1, t2);
        glm::vec3 tmax = glm::max(t1, t2);

        float t_min = glm::max(tmin.x, glm::max(tmin.y, tmin.z));
        float t_max = glm::min(tmax.x, glm::min(tmax.y, tmax.z));

        if (t_max >= t_min && t_max >= 0.0f) {
            t = t_min > 0.0f ? t_min : t_max;
            return true;
        }
        return false;
    }

    auto RayPicker::ray_sphere_intersect(
        const glm::vec3& ray_origin,
        const glm::vec3& ray_dir,
        const glm::vec3& sphere_center,
        float sphere_radius,
        float& t
    ) -> bool {
        glm::vec3 oc = ray_origin - sphere_center;
        float b = glm::dot(oc, ray_dir);
        float c = glm::dot(oc, oc) - sphere_radius * sphere_radius;
        float h = b * b - c;
        if (h < 0.0f) return false; // No intersection
        h = std::sqrt(h);
        t = -b - h; // Closest intersection
        if (t < 0.0f) t = -b + h; // If inside, try other intersection
        return t >= 0.0f;
    }

} // namespace klingon
