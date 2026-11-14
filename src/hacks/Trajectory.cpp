#include "Trajectory.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/EffectGameObject.hpp>
#include <Geode/modify/HardStreak.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>

using namespace geode::prelude;

namespace {
    bool  s_enabled          = false;
    bool  s_creating         = false;
    bool  s_cancel           = false;
    float s_delta            = 1.f / 60.f;
    float s_deathRotation    = 0.f;
}

namespace toasty::trajectory {
    bool enabled()    { return s_enabled; }
    void loadSaved()  { s_enabled = Mod::get()->getSavedValue<bool>("show-trajectory", false); }
    void setEnabled(bool on) {
        s_enabled = on;
        Mod::get()->setSavedValue<bool>("show-trajectory", on);
    }
}

class TrajectoryNode : public CCDrawNode {
public:
    static TrajectoryNode* create() {
        auto* n = new TrajectoryNode();
        if (n && n->init()) { n->autorelease(); return n; }
        CC_SAFE_DELETE(n);
        return nullptr;
    }
};

static TrajectoryNode* s_node = nullptr;

static TrajectoryNode* getNode() {
    if (!s_node) {
        s_node = TrajectoryNode::create();
        s_node->retain();
        ccBlendFunc bf; bf.src = GL_SRC_ALPHA; bf.dst = GL_ONE_MINUS_SRC_ALPHA;
        s_node->setBlendFunc(bf);
    }
    return s_node;
}

static void copyState(PlayerObject* src, PlayerObject* dst) {
    dst->setPosition(src->getPosition());
    dst->m_yVelocity        = src->m_yVelocity;
    dst->m_isShip           = src->m_isShip;
    dst->m_isBall           = src->m_isBall;
    dst->m_isBird           = src->m_isBird;
    dst->m_isDart           = src->m_isDart;
    dst->m_isRobot          = src->m_isRobot;
    dst->m_isSpider         = src->m_isSpider;
    dst->m_isSwing          = src->m_isSwing;
    dst->m_isUpsideDown     = src->m_isUpsideDown;
    dst->m_isDead           = false;
    dst->m_maybeIsColliding = false;
}

static void drawImpact(CCDrawNode* node, CCPoint pos) {
    float time  = CCDirector::sharedDirector()->getTotalFrames() * (1.f / 60.f);
    float pulse = 0.5f + 0.5f * std::sin(time * 6.f);
    float ringR = 10.f + pulse * 5.f;
    float alpha = 0.9f - pulse * 0.3f;
    const int SEG = 24;
    std::vector<CCPoint> pts(SEG);
    for (int i = 0; i < SEG; i++) {
        float a = static_cast<float>(i) / SEG * M_PI * 2.f;
        pts[i] = pos + CCPoint{std::cos(a) * ringR, std::sin(a) * ringR};
    }
    node->drawPolygon(pts.data(), SEG,
        ccc4f(1.f, 0.15f, 0.15f, alpha * 0.3f),
        1.5f, ccc4f(1.f, 0.15f, 0.15f, alpha));
}

static void simulateOnce(GJBaseGameLayer* bgl, PlayerObject* ghost,
                          PlayerObject* real, bool held) {
    copyState(real, ghost);
    ghost->releaseButton(PlayerButton::Jump);
    if (held) ghost->pushButton(PlayerButton::Jump);

    static constexpr int   ITERS = 240;
    static constexpr float DELTA = 1.f / 60.f;

    ccColor4F col = held
        ? ccc4f(1.0f, 0.9f, 0.1f, 0.9f)
        : ccc4f(1.0f, 1.0f, 1.0f, 0.9f);

    CCPoint prev = ghost->getPosition();
    s_cancel = false;

    for (int i = 0; i < ITERS; i++) {
        ghost->m_collisionLogTop->removeAllObjects();
        ghost->m_collisionLogBottom->removeAllObjects();
        ghost->m_collisionLogLeft->removeAllObjects();
        ghost->m_collisionLogRight->removeAllObjects();

        bgl->checkCollisions(ghost, DELTA, false);

        if (s_cancel) {
            drawImpact(getNode(), ghost->getPosition());
            break;
        }

        ghost->update(DELTA);
        ghost->updateRotation(DELTA);

        CCPoint cur = ghost->getPosition();

        if (i >= ITERS - 40)
            col.a = static_cast<float>(ITERS - i) / 40.f;

        getNode()->drawSegment(prev, cur, 0.6f, col);
        prev = cur;
    }
}

