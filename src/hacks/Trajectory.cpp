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

// ── state ─────────────────────────────────────────────────────────────────────
namespace {
    bool  s_enabled    = false;
    bool  s_simulating = false;
}

namespace toasty::trajectory {
    bool enabled()   { return s_enabled; }
    void loadSaved() { s_enabled = Mod::get()->getSavedValue<bool>("show-trajectory", false); }
    void setEnabled(bool on) {
        s_enabled = on;
        Mod::get()->setSavedValue<bool>("show-trajectory", on);
    }
}

// ── TrajectoryNode ─────────────────────────────────────────────────────────────
class TrajectoryNode : public CCDrawNode {
public:
    PlayerObject* m_ghost = nullptr;

    static TrajectoryNode* create() {
        auto* n = new TrajectoryNode();
        if (n && n->init()) { n->autorelease(); return n; }
        CC_SAFE_DELETE(n);
        return nullptr;
    }

    bool isSimulating() { return s_simulating; }

    void simulate(PlayerObject* plr, bool held) {
        if (!plr || plr->m_isDead || !m_ghost) return;

        // copy state
        m_ghost->setPosition(plr->getPosition());
        m_ghost->m_yVelocity        = plr->m_yVelocity;
        m_ghost->m_isShip           = plr->m_isShip;
        m_ghost->m_isBall           = plr->m_isBall;
        m_ghost->m_isBird           = plr->m_isBird;
        m_ghost->m_isDart           = plr->m_isDart;
        m_ghost->m_isRobot          = plr->m_isRobot;
        m_ghost->m_isSpider         = plr->m_isSpider;
        m_ghost->m_isSwing          = plr->m_isSwing;
        m_ghost->m_isUpsideDown     = plr->m_isUpsideDown;
        m_ghost->m_isDead           = false;
        m_ghost->m_maybeIsColliding = false;

        m_ghost->releaseButton(PlayerButton::Jump);
        if (held) m_ghost->pushButton(PlayerButton::Jump);

        // sama persis qolmod: delta=0.5, iters=240
        static constexpr float DELTA = 0.5f;
        static constexpr int   ITERS = 240;

        ccColor4F col = held
            ? ccc4f(1.0f, 0.9f, 0.1f, 0.9f)
            : ccc4f(1.0f, 1.0f, 1.0f, 0.9f);

        auto* bgl = static_cast<GJBaseGameLayer*>(
            CCDirector::sharedDirector()->getRunningScene()->getChildByType<PlayLayer>(0));
        if (!bgl) return;

        CCPoint prev = m_ghost->getPosition();
        s_simulating = true;

        for (int i = 0; i < ITERS; i++) {
            m_ghost->m_collisionLogTop->removeAllObjects();
            m_ghost->m_collisionLogBottom->removeAllObjects();
            m_ghost->m_collisionLogLeft->removeAllObjects();
            m_ghost->m_collisionLogRight->removeAllObjects();

            // qolmod order: update → checkCollisions → updateRotation
            m_ghost->update(DELTA);
            bgl->checkCollisions(m_ghost, DELTA, false);
            m_ghost->updateRotation(DELTA);

            CCPoint cur = m_ghost->getPosition();

            // fade di ujung
            if (i >= ITERS - 30)
                col.a = static_cast<float>(ITERS - i) / 30.f;

            drawSegment(prev, cur, 1.f, col);
            prev = cur;

            if (m_ghost->m_isDead || m_ghost->m_maybeIsColliding)
                break;
        }

        s_simulating = false;
    }

    void redraw(PlayerObject* plr) {
        clear();
        if (!s_enabled || !plr) return;
        simulate(plr, false);
        simulate(plr, true);
    }
};

static TrajectoryNode* s_node = nullptr;

// ── PlayLayer hooks ───────────────────────────────────────────────────────────
class $modify(TrajPlayLayer, PlayLayer) {
    struct Fields {
        PlayerObject* ghost = nullptr;
    };

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        if (!toasty::trajectory::enabled()) return;

