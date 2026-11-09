#include "LayoutMode.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

namespace {
    bool s_enabled = false;

    void applyToLayer(PlayLayer* layer, bool hide) {
        if (!layer || !layer->m_objectLayer) return;

        auto children = layer->m_objectLayer->getChildren();
        if (!children) return;

        for (auto* obj : CCArrayExt<GameObject*>(children)) {
            if (!obj) continue;
            if (obj->m_isNoTouch && !obj->m_isTrigger) {
                obj->setVisible(!hide);
            }
        }
    }
}

namespace toasty::layout {
    bool enabled() {
        return s_enabled;
    }

    void loadSaved() {
        s_enabled = Mod::get()->getSavedValue<bool>("layout-mode", false);
    }

    void setEnabled(bool on) {
        s_enabled = on;
        Mod::get()->setSavedValue<bool>("layout-mode", on);
        apply(on);
    }

    void apply(bool on) {
        if (auto layer = PlayLayer::get()) {
            applyToLayer(layer, on);
        }
    }
}

class $modify(LayoutPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool dontCreateObjects) {
        if (!PlayLayer::init(level, dontCreateObjects)) return false;
        if (toasty::layout::enabled()) {
            toasty::layout::apply(true);
        }
        return true;
    }

    void addObject(GameObject* obj) {
        PlayLayer::addObject(obj);
        if (toasty::layout::enabled() && obj && obj->m_isNoTouch && !obj->m_isTrigger) {
            obj->setVisible(false);
        }
    }
};
