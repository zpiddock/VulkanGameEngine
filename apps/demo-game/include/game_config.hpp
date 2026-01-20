#pragma once

#include "klingon/config.hpp"
#include <ser20/archives/json.hpp>

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
            ar(SER20_NVP(difficulty),
               SER20_NVP(master_volume),
               SER20_NVP(enable_cheats),
               SER20_NVP(show_fps));
        }
    } gameplay;

    template<class Archive>
    void serialize(Archive& ar) {
        ar(SER20_NVP(engine), SER20_NVP(gameplay));
    }
};
