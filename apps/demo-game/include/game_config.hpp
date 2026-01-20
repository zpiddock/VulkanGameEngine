#pragma once

#include "klingon/config.hpp"
#include <cereal/archives/json.hpp>

/**
 * Game-specific configuration.
 * Extends KlingonConfig with gameplay-specific settings.
 */
struct GameConfig {
    klingon::KlingonConfig engine;  // Base engine config

    // Game-specific settings
    struct Gameplay {
        float difficulty = 1.0f;
        float master_volume = 0.8f;
        bool enable_cheats = false;
        bool show_fps = true;

        template<class Archive>
        void serialize(Archive& ar) {
            ar(CEREAL_NVP(difficulty),
               CEREAL_NVP(master_volume),
               CEREAL_NVP(enable_cheats),
               CEREAL_NVP(show_fps));
        }
    } gameplay;

    template<class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(engine), CEREAL_NVP(gameplay));
    }
};
