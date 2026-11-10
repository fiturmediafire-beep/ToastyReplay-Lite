#include "LayoutMode.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCNode.hpp>

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
                bool isDecoration = obj->m_objectType == GameObjectType::Decoration
                    && obj->m_objectID != 44
                    && obj->m_objectID != 38
                    && obj->m_objectID != 749
                    && obj->m_objectID != 747;
                if (isDecoration) {
                    obj->CCNode::setVisible(!on);
                } else if (obj->m_isHide) {
                    obj->CCNode::setVisible(on);
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

    void addObject(GameObject* obj) {
        PlayLayer::addObject(obj);
        if (toasty::layout::enabled() && obj && obj->m_isHide) {
            obj->m_isHide = false;
            obj->CCNode::setVisible(true);
            obj->setOpacity(255);
        }
    }
};

class $modify(LayoutCCNode, CCNode) {
    void setVisible(bool v) {
        if (!toasty::layout::enabled() || PlayLayer::get() == nullptr) {
            return CCNode::setVisible(v);
        }
        auto* obj = typeinfo_cast<GameObject*>(this);
        if (!obj) return CCNode::setVisible(v);

        bool isDecoration = obj->m_objectType == GameObjectType::Decoration
            && obj->m_objectID != 44
            && obj->m_objectID != 38
            && obj->m_objectID != 749
            && obj->m_objectID != 747;
        if (isDecoration) return CCNode::setVisible(false);
        if (obj->m_isHide && !v) return CCNode::setVisible(true);
        return CCNode::setVisible(v);
    }
};
