#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/CCScheduler.hpp>

using namespace geode::prelude;

// ── Noclip ──────────────────────────────────────
class $modify(PlayerObject) {
    void playerDestroyed(bool p0) {
        if (Mod::get()->getSettingValue<bool>("noclip")) return;
        PlayerObject::playerDestroyed(p0);
    }

    void update(float dt) {
        PlayerObject::update(dt);

        // Straight fly
        if (Mod::get()->getSettingValue<bool>("straight-fly") && m_isShip) {
            if (auto body = m_physicsBody) {
                auto vel = body->getVelocity();
                float s = Mod::get()->getSettingValue<double>("straight-fly-smoothness");
                body->setVelocity({ vel.x, vel.y * (1.f - s) });
            }
        }

        // Noclip opacity
        float opacity = Mod::get()->getSettingValue<bool>("noclip")
            ? Mod::get()->getSettingValue<double>("noclip-opacity") * 255.f
            : 255.f;
        setOpacity(static_cast<GLubyte>(opacity));

        // Hide player
        setVisible(!Mod::get()->getSettingValue<bool>("hide-player"));
    }
};

// ── Speedhack + TPS ─────────────────────────────
class $modify(CCScheduler) {
    void update(float dt) {
        if (Mod::get()->getSettingValue<bool>("speedhack")) {
            dt *= static_cast<float>(
                Mod::get()->getSettingValue<double>("speedhack-value")
            );
        }
        CCScheduler::update(dt);
    }
};

// ── PlayLayer ────────────────────────────────────
class $modify(PlayLayer) {
    // Instant respawn
    void delayedResetLevel() {
        if (Mod::get()->getSettingValue<bool>("respawn-bypass")) {
            resetLevel();
            return;
        }
        PlayLayer::delayedResetLevel();
    }

    // Solid wave trail + hitboxes
    void update(float dt) {
        PlayLayer::update(dt);

        bool solid = Mod::get()->getSettingValue<bool>("solid-wave-trail");
        if (m_player1 && m_player1->m_waveTrail)
            m_player1->m_waveTrail->m_isSolid = solid;
        if (m_player2 && m_player2->m_waveTrail)
            m_player2->m_waveTrail->m_isSolid = solid;
    }
};

// ── FPS Bypass ───────────────────────────────────
$on_mod(Loaded) {
    auto applyFps = [](bool) {
        bool on  = Mod::get()->getSettingValue<bool>("fps-bypass");
        int  fps = static_cast<int>(Mod::get()->getSettingValue<int64_t>("fps-value"));
        CCDirector::sharedDirector()->setAnimationInterval(1.0 / (on ? fps : 240));
    };
    listenForSettingChanges<bool>("fps-bypass", applyFps);
    listenForSettingChanges<int64_t>("fps-value", [applyFps](int64_t) { applyFps(true); });
}