static void updateTrajectory(PlayLayer* pl, PlayerObject* ghost1, PlayerObject* ghost2) {
    if (!ghost1 || !ghost2) return;
    s_creating = true;
    getNode()->setVisible(true);
    getNode()->clear();
    simulateOnce(pl, ghost1, pl->m_player1, true);
    simulateOnce(pl, ghost2, pl->m_player1, false);
    s_creating = false;
}

class $modify(TrajPlayLayer, PlayLayer) {
    struct Fields {
        PlayerObject* ghost1 = nullptr;
        PlayerObject* ghost2 = nullptr;
    };

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        m_fields->ghost1 = nullptr;
        m_fields->ghost2 = nullptr;
        s_cancel = false;
        s_creating = false;
        if (!toasty::trajectory::enabled()) return;

        m_fields->ghost1 = PlayerObject::create(1, 1, this, this, true);
        m_fields->ghost1->retain();
        m_fields->ghost1->setPosition({0, 105});
        m_fields->ghost1->setVisible(false);
        m_objectLayer->addChild(m_fields->ghost1);

        m_fields->ghost2 = PlayerObject::create(1, 1, this, this, true);
        m_fields->ghost2->retain();
        m_fields->ghost2->setPosition({0, 105});
        m_fields->ghost2->setVisible(false);
        m_objectLayer->addChild(m_fields->ghost2);

        m_objectLayer->addChild(getNode(), 500);
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (!toasty::trajectory::enabled() || s_creating) return;
        updateTrajectory(this, m_fields->ghost1, m_fields->ghost2);
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        if (s_creating && (player == m_fields->ghost1 || player == m_fields->ghost2)) {
            s_deathRotation = player->getRotation();
            s_cancel = true;
            return;
        }
        PlayLayer::destroyPlayer(player, obj);
    }

    void onQuit() {
        if (s_node) s_node->clear();
        s_cancel = false;
        s_creating = false;
        m_fields->ghost1 = nullptr;
        m_fields->ghost2 = nullptr;
        PlayLayer::onQuit();
    }

    void playEndAnimationToPos(CCPoint p0) {
        if (!s_creating) PlayLayer::playEndAnimationToPos(p0);
    }
};

class $modify(TrajBGL, GJBaseGameLayer) {
    bool canBeActivatedByPlayer(PlayerObject* p0, EffectGameObject* p1) {
        if (s_creating) return false;
        return GJBaseGameLayer::canBeActivatedByPlayer(p0, p1);
    }
    void playerTouchedRing(PlayerObject* p0, RingObject* p1) {
        if (!s_creating) GJBaseGameLayer::playerTouchedRing(p0, p1);
    }
    void playerTouchedTrigger(PlayerObject* p0, EffectGameObject* p1) {
        if (!s_creating) GJBaseGameLayer::playerTouchedTrigger(p0, p1);
    }
    void activateSFXTrigger(SFXTriggerGameObject* p0) {
        if (!s_creating) GJBaseGameLayer::activateSFXTrigger(p0);
    }
    void activateSongEditTrigger(SongTriggerGameObject* p0) {
        if (!s_creating) GJBaseGameLayer::activateSongEditTrigger(p0);
    }
    void gameEventTriggered(GJGameEvent p0, int p1, int p2) {
        if (!s_creating) GJBaseGameLayer::gameEventTriggered(p0, p1, p2);
    }
};

class $modify(TrajPlayerObject, PlayerObject) {
    void update(float dt) {
        PlayerObject::update(dt);
        s_delta = dt;
    }
    void playSpiderDashEffect(CCPoint p0, CCPoint p1) {
        if (!s_creating) PlayerObject::playSpiderDashEffect(p0, p1);
    }
    void incrementJumps() {
        if (!s_creating) PlayerObject::incrementJumps();
    }
    void ringJump(RingObject* p0, bool p1) {
        if (!s_creating) PlayerObject::ringJump(p0, p1);
    }
};

class $modify(TrajHardStreak, HardStreak) {
    void addPoint(CCPoint p0) {
        if (!s_creating) HardStreak::addPoint(p0);
    }
};

class $modify(TrajGameObject, GameObject) {
    void playShineEffect() {
        if (!s_creating) GameObject::playShineEffect();
    }
};

class $modify(TrajEffectGameObject, EffectGameObject) {
    void triggerObject(GJBaseGameLayer* p0, int p1, const std::vector<int>* p2) {
        if (!s_creating) EffectGameObject::triggerObject(p0, p1, p2);
    }
};
