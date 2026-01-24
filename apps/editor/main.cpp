#include "klingon/engine.hpp"
#include "klingon/scene.hpp"
#include "klingon/movement_controller.hpp"
#include "klingon/picking/ray_picker.hpp"
#include "federation/log.hpp"
#include "federation/config_manager.hpp"
#include "editor_config.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <limits>

#include "editor_ui.hpp"
#include "borg/window.hpp"
#include "klingon/renderer.hpp"

auto main() -> int {

    klingon_editor::Editor editor{};

    federation::Logger::set_level(federation::LogLevel::Trace);
    try {
        // Load editor-specific config (includes engine config)
        auto editor_config = federation::ConfigManager::load<EditorConfig>("editor.json");

        // Override engine config with editor-specific values
        editor_config.engine.application.name = "Klingon Editor";
        editor_config.engine.window.width = 1920;
        editor_config.engine.window.height = 1080;
        editor_config.engine.vulkan.instance.enable_validation = true;  // Always on for editor
        editor_config.engine.renderer.debug.enable_imgui = true;  // Always on for editor

        klingon::Engine engine{editor_config.engine};

        // Create scene (owns camera automatically)
        klingon::Scene scene{};
        scene.set_name("Editor Scene");
        scene.get_camera_transform().translation.z = -5.0f; // Start camera further back for editor

        // Create movement controller for scene camera
        klingon::MovementController scene_camera_controller{};
        scene_camera_controller.set_target(&scene.get_camera_transform());
        scene_camera_controller.set_ui_mode(true); // Start in UI mode for editor

        // Register controller with input system
        engine.get_input().add_subscriber(&scene_camera_controller);

        // Enable raw mouse motion if available
        auto *window = engine.get_window().get_native_handle();
        if (::glfwRawMouseMotionSupported()) {
            engine.get_window().set_input_mode(GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }

        editor.add_test_objects(scene, engine);

        // Set active scene (engine handles all rendering automatically)
        engine.set_active_scene(&scene);

        // Set up update callback (editor logic)
        engine.set_update_callback([&](float delta_time) {
            // Update scene camera
            scene_camera_controller.update(window, delta_time, scene.get_camera_transform());
        });

        std::optional<klingon::GameObject::id_t> selected_object_id;
        static ImGuizmo::OPERATION current_gizmo_operation = ImGuizmo::TRANSLATE;
        static ImGuizmo::MODE current_gizmo_mode = ImGuizmo::WORLD;

        // Set up ImGui callback (editor UI)
        engine.set_imgui_callback([&]() {
            ImGuizmo::BeginFrame();

            // Handle object selection
            if (!ImGuizmo::IsUsing() && !::ImGui::GetIO().WantCaptureMouse && ::ImGui::IsMouseClicked(0)) {
                auto mouse_pos = ::ImGui::GetMousePos();
                auto viewport_pos = ::ImGui::GetMainViewport()->Pos;
                auto viewport_size = ::ImGui::GetMainViewport()->Size;

                if (viewport_size.x > 0 && viewport_size.y > 0) {
                    glm::vec2 uv = {
                        (mouse_pos.x - viewport_pos.x) / viewport_size.x,
                        (mouse_pos.y - viewport_pos.y) / viewport_size.y
                    };

                    auto picked_id = klingon::RayPicker::pick_object(scene, uv);
                    if (picked_id) {
                        selected_object_id = picked_id;
                        FED_INFO("Selected object {}", *selected_object_id);
                    } else {
                        selected_object_id.reset();
                    }
                }
            }

            // Main menu bar
            if (::ImGui::BeginMainMenuBar()) {
                if (::ImGui::BeginMenu("File")) {
                    if (::ImGui::MenuItem("Save Scene")) {
                        // engine.save_scene(engine.get_active_scene(), "editor_scene.json");
                    }
                    if (::ImGui::MenuItem("Exit")) {
                        engine.shutdown();
                    }
                    ::ImGui::EndMenu();
                }
                if (::ImGui::BeginMenu("View")) {
                    // View options will go here
                    ::ImGui::EndMenu();
                }
                ::ImGui::EndMainMenuBar();
            }

            // Scene hierarchy window
            ::ImGui::Begin("Scene Hierarchy");
            ::ImGui::Text("Scene: %s", scene.get_name().c_str());
            ::ImGui::Separator();

            if (::ImGui::TreeNode("Game Objects")) {
                ::ImGui::Text("Count: %zu", scene.get_game_objects().size());
                for (const auto &[id, obj]: scene.get_game_objects()) {
                    int flags = ::ImGuiTreeNodeFlags_Leaf | ::ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    if (selected_object_id && *selected_object_id == id) {
                        flags |= ::ImGuiTreeNodeFlags_Selected;
                    }

                    ::ImGui::TreeNodeEx((void*)(intptr_t)id, flags, "Object %u", id);
                    if (::ImGui::IsItemClicked()) {
                        selected_object_id = id;
                    }
                }
                ::ImGui::TreePop();
            }
            ::ImGui::End();

            // Properties window
            ::ImGui::Begin("Properties");
            if (selected_object_id) {
                auto* obj = scene.get_game_object(*selected_object_id);
                if (obj) {
                    ::ImGui::Text("Object ID: %u", *selected_object_id);
                    ::ImGui::Separator();

                    ::ImGui::DragFloat3("Position", &obj->transform.translation.x, 0.1f);

                    glm::vec3 rotation_deg = glm::degrees(obj->transform.rotation);
                    if (::ImGui::DragFloat3("Rotation", &rotation_deg.x, 1.0f)) {
                        obj->transform.rotation = glm::radians(rotation_deg);
                    }

                    ::ImGui::DragFloat3("Scale", &obj->transform.scale.x, 0.1f);

                    ::ImGui::ColorEdit3("Color", &obj->color.x);

                    if (obj->point_light) {
                        ::ImGui::Separator();
                        ::ImGui::Text("Point Light");
                        ::ImGui::DragFloat("Intensity", &obj->point_light->light_intensity, 0.1f, 0.0f, 100.0f);
                    }
                } else {
                    selected_object_id.reset();
                    ::ImGui::Text("Selected object not found (deleted?)");
                }
            } else {
                ::ImGui::Text("Select an object to edit properties");
            }
            ::ImGui::End();

            // Viewport stats
            ::ImGui::Begin("Viewport Stats");
            ::ImGui::Text("FPS: %.1f", ::ImGui::GetIO().Framerate);
            ::ImGui::Text("Frame Time: %.3f ms", 1000.0f / ::ImGui::GetIO().Framerate);
            ::ImGui::Separator();

            auto &cam_transform = scene.get_camera_transform();
            ::ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)",
                          cam_transform.translation.x,
                          cam_transform.translation.y,
                          cam_transform.translation.z);
            ::ImGui::Text("Camera Rotation: (%.2f, %.2f, %.2f)",
                          glm::degrees(cam_transform.rotation.x),
                          glm::degrees(cam_transform.rotation.y),
                          glm::degrees(cam_transform.rotation.z));

            ::ImGui::Separator();
            ::ImGui::Text("Controls:");
            ::ImGui::BulletText("F1 - Toggle Camera Mode");
            ::ImGui::BulletText("WASD - Move Camera (Scene mode)");
            ::ImGui::BulletText("Mouse - Rotate Camera (Scene mode)");
            ::ImGui::BulletText("Click - Select Object");
            ::ImGui::End();

            // Gizmo Toolbar
            ::ImGui::Begin("Toolbar");
            if (::ImGui::RadioButton("Translate", current_gizmo_operation == ImGuizmo::TRANSLATE))
                current_gizmo_operation = ImGuizmo::TRANSLATE;
            ::ImGui::SameLine();
            if (::ImGui::RadioButton("Rotate", current_gizmo_operation == ImGuizmo::ROTATE))
                current_gizmo_operation = ImGuizmo::ROTATE;
            ::ImGui::SameLine();
            if (::ImGui::RadioButton("Scale", current_gizmo_operation == ImGuizmo::SCALE))
                current_gizmo_operation = ImGuizmo::SCALE;

            if (current_gizmo_operation != ImGuizmo::SCALE) {
                ::ImGui::SameLine();
                if (::ImGui::RadioButton("World", current_gizmo_mode == ImGuizmo::WORLD))
                    current_gizmo_mode = ImGuizmo::WORLD;
                ::ImGui::SameLine();
                if (::ImGui::RadioButton("Local", current_gizmo_mode == ImGuizmo::LOCAL))
                    current_gizmo_mode = ImGuizmo::LOCAL;
            }
            ::ImGui::End();

            // ImGuizmo
            if (selected_object_id) {
                auto* obj = scene.get_game_object(*selected_object_id);
                if (obj) {
                    auto viewport = ::ImGui::GetMainViewport();
                    ImGuizmo::SetRect(viewport->Pos.x, viewport->Pos.y, viewport->Size.x, viewport->Size.y);
                    ImGuizmo::SetOrthographic(false);

                    const auto& camera = scene.get_camera();
                    glm::mat4 camera_view = camera.get_view();
                    glm::mat4 camera_projection = camera.get_projection();
                    camera_projection[1][1] *= -1.f; // Flip Y-axis for ImGuizmo
                    glm::mat4 object_matrix = obj->transform.mat4();

                    if (ImGuizmo::Manipulate(glm::value_ptr(camera_view), glm::value_ptr(camera_projection), current_gizmo_operation, current_gizmo_mode, glm::value_ptr(object_matrix))) {
                        glm::vec3 translation, rotation_deg, scale;
                        ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(object_matrix), glm::value_ptr(translation), glm::value_ptr(rotation_deg), glm::value_ptr(scale));

                        obj->transform.translation = translation;
                        obj->transform.rotation = glm::radians(rotation_deg);
                        obj->transform.scale = scale;
                    }
                }
            }
        });

        FED_INFO("Starting editor");

        // Run the engine (blocks until exit)
        engine.run();

        return EXIT_SUCCESS;
    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}