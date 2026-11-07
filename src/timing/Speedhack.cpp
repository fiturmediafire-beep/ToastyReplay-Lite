#include "Speedhack.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace {
    bool s_enabled = false;
    double s_rate = toasty::speedhack::Default;

    double boundedRate(double value) {
        if (!std::isfinite(value)) {
            return toasty::speedhack::Default;
        }
        return std::clamp(value, toasty::speedhack::Minimum, toasty::speedhack::Maximum);
    }
}

namespace toasty::speedhack {
    bool enabled() {
        return s_enabled;
    }

    double rate() {
        return s_rate;
    }

    void setEnabled(bool value) {
        s_enabled = value;
        if (Mod::get()->getSavedValue<bool>("speedhack", false) != value) {
            Mod::get()->setSavedValue<bool>("speedhack", value);
        }
        syncAudio();
    }

    void syncAudio() {
        auto engine = FMODAudioEngine::sharedEngine();
        if (!engine || !engine->m_backgroundMusicChannel) {
            return;
        }
        auto scaled = s_enabled && Mod::get()->getSavedValue<bool>("speedhack-audio", false);
        engine->m_backgroundMusicChannel->setPitch(
            static_cast<float>(scaled ? s_rate : Default));
    }

    void setRate(double value) {
        s_rate = boundedRate(value);
        if (Mod::get()->getSavedValue<double>("speedhack-rate", Default) != s_rate) {
            Mod::get()->setSavedValue<double>("speedhack-rate", s_rate);
        }
        syncAudio();
    }
}

class $modify(SpeedhackGJBGL, GJBaseGameLayer) {
    void update(float dt) override {
        if (s_enabled && std::isfinite(dt) && dt > 0.f) {
            dt *= static_cast<float>(s_rate);
        }
        GJBaseGameLayer::update(dt);
    }
};

class $modify(SpeedhackPlayLayer, PlayLayer) {
    void resetLevel() {
        PlayLayer::resetLevel();
        toasty::speedhack::syncAudio();
    }

    void loadFromCheckpoint(CheckpointObject* checkpoint) {
        PlayLayer::loadFromCheckpoint(checkpoint);
        toasty::speedhack::syncAudio();
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        PlayLayer::destroyPlayer(player, object);
        toasty::speedhack::syncAudio();
    }
};

$on_mod(Loaded) {
    s_rate = boundedRate(
        Mod::get()->getSavedValue<double>("speedhack-rate", toasty::speedhack::Default));
    s_enabled = Mod::get()->getSavedValue<bool>("speedhack", false);
}
