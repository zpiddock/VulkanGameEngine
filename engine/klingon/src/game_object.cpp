#include "klingon/game_object.hpp"

namespace klingon {
    auto GameObject::create_game_object() -> GameObject {
        static id_t current_id = 0;
        return GameObject{current_id++};
    }

    auto GameObject::create_point_light(float intensity, float radius, glm::vec3 color) -> GameObject {
        auto game_obj = create_game_object();
        game_obj.color = color;
        game_obj.transform.scale.x = radius;
        game_obj.point_light = std::make_unique<PointLightComponent>();
        game_obj.point_light->light_intensity = intensity;
        return game_obj;
    }
} // namespace klingon
