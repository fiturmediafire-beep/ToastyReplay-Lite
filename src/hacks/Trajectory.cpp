#include "Trajectory.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>

using namespace geode::prelude;

namespace {
    bool s_enabled = false;
}

namespace toasty::trajectory {
    bool enabled() { return s_enabled; }
    void loadSaved() { s_enabled = Mod::get()->getSavedValue<bool>("show-trajectory", false); }
    void setEnabled(bool on) {
        s_enabled = on;
        Mod::get()->setSavedValue<bool>("show-trajectory", on);
    }
}

class TrajectoryNode : public CCDrawNode {
public:
    static TrajectoryNode* create(GJBaseGameLayer* bgl) {
        auto* node = new TrajectoryNode();
        if (node && node->init(bgl)) {
            node->autorelease();
            return node;
        }
        CC_SAFE_DELETE(node);
        return nullptr;
    }

    void redraw() {
        this->clear();

        // Remove old labels
        if (auto* l = typeinfo_cast<CCLabelBMFont*>(this->getChildByTag(100))) l->setVisible(false);
        if (auto* l = typeinfo_cast<CCLabelBMFont*>(this->getChildByTag(101))) l->setVisible(false);

        if (!toasty::trajectory::enabled()) return;
        auto* pl = PlayLayer::get();
        if (!pl || !pl->m_player1) return;

        simulateOnce(pl->m_player1, false);
        simulateOnce(pl->m_player1, true);
    }

private:
    GJBaseGameLayer* m_bgl = nullptr;
    PlayerObject*    m_ghost = nullptr;

    bool init(GJBaseGameLayer* bgl) {
        if (!CCDrawNode::init()) return false;
        m_bgl = bgl;
        m_ghost = PlayerObject::create(0, 0, bgl, bgl->m_objectLayer, false);
        m_ghost->setVisible(false);
        bgl->m_objectLayer->addChild(m_ghost);
        return true;
    }

    void copyState(PlayerObject* src, PlayerObject* dst) {
        dst->setPosition(src->getPosition());
        dst->m_yVelocity     = src->m_yVelocity;
        dst->m_isShip        = src->m_isShip;
        dst->m_isBall        = src->m_isBall;
        dst->m_isBird        = src->m_isBird;
        dst->m_isDart        = src->m_isDart;
        dst->m_isRobot       = src->m_isRobot;
        dst->m_isSpider      = src->m_isSpider;
        dst->m_isSwing       = src->m_isSwing;
        dst->m_isUpsideDown  = src->m_isUpsideDown;
        dst->m_isDead        = false;
        dst->m_maybeIsColliding = false;
    }

    void simulateOnce(PlayerObject* plr, bool held) {
        copyState(plr, m_ghost);
        m_ghost->releaseButton(PlayerButton::Jump);
        if (held) m_ghost->pushButton(PlayerButton::Jump);

        // Kuning = hold, putih = release
ccColor4F baseCol = held
    ? ccc4f(1.0f, 0.9f, 0.1f, 1.f)   // kuning
    : ccc4f(1.0f, 1.0f, 1.0f, 1.f);  // putih

static constexpr int   ITERS    = 200;
static constexpr float DELTA    = 0.5f / 60.f;
static constexpr float DOT_STEP = 4.f;   // lebih rapat
static constexpr float BASE_R   = 6.f;   // lebih besar

        CCPoint prev    = m_ghost->getPosition();
        float   accDist = 0.f;
        int     dotIdx  = 0;

        for (int i = 0; i < ITERS; i++) {
            m_ghost->m_collisionLogTop->removeAllObjects();
            m_ghost->m_collisionLogBottom->removeAllObjects();
            m_ghost->m_collisionLogLeft->removeAllObjects();
            m_ghost->m_collisionLogRight->removeAllObjects();

            m_ghost->update(DELTA);
            m_bgl->checkCollisions(m_ghost, DELTA, false);
            m_ghost->updateRotation(DELTA);

            CCPoint cur = m_ghost->getPosition();
            float   segLen = ccpDistance(prev, cur);
            accDist += segLen;

            float t = static_cast<float>(i) / static_cast<float>(ITERS);

            if (m_ghost->m_isDead || m_ghost->m_maybeIsColliding) {
                // Lingkaran merah berdenyut
                float time  = CCDirector::sharedDirector()->getTotalFrames() * (1.f / 60.f);
                float pulse = 0.5f + 0.5f * std::sin(time * 6.f);
                float ringR = 10.f + pulse * 5.f;
                float alpha = 0.9f - pulse * 0.3f;
                drawCircle(cur, ringR, ccc4f(1.f, 0.15f, 0.15f, alpha),
                           1.5f, ccc4f(1.f, 0.15f, 0.15f, 0.f), 24);

                // Timer label
                float secs = static_cast<float>(i) * DELTA;
                ccColor3B textCol;
                if (secs > 1.f)       textCol = ccc3(80, 255, 80);
                else if (secs > 0.5f) textCol = ccc3(255, 220, 50);
                else                  textCol = ccc3(255, 60, 60);

                int tag = held ? 100 : 101;
                auto* label = typeinfo_cast<CCLabelBMFont*>(this->getChildByTag(tag));
                if (!label) {
                    label = CCLabelBMFont::create("", "bigFont.fnt");
                    label->setScale(0.35f);
                    label->setTag(tag);
                    this->addChild(label);
                }
                label->setString(fmt::format("{:.1f}s", secs).c_str());
                label->setColor(textCol);
                CCPoint worldPos = m_bgl->m_objectLayer->convertToWorldSpace(cur);
                CCPoint localPos = this->convertToNodeSpace(worldPos);
                label->setPosition(localPos + CCPoint{0.f, 18.f});
                label->setVisible(true);
                break;
            }

            // Dots dengan fade
            while (accDist >= DOT_STEP) {
                accDist -= DOT_STEP;
                dotIdx++;
                float alpha = 0.85f - t * 0.75f;
                float r     = BASE_R * (1.f - t * 0.4f);
                if (dotIdx % 5 == 0) r *= 1.4f;
                ccColor4F col = baseCol;
                col.a = alpha;
                drawDot(cur, r, col);
            }

            prev = cur;
        }
    }

    void drawCircle(CCPoint center, float radius, ccColor4F fill,
                    float bw, ccColor4F border, int seg) {
        std::vector<CCPoint> pts;
        pts.reserve(seg);
        for (int i = 0; i < seg; i++) {
            float a = static_cast<float>(i) / static_cast<float>(seg) * M_PI * 2.f;
            pts.push_back(center + CCPoint{std::cos(a) * radius, std::sin(a) * radius});
        }
        drawPolygon(pts.data(), static_cast<unsigned int>(pts.size()), fill, bw, border);
    }
};

class $modify(TrajPlayLayer, PlayLayer) {
    struct Fields {
        TrajectoryNode* traj = nullptr;
    };

    bool init(GJGameLevel* level, bool dontCreateObjects) {
        if (!PlayLayer::init(level, dontCreateObjects)) return false;
        if (toasty::trajectory::enabled()) {
            auto* node = TrajectoryNode::create(this);
            if (node) {
                this->m_objectLayer->addChild(node, 100);
                m_fields->traj = node;
            }
        }
        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);
        if (m_fields->traj) {
            m_fields->traj->redraw();
        }
    }
};
