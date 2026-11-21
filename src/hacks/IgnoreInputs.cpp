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
        auto session = toasty::engine::activeSession();

        if (toasty::ignoreinputs::enabled()) {
            // block semua input kecuali yang dari replay engine sendiri
            if (!session || !session->acceptingPlaybackInput) return;
        } else if (session && session->mode == toasty::engine::Mode::Play
                   && !session->resetting && !session->acceptingPlaybackInput) {
            // ignore inputs OFF + lagi playback → izinkan input player masuk
            // bypass block engine dengan set acceptingPlaybackInput sementara
            session->acceptingPlaybackInput = true;
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            session->acceptingPlaybackInput = false;
            return;
        }

        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }
};
