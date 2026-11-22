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
    bool  s_ghostDying = false;  // true saat kita handle ghost mati, bukan player asli
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

    // gambar hitbox impact — kotak centered di posisi ghost, skala ikut vehicleSize
    void drawImpactHitbox(PlayerObject* plr) {
        CCPoint c = plr->getPosition();
        float sz = 15.f * plr->m_vehicleSize;

        CCPoint verts[4] = {
            ccp(c.x - sz, c.y - sz),
            ccp(c.x + sz, c.y - sz),
            ccp(c.x + sz, c.y + sz),
            ccp(c.x - sz, c.y + sz),
        };
        drawPolygon(verts, 4,
            ccc4f(1.f, 0.15f, 0.15f, 0.25f), 1.5f,
            ccc4f(1.f, 0.15f, 0.15f, 0.9f));

        float sz2 = sz * 0.3f;
        CCPoint verts2[4] = {
            ccp(c.x - sz2, c.y - sz2),
            ccp(c.x + sz2, c.y - sz2),
            ccp(c.x + sz2, c.y + sz2),
            ccp(c.x - sz2, c.y + sz2),
        };
        drawPolygon(verts2, 4,
            ccc4f(1.f, 0.15f, 0.15f, 0.1f), 1.f,
            ccc4f(1.f, 0.5f, 0.5f, 0.9f));
    }

    void simulateWithBrightness(GJBaseGameLayer* bgl, PlayerObject* plr, bool held, bool bright) {
        if (!plr || plr->m_isDead || !m_ghost) return;

        s_simulating = true;  // set dulu sebelum copy state supaya side effects diblok

        // copy semua state dari player asli
        m_ghost->setPosition(plr->getPosition());
        m_ghost->setRotation(plr->getRotation());
        m_ghost->setScaleX(plr->getScaleX());
        m_ghost->setScaleY(plr->getScaleY());
        m_ghost->m_yVelocity          = plr->m_yVelocity;
        m_ghost->m_isShip             = plr->m_isShip;
        m_ghost->m_isBall             = plr->m_isBall;
        m_ghost->m_isBird             = plr->m_isBird;
        m_ghost->m_isDart             = plr->m_isDart;
        m_ghost->m_isRobot            = plr->m_isRobot;
        m_ghost->m_isSpider           = plr->m_isSpider;
        m_ghost->m_isSwing            = plr->m_isSwing;
        m_ghost->m_isUpsideDown       = plr->m_isUpsideDown;
        m_ghost->m_isOnGround         = plr->m_isOnGround;
        m_ghost->m_isOnGround2        = plr->m_isOnGround;
        m_ghost->m_vehicleSize        = plr->m_vehicleSize;
        m_ghost->m_stateScale         = plr->m_stateScale;
        m_ghost->m_scaleXRelated2     = plr->m_scaleXRelated2;
        m_ghost->m_scaleXRelated3     = plr->m_scaleXRelated3;
        m_ghost->m_scaleXRelated4     = plr->m_scaleXRelated4;
        m_ghost->m_scaleXRelated5     = plr->m_scaleXRelated5;
        m_ghost->m_isDead             = false;
        m_ghost->m_maybeIsColliding   = false;
        m_ghost->m_jumpBuffered       = plr->m_jumpBuffered;
        m_ghost->m_playerSpeed        = plr->m_playerSpeed;
        m_ghost->m_gravityMod         = plr->m_gravityMod;
        // copy button state dulu, baru override dengan held
        m_ghost->m_holdingButtons[0]  = plr->m_holdingButtons[0];
        m_ghost->m_holdingButtons[1]  = plr->m_holdingButtons[1];
        m_ghost->m_holdingButtons[2]  = plr->m_holdingButtons[2];

        m_ghost->releaseButton(PlayerButton::Jump);
        if (held) m_ghost->pushButton(PlayerButton::Jump);

        static constexpr float DELTA = 0.5f;
        static constexpr int   ITERS = 240;

        // bright = arc yang aktif (sesuai tombol), gelap = arc yang tidak aktif
        ccColor4F col = bright
            ? ccc4f(0.3f, 0.7f, 1.0f, 0.9f)   // biru terang
            : ccc4f(0.1f, 0.3f, 0.7f, 0.9f);  // biru gelap

        CCPoint prev = m_ghost->getPosition();

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

            if (s_ghostDying || m_ghost->m_isDead || m_ghost->m_maybeIsColliding) {
                drawImpactHitbox(m_ghost);
                s_ghostDying = false;
                break;
            }
        }

        s_simulating = false;
        s_ghostDying = false;
        m_ghost->m_isDead = false;
        m_ghost->m_maybeIsColliding = false;
    }

    void redraw(GJBaseGameLayer* bgl, PlayerObject* plr) {
        clear();
        if (!s_enabled || !plr) return;
        bool pressing = plr->m_holdingButtons[(int)PlayerButton::Jump];
        // arc yang sesuai tombol ditekan = terang, yang lain = gelap
        simulateWithBrightness(bgl, plr, false, !pressing); // release: terang kalau tidak ditekan
        simulateWithBrightness(bgl, plr, true,  pressing);  // hold:    terang kalau ditekan
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

        // cleanup lama dulu kalau ada
        if (s_node) {
            s_node->removeFromParentAndCleanup(true);
            s_node->release();
            s_node = nullptr;
        }
        if (m_fields->ghost) {
            m_fields->ghost->release();
            m_fields->ghost = nullptr;
        }

        // pakai false supaya ghost tidak terdaftar sebagai m_player2
        m_fields->ghost = PlayerObject::create(1, 1, this, this, false);
        m_fields->ghost->retain();
        m_fields->ghost->setPosition({-99999, -99999});
        m_fields->ghost->setVisible(false);
        // tambahkan ke s_node (CCDrawNode) bukan PlayLayer — jauh dari jangkauan engine
        // s_node dibuat setelah ini, jadi addChild dilakukan di bawah

        s_node = TrajectoryNode::create();
        s_node->retain();
        s_node->m_ghost = m_fields->ghost;
        // ghost di-parent ke s_node — jauh dari PlayLayer, engine tidak akan iterasi
        s_node->addChild(m_fields->ghost);

        ccBlendFunc bf; bf.src = GL_SRC_ALPHA; bf.dst = GL_ONE_MINUS_SRC_ALPHA;
        s_node->setBlendFunc(bf);
        m_objectLayer->addChild(s_node, 500);
    }

      void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (!toasty::trajectory::enabled() || s_simulating) return;
        // jangan redraw saat player mati
        if (!m_player1 || m_player1->m_isDead) {
            if (s_node) s_node->clear();
            return;
        }
        // pastikan s_node selalu ada di objectLayer (bisa hilang setelah resetLevel/record)
        if (s_node && s_node->getParent() != m_objectLayer) {
            if (s_node->getParent()) s_node->removeFromParent();
            m_objectLayer->addChild(s_node, 500);
        }
        if (s_node) s_node->redraw(this, m_player1);
}

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        if (s_simulating && s_node && player == s_node->m_ghost) {
            // JANGAN set m_isDead di sini — engine hook setelah kita akan cek m_isDead
            // dan stop playback kalau true. Tandai lewat flag terpisah.
            s_ghostDying = true;
            return;
        }
        PlayLayer::destroyPlayer(player, obj);
    }

    void onQuit() {
        if (s_node) {
            s_node->removeFromParentAndCleanup(true);
            s_node->release();
            s_node = nullptr;
        }
        if (m_fields->ghost) {
            m_fields->ghost->release();
            m_fields->ghost = nullptr;
        }
        PlayLayer::onQuit();
    }

    void playEndAnimationToPos(CCPoint p0) {
        if (!s_simulating) PlayLayer::playEndAnimationToPos(p0);
    }
};

