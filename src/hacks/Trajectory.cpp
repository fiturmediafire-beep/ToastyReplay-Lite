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

// ── state ────────────────────────────────────────────────────────────────────
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

// ── TrajectoryNode ────────────────────────────────────────────────────────────
// Attach ke m_objectLayer, draw langsung di object-layer coordinates.
// Tidak perlu konversi — CCDrawNode anak m_objectLayer = sama space.
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

// ── simulation ────────────────────────────────────────────────────────────────
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

// s_impactPos & s_impactSecs disimpan saat simulasi, digambar di redraw
static CCPoint s_impactPos[2]  = {};
static float   s_impactSecs[2] = {-1.f, -1.f};

static void drawImpact(CCDrawNode* node, CCPoint pos, float secs, int idx) {
    // simpan untuk timer label (digambar lewat CCLabelBMFont di PlayLayer node)
    s_impactPos[idx]  = pos;
    s_impactSecs[idx] = secs;

    // kotak hitbox, bukan lingkaran
    float half = 15.f;
    CCPoint verts[4] = {
        pos + CCPoint{-half, -half},
        pos + CCPoint{ half, -half},
        pos + CCPoint{ half,  half},
        pos + CCPoint{-half,  half},
    };
    node->drawPolygon(verts, 4,
        ccc4f(1.f, 0.15f, 0.15f, 0.25f),
        1.5f, ccc4f(1.f, 0.15f, 0.15f, 0.9f));
}

static void simulateOnce(GJBaseGameLayer* bgl, PlayerObject* ghost,
                          PlayerObject* real, bool held, int impactIdx) {
    copyState(real, ghost);
    ghost->releaseButton(PlayerButton::Jump);
    if (held) ghost->pushButton(PlayerButton::Jump);

    // Delta besar = lebih sedikit iterasi, lebih jauh jangkauan, lebih ringan
    // 300 iter × 1/15s = 20 detik ke depan, beban 4x lebih ringan dari 1200×1/60
    static constexpr int   ITERS    = 300;
    static constexpr float DELTA    = 1.f / 15.f;

    // warna: putih=release, kuning=hold
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
            float secs = static_cast<float>(i) * DELTA;
            drawImpact(getNode(), ghost->getPosition(), secs, impactIdx);
            break;
        }

        ghost->update(DELTA);
        ghost->updateRotation(DELTA);

        CCPoint cur = ghost->getPosition();

        // fade alpha di ujung
        if (i >= ITERS - 40)
            col.a = static_cast<float>(ITERS - i) / 40.f;

        // drawSegment = line tebal, pasti kelihatan, sama persis dengan xdBot
        getNode()->drawSegment(prev, cur, 0.6f, col);

        prev = cur;
    }
}

static void updateTrajectory(PlayLayer* pl, PlayerObject* ghost1, PlayerObject* ghost2) {
    if (!ghost1 || !ghost2) return;
    s_creating = true;
    s_impactSecs[0] = s_impactSecs[1] = -1.f;
    getNode()->setVisible(true);
    getNode()->clear();

    simulateOnce(pl, ghost1, pl->m_player1, true,  0);
    simulateOnce(pl, ghost2, pl->m_player1, false, 1);

    s_creating = false;
}

// ── hooks ─────────────────────────────────────────────────────────────────────
static CCLabelBMFont* getTimerLabel(CCNode* parent, int tag) {
    auto* l = typeinfo_cast<CCLabelBMFont*>(parent->getChildByTag(tag));
    if (!l) {
        l = CCLabelBMFont::create("", "bigFont.fnt");
        l->setScale(0.5f);
        l->setTag(tag);
        parent->addChild(l, 200);
    }
    return l;
}

class $modify(TrajPlayLayer, PlayLayer) {
    struct Fields {
        PlayerObject* ghost1 = nullptr;
        PlayerObject* ghost2 = nullptr;
    };

    // setupHasCompleted = setelah semua objek di-load, cocok untuk init ghost
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

    // postUpdate = setelah physics, timing yang benar
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (!toasty::trajectory::enabled() || s_creating) return;
        updateTrajectory(this, m_fields->ghost1, m_fields->ghost2);

        // Gambar timer label di screen space untuk setiap impact point
        for (int idx = 0; idx < 2; idx++) {
            auto* lbl = getTimerLabel(this, 9900 + idx);
            if (s_impactSecs[idx] < 0.f) {
                lbl->setVisible(false);
                continue;
            }
            // Konversi object-layer coords ke screen (UI) coords
            CCPoint world = m_objectLayer->convertToWorldSpace(s_impactPos[idx]);
            lbl->setPosition(world + CCPoint{0.f, 22.f});

            float secs = s_impactSecs[idx];
            ccColor3B col;
            if (secs > 1.f)       col = ccc3(80, 255, 80);
            else if (secs > 0.5f) col = ccc3(255, 220, 50);
            else                  col = ccc3(255, 60, 60);

            lbl->setString(fmt::format("{:.1f}s", secs).c_str());
            lbl->setColor(col);
            lbl->setVisible(true);
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        // jika ghost yang mati, cancel simulasi saja
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
        s_impactSecs[0] = s_impactSecs[1] = -1.f;
        m_fields->ghost1 = nullptr;
        m_fields->ghost2 = nullptr;
        PlayLayer::onQuit();
    }

    void playEndAnimationToPos(CCPoint p0) {
        if (!s_creating)
            PlayLayer::playEndAnimationToPos(p0);
    }
};

// Blokir side effects selama simulasi
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
