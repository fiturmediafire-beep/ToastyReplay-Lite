#include "LayoutMode.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GameObject.hpp>

using namespace geode::prelude;

namespace {
    bool s_enabled = false;
}

namespace toasty::layout {
    bool enabled() { return s_enabled; }

    void loadSaved() {
        s_enabled = Mod::get()->getSavedValue<bool>("layout-mode", false);
    }

    void setEnabled(bool on) {
        s_enabled = on;
        Mod::get()->setSavedValue<bool>("layout-mode", on);
    }

    void apply(bool on) {
        if (auto layer = PlayLayer::get()) {
            if (!layer->m_objectLayer) return;
            for (auto* obj : CCArrayExt<GameObject*>(layer->m_objectLayer->getChildren())) {
                if (!obj) continue;
                if (obj->m_objectType == GameObjectType::Decoration
                    && obj->m_objectID != 44
                    && obj->m_objectID != 38
                    && obj->m_objectID != 749
                    && obj->m_objectID != 747) {
                    obj->setVisible(!on);
                }
            }
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
};

class $modify(LayoutGameObject, GameObject) {
    void setVisible(bool v) {
        if (!toasty::layout::enabled() || PlayLayer::get() == nullptr) {
            return GameObject::setVisible(v);
        }
        if (m_objectType == GameObjectType::Decoration
            && m_objectID != 44
            && m_objectID != 38
            && m_objectID != 749
            && m_objectID != 747) {
            return GameObject::setVisible(false);
        }
        return GameObject::setVisible(v);
    }
};
