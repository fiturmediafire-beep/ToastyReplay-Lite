#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include "Engine.hpp"
#include "RandomSeed.hpp"

using namespace geode::prelude;

class $modify(ToastySeedReset, PlayLayer) {
    void resetLevel() {
        PlayLayer::resetLevel();

        auto session = toasty::engine::activeSession();
        if (!session) return;

        if (session->mode == toasty::engine::Mode::Record && session->recording.seed) {
            toasty::seed::apply(this, *session->recording.seed);
        } else if (session->mode == toasty::engine::Mode::Play &&
                   session->playback && session->playback->seed) {
            toasty::seed::apply(this, *session->playback->seed);
        }
    }
};
