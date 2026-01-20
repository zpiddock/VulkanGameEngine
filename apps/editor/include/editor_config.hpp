#pragma once

#include "klingon/config.hpp"
#include <ser20/archives/json.hpp>
#include <ser20/types/vector.hpp>

/**
 * Editor-specific configuration.
 * Extends KlingonConfig with editor UI settings.
 */
struct EditorConfig {
    klingon::KlingonConfig engine;

    // Editor UI settings
    struct UI {
        bool enable_docking = true;
        bool enable_viewports = true;
        float font_size = 16.0f;
        std::string theme = "dark";

        // Recent files
        std::vector<std::string> recent_projects = {};

        template<class Archive>
        void serialize(Archive& ar) {
            ar(SER20_NVP(enable_docking),
               SER20_NVP(enable_viewports),
               SER20_NVP(font_size),
               SER20_NVP(theme),
               SER20_NVP(recent_projects));
        }
    } ui;

    template<class Archive>
    void serialize(Archive& ar) {
        ar(SER20_NVP(engine), SER20_NVP(ui));
    }
};
