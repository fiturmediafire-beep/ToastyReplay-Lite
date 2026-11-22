#include "PlaybackButtons.hpp"

#include "../engine/Engine.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

namespace toasty::ui {
    bool playbackButtonsVisible() {
        return Mod::get()->getSavedValue<bool>("show-playback-buttons", false);
    }

    void refreshPlaybackButtons() {
        auto layer = PlayLayer::get();
        if (!layer) return;
        auto menu = layer->getChildByIDRecursive("playback-buttons-menu"_spr);
        if (!menu) return;

        bool show = playbackButtonsVisible();
        menu->setVisible(show);
        if (!show) return;

        auto mode = toasty::engine::mode();
        if (auto playBtn = typeinfo_cast<CCMenuItemSpriteExtra*>(
                menu->getChildByID("playback-play"_spr))) {
            const char* frame = (mode == toasty::engine::Mode::Play)
                ? "GJ_stopEditorBtn_001.png"
                : "GJ_playEditorBtn_001.png";
            if (auto spr = CCSprite::createWithSpriteFrameName(frame)) {
                spr->setScale(.55f);
                playBtn->setNormalImage(spr);
                playBtn->updateSprite();
            }
        }
    }
} // namespace toasty::ui

class $modify(PlaybackBtnPlayLayer, PlayLayer) {
    struct Fields {
        CCMenu* menu = nullptr;
    };

    CCMenuItemSpriteExtra* addBtn(CCMenu* menu,
                                  const char* frame,
                                  const std::string& id,
                                  SEL_MenuHandler handler) {
        auto spr = CCSprite::createWithSpriteFrameName(frame);
        if (!spr) return nullptr;
        spr->setScale(.55f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, handler);
        btn->setID(id);
        menu->addChild(btn);
        return btn;
    }

    bool init(GJGameLevel* level, bool dontCreateObjects) {
        if (!PlayLayer::init(level, dontCreateObjects)) return false;

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        auto menu = CCMenu::create();
        menu->setID("playback-buttons-menu"_spr);
        menu->setPosition({0.f, 0.f});
        menu->setContentSize(winSize);
        m_fields->menu = menu;

        auto stopBtn = this->addBtn(menu,
            "GJ_deleteSongBtn_001.png",
            "playback-stop"_spr,
            menu_selector(PlaybackBtnPlayLayer::onStopPlayback));

        auto playBtn = this->addBtn(menu,
            "GJ_playEditorBtn_001.png",
            "playback-play"_spr,
            menu_selector(PlaybackBtnPlayLayer::onTogglePlayback));

        float edge = winSize.width - 14.f;
        for (auto btn : {stopBtn, playBtn}) {
            if (!btn) continue;
            float half = btn->getScaledContentWidth() / 2.f;
            edge -= half;
            btn->setPosition({edge, 30.f});
            edge -= half + 6.f;
        }

        CCNode* host = m_uiLayer ? static_cast<CCNode*>(m_uiLayer) : static_cast<CCNode*>(this);
        host->addChild(menu, 100);
        handleTouchPriority(menu);

        toasty::ui::refreshPlaybackButtons();
        return true;
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        toasty::ui::refreshPlaybackButtons();
    }

    void onTogglePlayback(CCObject*) {
        toasty::engine::togglePlayback();
        toasty::ui::refreshPlaybackButtons();
    }

    void onStopPlayback(CCObject*) {
        toasty::engine::stopPlayback();
        toasty::ui::refreshPlaybackButtons();
    }
};
