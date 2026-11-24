#include "RandomSeed.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GameToolbox.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>

#include <random>

using namespace geode::prelude;

namespace toasty::seed {

    bool enabled() {
        return Mod::get()->getSavedValue<bool>("set-seed", false);
    }

    uint64_t value() {
        return Mod::get()->getSavedValue<uint64_t>("seed-value", 1);
    }

    void setValue(uint64_t value) {
        Mod::get()->setSavedValue<uint64_t>("seed-value", value);
    }

    void apply(GJBaseGameLayer* layer, uint64_t seed) {
        if (!layer) {
            return;
        }

        // Same seed generation algorithm used by xdBot.
        std::mt19937 generator(
            static_cast<std::mt19937::result_type>(
                seed + layer->m_gameState.m_currentProgress
            )
        );

        std::uniform_int_distribution<int> distribution(
            10000, 999999999
        );

        int finalSeed = distribution(generator);

        layer->m_randomSeed = finalSeed;
        layer->m_replayRandSeed = finalSeed;

        GameToolbox::fast_srand(finalSeed);
    }

}
