#pragma once

#include "klingon/config.hpp"
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>

/**
 * Editor-specific configuration.
 * Extends KlingonConfig with editor UI settings.
 */
struct EditorConfig {
    klingon::KlingonConfig engine;

    // Editor UI settings
    struct UI {
        bool enable_docking = true;
        bool enable_viewports = false;
        float font_size = 16.0f;
        std::string theme = "dark";

        // Recent files
        std::vector<std::string> recent_projects = {};

        template<class Archive>
        void serialize(Archive& ar) {
            ar(CEREAL_NVP(enable_docking),
               CEREAL_NVP(enable_viewports),
               CEREAL_NVP(font_size),
               CEREAL_NVP(theme),
               CEREAL_NVP(recent_projects));
        }
    } ui;

    template<class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(engine), CEREAL_NVP(ui));
    }
};
