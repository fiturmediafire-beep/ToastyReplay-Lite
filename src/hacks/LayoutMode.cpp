#include "LayoutMode.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
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

class $modify(LayoutPlayLayer, PlayLayer) {
    void addObject(GameObject* obj) {
        PlayLayer::addObject(obj);
        if (!toasty::layout::enabled()) return;

        bool isDecoration = (obj->m_objectType == GameObjectType::Decoration
            && obj->m_objectID != 44 && obj->m_objectID != 38
            && obj->m_objectID != 749 && obj->m_objectID != 747);
        bool isFilter = s_objectFilter.contains(obj->m_objectID);

        if (isDecoration || isFilter || obj->m_isNoTouch) {
            obj->setVisible(false);
            return;
        }

        obj->m_hasNoGlow = true;
        obj->m_activeMainColorID = -1;
        obj->m_activeDetailColorID = -1;
        obj->m_detailUsesHSV = false;
        obj->m_baseUsesHSV = false;
        obj->m_isHide = false;
    }
};
