
#include "TtrlStorage.hpp"

#include <Geode/loader/Mod.hpp>

using namespace geode::prelude;

namespace toasty::replay::ttrl {
    asp::fs::Path defaultReplayDirectory() {
        return Mod::get()->getSaveDir() / "replays";
    }
} // namespace toasty::replay::ttrl