        // buat ghost player
        m_fields->ghost = PlayerObject::create(1, 1, this, this, true);
        m_fields->ghost->retain();
        m_fields->ghost->setPosition({0, 105});
        m_fields->ghost->setVisible(false);
        m_objectLayer->addChild(m_fields->ghost);

        // buat draw node, attach ke m_objectLayer (sama space)
        if (!s_node) {
            s_node = TrajectoryNode::create();
            s_node->retain();
        }
        s_node->m_ghost = m_fields->ghost;
        s_node->clear();

        ccBlendFunc bf; bf.src = GL_SRC_ALPHA; bf.dst = GL_ONE_MINUS_SRC_ALPHA;
        s_node->setBlendFunc(bf);

        m_objectLayer->addChild(s_node, 500);
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (!toasty::trajectory::enabled() || s_simulating) return;
        if (s_node) s_node->redraw(m_player1);
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        // qolmod: saat simulasi, cukup set m_isDead = true
        if (s_simulating && player == s_node->m_ghost) {
            player->m_isDead = true;
            return;
        }
        PlayLayer::destroyPlayer(player, obj);
    }

    void onQuit() {
        if (s_node) {
            s_node->removeFromParentAndCleanup(false);
            s_node->clear();
            s_node->m_ghost = nullptr;
        }
        m_fields->ghost = nullptr;
        PlayLayer::onQuit();
    }

    void playEndAnimationToPos(CCPoint p0) {
        if (!s_simulating) PlayLayer::playEndAnimationToPos(p0);
    }
};

// ── GJBaseGameLayer hooks ─────────────────────────────────────────────────────
class $modify(TrajBGL, GJBaseGameLayer) {
    bool canBeActivatedByPlayer(PlayerObject* p0, EffectGameObject* p1) {
        if (s_simulating) return false;
        return GJBaseGameLayer::canBeActivatedByPlayer(p0, p1);
    }
    void playerTouchedRing(PlayerObject* p0, RingObject* p1) {
        if (!s_simulating) GJBaseGameLayer::playerTouchedRing(p0, p1);
    }
    void playerTouchedTrigger(PlayerObject* p0, EffectGameObject* p1) {
        if (!s_simulating) GJBaseGameLayer::playerTouchedTrigger(p0, p1);
    }
    void activateSFXTrigger(SFXTriggerGameObject* p0) {
        if (!s_simulating) GJBaseGameLayer::activateSFXTrigger(p0);
    }
    void activateSongEditTrigger(SongTriggerGameObject* p0) {
        if (!s_simulating) GJBaseGameLayer::activateSongEditTrigger(p0);
    }
    void gameEventTriggered(GJGameEvent p0, int p1, int p2) {
        if (!s_simulating) GJBaseGameLayer::gameEventTriggered(p0, p1, p2);
    }
};

class $modify(TrajPlayerObject, PlayerObject) {
    void playSpiderDashEffect(CCPoint p0, CCPoint p1) {
        if (!s_simulating) PlayerObject::playSpiderDashEffect(p0, p1);
    }
    void incrementJumps() {
        if (!s_simulating) PlayerObject::incrementJumps();
    }
    void ringJump(RingObject* p0, bool p1) {
        if (!s_simulating) PlayerObject::ringJump(p0, p1);
    }
};

class $modify(TrajHardStreak, HardStreak) {
    void addPoint(CCPoint p0) {
        if (!s_simulating) HardStreak::addPoint(p0);
    }
};

class $modify(TrajGameObject, GameObject) {
    void playShineEffect() {
        if (!s_simulating) GameObject::playShineEffect();
    }
};

class $modify(TrajEffectGameObject, EffectGameObject) {
    void triggerObject(GJBaseGameLayer* p0, int p1, const std::vector<int>* p2) {
        if (!s_simulating) EffectGameObject::triggerObject(p0, p1, p2);
    }
};
