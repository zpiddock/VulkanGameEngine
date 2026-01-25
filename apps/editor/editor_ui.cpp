#include "editor_ui.hpp"

#include "imgui.h"
#include "ImGuizmo.h"
#include "federation/log.hpp"
#include "klingon/picking/ray_picker.hpp"

namespace klingon_editor {

    auto draw_ui() -> void {


    }

    auto Editor::add_test_objects(klingon::Scene &scene, klingon::Engine &engine) -> void {

        auto &device = engine.get_renderer().get_device_ref();
        auto &texture_manager = engine.get_renderer().get_texture_manager();
        klingon::AssetLoader asset_loader{{.device = device, .texture_manager = texture_manager}};

        // Load smooth vase
        auto smooth_vase = klingon::GameObject::create_game_object();
        smooth_vase.model_data = asset_loader.load_model("assets/models/smooth_vase.obj");
        smooth_vase.transform.translation = {-0.5f, 0.5f, 0.0f};
        smooth_vase.transform.scale = glm::vec3{3.f};
        scene.add_game_object(std::move(smooth_vase));

        // Load flat vase
        auto flat_vase = klingon::GameObject::create_game_object();
        flat_vase.model_data = asset_loader.load_model("assets/models/flat_vase.obj");
        flat_vase.transform.translation = {0.5f, 0.5f, 0.0f};
        flat_vase.transform.scale = glm::vec3{3.f};
        scene.add_game_object(std::move(flat_vase));

        // Load floor (quad)
        auto floor = klingon::GameObject::create_game_object();
        floor.model_data = asset_loader.load_model("assets/models/quad.obj");
        floor.transform.translation = {0.f, 0.5f, 0.f};
        floor.transform.scale = glm::vec3{3.f, 1.f, 3.f};
        scene.add_game_object(std::move(floor));

        auto human = klingon::GameObject::create_game_object();
        human.model_data = asset_loader.load_model("assets/models/human.fbx");
        human.transform.translation = {0.f, 0.f, 0.f};
        human.transform.rotation = {glm::radians(-90.f), glm::radians(180.f), 0.f};
        human.transform.scale = glm::vec3{0.01f};
        scene.add_game_object(std::move(human));


        // Create point lights
        std::vector<glm::vec3> light_colors{
                    {1.f, 0.1f, 0.1f},
                    {0.1f, 0.1f, 1.f},
                    {0.1f, 1.f, 0.1f},
                    {1.f, 1.f, 0.1f},
                    {0.1f, 1.f, 1.f},
                    {1.f, 0.1f, 1.f}
        };

        for (std::size_t i = 0; i < light_colors.size(); i++) {
            auto point_light = klingon::GameObject::create_point_light(0.2f, 0.1f, light_colors[i]);
            auto rotate_light = glm::rotate(
                glm::mat4(1.f),
                (static_cast<float>(i) * glm::two_pi<float>()) / static_cast<float>(light_colors.size()),
                {0.f, -1.f, 0.f}
            );
            point_light.transform.translation = glm::vec3(rotate_light * glm::vec4(-1.f, -1.f, -1.f, 1.f));
            scene.add_game_object(std::move(point_light));
        }
    }
}