// ── GJBaseGameLayer hooks ─────────────────────────────────────────────────────
// Port dari qolmod: collisionCheckObjects handle portal saat simulasi
class $modify(TrajBGL, GJBaseGameLayer) {

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        if (s_simulating) return;
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }

    // port qolmod customCollisionCheck — handle portal & orb saat simulasi
    void customCollisionCheck(PlayerObject* player, GameObject* obj) {
        bool intersects = player->getObjectRect().intersectsRect(obj->getObjectRect());
        if (!intersects) return;

        switch (obj->m_objectType) {
            case GameObjectType::CubePortal:
                player->toggleFlyMode(false, false);
                player->toggleRollMode(false, false);
                player->toggleBirdMode(false, false);
                player->toggleDartMode(false, false);
                player->toggleRobotMode(false, false);
                player->toggleSpiderMode(false, false);
                player->toggleSwingMode(false, false);
                break;
            case GameObjectType::ShipPortal:
                player->toggleFlyMode(true, true);
                break;
            case GameObjectType::BallPortal:
                player->toggleRollMode(true, true);
                break;
            case GameObjectType::UfoPortal:
                player->toggleBirdMode(true, true);
                break;
            case GameObjectType::WavePortal:
                player->toggleDartMode(true, true);
                break;
            case GameObjectType::RobotPortal:
                player->toggleRobotMode(true, true);
                break;
            case GameObjectType::SpiderPortal:
                player->toggleSpiderMode(true, true);
                break;
            case GameObjectType::SwingPortal:
                player->toggleSwingMode(true, true);
                break;
            case GameObjectType::NormalGravityPortal:
                this->flipGravity(player, false, true);
                break;
            case GameObjectType::InverseGravityPortal:
                this->flipGravity(player, true, true);
                break;
            case GameObjectType::GravityTogglePortal:
                this->flipGravity(player, !player->m_isUpsideDown, true);
                break;
            case GameObjectType::YellowJumpPad:
            case GameObjectType::PinkJumpPad:
            case GameObjectType::RedJumpPad:
            case GameObjectType::SpiderPad:
                player->bumpPlayer(getBumpMod(player, (int)obj->m_objectType),
                    (int)obj->m_objectType, true, nullptr);
                break;
            default:
                break;
        }
    }

    void collisionCheckObjects(PlayerObject* p0, std::vector<GameObject*>* p1, int p2, float p3) {
        if (!s_simulating || !p1) {
            GJBaseGameLayer::collisionCheckObjects(p0, p1, p2, p3);
            return;
        }

        // saat simulasi: filter hanya Solid/Hazard/Slope, sisanya handle manual (portal dll)
        std::vector<GameObject*> solids;
        for (auto obj : *p1) {
            auto t = obj->m_objectType;
            if (t == GameObjectType::Solid ||
                t == GameObjectType::Hazard ||
                t == GameObjectType::AnimatedHazard ||
                t == GameObjectType::Slope) {
                solids.push_back(obj);
            } else {
                customCollisionCheck(p0, obj);
            }
        }
        int sz = (int)solids.size();
        GJBaseGameLayer::collisionCheckObjects(p0, &solids, sz, p3);
    }

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
