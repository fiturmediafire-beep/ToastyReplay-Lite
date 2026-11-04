#include "TtrlStorage.hpp"

#include <Geode/loader/Mod.hpp>
#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace toasty::replay::ttrl {
    asp::fs::Path defaultReplayDirectory() {
        auto path = Mod::get()->getSaveDir() / "replays";
        log::info("Replay directory: {}", path.toString().unwrapOrDefault());
        return path;
    }
} // namespace toasty::replay::ttrl
