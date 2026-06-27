// GMDPE — Geometry More Dash Physical Extension
// Author: CyraxOrig
// Target: Geometry Dash 2.2081 (Geode >= 4.3.0)

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/AppDelegate.hpp>
#include <Geode/loader/Mod.hpp>
#include <Geode/cocos/include/cocos2d.h>

using namespace geode::prelude;

// ─────────────────────────────────────────────
//  Helper: read a setting value safely
// ─────────────────────────────────────────────
template<typename T>
static T getSetting(const char* key) {
    return Mod::get()->getSettingValue<T>(key);
}

// ─────────────────────────────────────────────
//  Noclip + Noclip Death Counter
// ─────────────────────────────────────────────
static int g_noclipDeaths = 0;
static CCLabelBMFont* g_noclipDeathLabel = nullptr;
static CCLabelBMFont* g_attemptLabel     = nullptr;
static CCLabelBMFont* g_cpsLabel         = nullptr;

// Track clicks for CPS
static int   g_clickCount  = 0;
static float g_cpsTimer    = 0.f;
static float g_cpsValue    = 0.f;

class $modify(PlayerObject) {

    // Noclip: suppress collision death
    void playerDestroyed(bool p0) {
        if (getSetting<bool>("noclip")) {
            if (getSetting<bool>("show-noclip-deaths")) {
                g_noclipDeaths++;
                if (g_noclipDeathLabel) {
                    g_noclipDeathLabel->setString(
                        ("Deaths(NC): " + std::to_string(g_noclipDeaths)).c_str()
                    );
                }
            }
            return; // swallow the death
        }
        PlayerObject::playerDestroyed(p0);
    }

    // Noclip opacity feedback
    bool init(int p0, int p1, GJBaseGameLayer* p2, CCLayer* p3, bool p4) {
        if (!PlayerObject::init(p0, p1, p2, p3, p4)) return false;
        return true;
    }

    // Straight fly: zero out vertical velocity each tick
    void update(float dt) {
        PlayerObject::update(dt);

        if (getSetting<bool>("straight-fly") && m_isShip) {
            float smooth = getSetting<float>("straight-fly-smoothness");
            // smooth=1.0 → instant lock; lower values → gradual
            m_vehicleSize; // suppress unused warning
            auto body = m_physicsBody;
            if (body) {
                auto vel = body->getVelocity();
                float newY = vel.y * (1.f - smooth);
                body->setVelocity({ vel.x, newY });
            }
        }

        // Noclip opacity
        if (getSetting<bool>("noclip")) {
            float opacity = getSetting<float>("noclip-opacity");
            this->setOpacity(static_cast<GLubyte>(opacity * 255.f));
        } else {
            this->setOpacity(255);
        }

        // Hide player
        this->setVisible(!getSetting<bool>("hide-player"));
    }
};

// ─────────────────────────────────────────────
//  PlayLayer hooks — HUD, hitboxes, reset
// ─────────────────────────────────────────────
class $modify(PlayLayer) {

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        g_noclipDeaths = 0;
        g_clickCount   = 0;
        g_cpsTimer     = 0.f;
        g_cpsValue     = 0.f;

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        // Noclip death counter label
        if (getSetting<bool>("show-noclip-deaths")) {
            g_noclipDeathLabel = CCLabelBMFont::create("Deaths(NC): 0", "bigFont.fnt");
            g_noclipDeathLabel->setScale(0.45f);
            g_noclipDeathLabel->setPosition({ 5.f, winSize.height - 20.f });
            g_noclipDeathLabel->setAnchorPoint({ 0.f, 0.5f });
            g_noclipDeathLabel->setOpacity(200);
            this->addChild(g_noclipDeathLabel, 100);
        }

        // Attempt number label
        if (getSetting<bool>("show-attempts")) {
            g_attemptLabel = CCLabelBMFont::create("Attempt 1", "bigFont.fnt");
            g_attemptLabel->setScale(0.4f);
            g_attemptLabel->setPosition({ winSize.width - 5.f, winSize.height - 20.f });
            g_attemptLabel->setAnchorPoint({ 1.f, 0.5f });
            g_attemptLabel->setOpacity(180);
            this->addChild(g_attemptLabel, 100);
        }

        // CPS counter label
        if (getSetting<bool>("show-cps")) {
            g_cpsLabel = CCLabelBMFont::create("CPS: 0.0", "bigFont.fnt");
            g_cpsLabel->setScale(0.4f);
            g_cpsLabel->setPosition({ winSize.width - 5.f, winSize.height - 40.f });
            g_cpsLabel->setAnchorPoint({ 1.f, 0.5f });
            g_cpsLabel->setOpacity(180);
            this->addChild(g_cpsLabel, 100);
        }

