#include "LayoutMode.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GameObject.hpp>
using namespace geode::prelude;

namespace {
    bool s_enabled = false;
    static std::unordered_set<int> s_objectFilter = {
        16, 17, 221, 743, 744, 899, 900, 915, 1006, 32, 33, 29, 30,
        104, 105, 717, 718, 1007, 1520, 2903, 3029, 3030, 3031, 2999,
        3606, 3612, 3009, 3010, 3014, 3015, 3021, 3020, 1818, 1819,
        1612, 1613, 3608
    };
}

namespace toasty::layout {
    bool enabled() { return s_enabled; }
    void loadSaved() { s_enabled = Mod::get()->getSavedValue<bool>("layout-mode", false); }
    void setEnabled(bool on) { s_enabled = on; Mod::get()->setSavedValue<bool>("layout-mode", on); }
}

class $modify(LayoutGameObject, GameObject) {
    void addGlow(std::string frame) {
        if (!toasty::layout::enabled()) {
            GameObject::addGlow(frame);
            return;
        }
        bool isDecoration = (m_objectType == GameObjectType::Decoration
            && m_objectID != 44 && m_objectID != 38
            && m_objectID != 749 && m_objectID != 747);
        bool isFilter = s_objectFilter.contains(m_objectID);
        if (isDecoration || isFilter || m_isNoTouch) {
            setVisible(false);
            return;
        }
        m_hasNoGlow = true;
        m_activeMainColorID = -1;
        m_activeDetailColorID = -1;
        m_detailUsesHSV = false;
        m_baseUsesHSV = false;
        m_isHide = false;
        GameObject::addGlow(frame);
    }
};

class $modify(LayoutPlayLayer, PlayLayer) {
    void addObject(GameObject* obj) {
        PlayLayer::addObject(obj);
    }
};
