#pragma once
#include "klingon/engine.hpp"
#include "klingon/scene.hpp"

namespace klingon_editor {
    class Editor {

    public:
        Editor() = default;

        ~Editor() = default;
        auto add_test_objects(klingon::Scene& scene, klingon::Engine& engine) -> void;
    };

    auto draw_ui() -> void;

} // namespace klingon_editor