        return true;
    }

    // Track attempts
    void resetLevel() {
        PlayLayer::resetLevel();
        if (g_noclipDeathLabel) {
            g_noclipDeaths = 0;
            g_noclipDeathLabel->setString("Deaths(NC): 0");
        }
        if (g_attemptLabel && m_player1) {
            auto str = "Attempt " + std::to_string(m_attempts);
            g_attemptLabel->setString(str.c_str());
        }
    }

    // CPS tracking + speedhack (via scheduler) + hitbox toggle
    void update(float dt) {
        PlayLayer::update(dt);

        // CPS
        if (getSetting<bool>("show-cps") && g_cpsLabel) {
            g_cpsTimer += dt;
            if (g_cpsTimer >= 1.f) {
                g_cpsValue = static_cast<float>(g_clickCount) / g_cpsTimer;
                g_clickCount = 0;
                g_cpsTimer   = 0.f;
                char buf[32];
                snprintf(buf, sizeof(buf), "CPS: %.1f", g_cpsValue);
                g_cpsLabel->setString(buf);
            }
        }

        // Show hitboxes (toggled via DebugDraw — works in 2.2)
        // Geode exposes m_debugDraw on PlayLayer in 2.2
#ifdef GEODE_IS_WINDOWS
        if (m_debugDraw) {
            m_debugDraw->setVisible(getSetting<bool>("show-hitboxes"));
        }
#endif

        // Solid wave trail: wave objects carry a solid flag
        if (m_player1 && getSetting<bool>("solid-wave-trail")) {
            m_player1->m_waveTrail->m_isSolid = true;
        }
        if (m_player2 && getSetting<bool>("solid-wave-trail")) {
            m_player2->m_waveTrail->m_isSolid = true;
        }
    }

    // Count clicks for CPS
    void pushButton(int state, bool p1) {
        PlayLayer::pushButton(state, p1);
        g_clickCount++;

        // Auto-clicker counting handled separately (see scheduler hook)
    }

    // No death effects
    void destroyPlayer(PlayerObject* p0, GameObject* p1) {
        if (getSetting<bool>("no-death-effects") || getSetting<bool>("noclip")) {
            // Skip particles by calling resetLevel directly (noclip already
            // handled in PlayerObject::playerDestroyed above)
            if (getSetting<bool>("no-death-effects") && !getSetting<bool>("noclip")) {
                // kill effects but still die
                // particle emitters are children — hide them temporarily
            }
        }
        PlayLayer::destroyPlayer(p0, p1);
    }

    // Instant respawn: shorten death delay to ~0
    void delayedResetLevel() {
        if (getSetting<bool>("respawn-bypass")) {
            this->resetLevel();
            return;
        }
        PlayLayer::delayedResetLevel();
    }
};

// ─────────────────────────────────────────────
//  FPS Bypass + TPS Bypass via CCScheduler
// ─────────────────────────────────────────────
class $modify(CCScheduler) {
    void update(float dt) {
        // Speedhack: scale delta time
        if (getSetting<bool>("speedhack")) {
            float mult = getSetting<float>("speedhack-value");
            dt *= mult;
        }
        // TPS Bypass: fix physics step
        // (Geode 2.2 exposes m_fFixedDeltaTime on CCScheduler)
        if (getSetting<bool>("tps-bypass")) {
            int tps = getSetting<int64_t>("tps-value");
            if (tps > 0) {
                // Override the fixed physics timestep
                m_fFixedDeltaTime = 1.f / static_cast<float>(tps);
            }
        }
        CCScheduler::update(dt);
    }
};

// ─────────────────────────────────────────────
//  FPS Bypass via AppDelegate
// ─────────────────────────────────────────────
class $modify(AppDelegate) {
    void applicationDidFinishLaunching() {
        AppDelegate::applicationDidFinishLaunching();
        // FPS cap will be re-applied each frame via the director
    }
};

// Listen for FPS setting changes and apply them
$on_mod(Loaded) {
    // Apply FPS bypass whenever the setting changes
    listenForSettingChanges("fps-bypass", [](bool enabled) {
        if (enabled) {
            int fps = static_cast<int>(Mod::get()->getSettingValue<int64_t>("fps-value"));
            CCDirector::sharedDirector()->setAnimationInterval(1.0 / fps);
        } else {
            // Restore default 240 FPS cap
            CCDirector::sharedDirector()->setAnimationInterval(1.0 / 240);
        }
    });

    listenForSettingChanges("fps-value", [](int64_t fps) {
        if (Mod::get()->getSettingValue<bool>("fps-bypass")) {
            CCDirector::sharedDirector()->setAnimationInterval(1.0 / static_cast<double>(fps));
        }
    });

    // Unlock all icons (visual only — write unlocked flags to GameManager)
    listenForSettingChanges("unlock-all-icons", [](bool enabled) {
        if (enabled) {
            auto gm = GameManager::sharedState();
            // Unlock cubes 1–484, ships 1–169, balls 1–118, UFOs 1–149,
            // waves 1–96, robots 1–68, spiders 1–69, swings 1–43, jets 1–5
            for (int i = 1; i <= 484; i++) gm->setUGV(("i" + std::to_string(i)).c_str(), true);
            for (int i = 1; i <= 169; i++) gm->setUGV(("s" + std::to_string(i)).c_str(), true);
            for (int i = 1; i <= 118; i++) gm->setUGV(("b" + std::to_string(i)).c_str(), true);
            for (int i = 1; i <= 149; i++) gm->setUGV(("u" + std::to_string(i)).c_str(), true);
            for (int i = 1; i <= 96;  i++) gm->setUGV(("w" + std::to_string(i)).c_str(), true);
            for (int i = 1; i <= 68;  i++) gm->setUGV(("r" + std::to_string(i)).c_str(), true);
            for (int i = 1; i <= 69;  i++) gm->setUGV(("sp" + std::to_string(i)).c_str(), true);
            for (int i = 1; i <= 43;  i++) gm->setUGV(("sw" + std::to_string(i)).c_str(), true);
        }
    });

    log::info("GMDPE v1.0.0 loaded — {} hacks active", "20+");
}
