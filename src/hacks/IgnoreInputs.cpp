#include "IgnoreInputs.hpp"
#include "../engine/Engine.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

namespace {
    bool s_enabled = false;
}

namespace toasty::ignoreinputs {
    bool enabled()   { return s_enabled; }
    void loadSaved() { s_enabled = Mod::get()->getSavedValue<bool>("ignore-inputs", true); }
    void setEnabled(bool on) {
        s_enabled = on;
        Mod::get()->setSavedValue<bool>("ignore-inputs", on);
    }
}

class $modify(IgnoreInputsBGL, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        if (toasty::ignoreinputs::enabled()) {
            auto session = toasty::engine::activeSession();
            if (!session || !session->acceptingPlaybackInput) return;
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }
};
