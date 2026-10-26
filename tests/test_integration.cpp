#include "TestFramework.h"

#include "integration/GameIntegration.h"
#include "integration/InputMonitor.h"
#include "integration/InputState.h"
#include "integration/RakNetPing.h"
#include "sim/ReachSim.h"
#include "gui/TextureManager.h"
#include "utils/Bitmap.h"

#include <windows.h>

#include <cstring>

using namespace bcc;

TEST_CASE("CPS : fenêtre glissante d'une seconde et pic") {
    CpsCounter c;
    for (int i = 0; i < 10; ++i) c.add(1.0 + i * 0.05); // 10 clics en 0,45 s
    CHECK(c.count(1.5) == 10);
    CHECK(c.count(2.04) == 9); // clics de 1,05 à 1,45 s encore dans la fenêtre
    CHECK(c.count(2.42) == 1); // seul 1,45 s reste
    CHECK(c.count(3.0) == 0);
    CHECK(c.peak() == 10);
    CHECK(c.total() == 10);
    c.reset();
    CHECK(c.total() == 0);
}

TEST_CASE("InputState : touches, répétition et clics en jeu seulement") {
    InputState s;
    CHECK(!s.onKey('W', true, 0, true));
    CHECK(s.onKey('W', true, 0.1, true)); // auto-répétition
    CHECK(s.keyDown('W'));
    s.onKey('W', false, 0.2, true);
    CHECK(!s.keyDown('W'));
    CHECK(s.keyPresses() == 1);

    s.onMouseButton(MouseButton::Left, true, 1.0, false); // hors jeu : non compté
    s.onMouseButton(MouseButton::Left, false, 1.05, false);
    CHECK(s.cps(MouseButton::Left, 1.1) == 0);
    s.onMouseButton(MouseButton::Left, true, 1.2, true);
    s.onMouseButton(MouseButton::Left, false, 1.25, true);
    s.onMouseButton(MouseButton::Right, true, 1.3, true);
    CHECK(s.cps(MouseButton::Left, 1.3) == 1);
    CHECK(s.cps(MouseButton::Right, 1.3) == 1);
    s.setCountOnlyInGame(false);
    s.onMouseButton(MouseButton::Left, true, 1.4, false);
    CHECK(s.cps(MouseButton::Left, 1.4) == 2);
    s.releaseAll();
    CHECK(!s.mouseDown(MouseButton::Right));
}

TEST_CASE("Raw Input : normalisation gauche/droite") {
    CHECK(InputMonitor::normalizeVk(VK_SHIFT, 0x2A, false) == VK_LSHIFT);
    CHECK(InputMonitor::normalizeVk(VK_SHIFT, 0x36, false) == VK_RSHIFT);
    CHECK(InputMonitor::normalizeVk(VK_CONTROL, 0x1D, false) == VK_LCONTROL);
    CHECK(InputMonitor::normalizeVk(VK_CONTROL, 0x1D, true) == VK_RCONTROL);
    CHECK(InputMonitor::normalizeVk(VK_MENU, 0x38, true) == VK_RMENU);
    CHECK(InputMonitor::normalizeVk('Z', 0x11, false) == 'Z');
}

TEST_CASE("Détection du jeu : nom de l'exécutable") {
    CHECK(GameIntegration::isMinecraftExe(L"C:\\XboxGames\\Minecraft for Windows\\Content\\Minecraft.Windows.exe"));
    CHECK(GameIntegration::isMinecraftExe(L"minecraft.windows.exe"));
    CHECK(!GameIntegration::isMinecraftExe(L"C:\\Program Files (x86)\\Minecraft Launcher\\MinecraftLauncher.exe"));
    CHECK(!GameIntegration::isMinecraftExe(L""));
}

TEST_CASE("RakNet : ping construit et pong analysé") {
    auto ping = raknet::buildUnconnectedPing(0x0102030405060708LL, 0xAABBCCDDEEFF0011ULL);
    REQUIRE(ping.size() == 33);
    CHECK(ping[0] == 0x01);
    CHECK(ping[1] == 0x01 && ping[8] == 0x08);
    CHECK(ping[9] == 0x00 && ping[10] == 0xff && ping[24] == 0x78);
    CHECK(ping[25] == 0xAA && ping[32] == 0x11);

    std::string motd = "MCPE;The Hive;800;1.21.0;12345;100000;123;Hub;Survival";
    std::vector<uint8_t> pong = {0x1c};
    for (int i = 0; i < 8; ++i) pong.push_back(ping[1 + i]); // temps renvoyé
    for (int i = 0; i < 8; ++i) pong.push_back(static_cast<uint8_t>(i));
    pong.insert(pong.end(), ping.begin() + 9, ping.begin() + 25); // magic
    pong.push_back(static_cast<uint8_t>(motd.size() >> 8));
    pong.push_back(static_cast<uint8_t>(motd.size() & 0xff));
    pong.insert(pong.end(), motd.begin(), motd.end());
    raknet::PongInfo info;
    REQUIRE(raknet::parseUnconnectedPong(pong.data(), pong.size(), info));
    CHECK(info.timeMs == 0x0102030405060708LL);
    CHECK(info.motd == "The Hive");
    CHECK(info.version == "1.21.0");
    CHECK(info.players == 12345);
    CHECK(info.maxPlayers == 100000);
    CHECK(!raknet::parseUnconnectedPong(pong.data(), 20, info)); // tronqué
    pong[20] ^= 0xff;
    CHECK(!raknet::parseUnconnectedPong(pong.data(), pong.size(), info)); // magic invalide
}

TEST_CASE("Reach Lab : géométrie rayon / hitbox") {
    sim::Aabb box{{-0.3f, 0, 2.7f}, {0.3f, 1.8f, 3.3f}};
    float t = 0;
    CHECK(sim::rayAabb({0, 1.62f, 0}, {0, 0, 1}, box, t));
    CHECK_NEAR(t, 2.7, 1e-4);
    CHECK(!sim::rayAabb({0, 1.62f, 0}, {0, 0, -1}, box, t)); // derrière
    CHECK(!sim::rayAabb({0, 3.f, 0}, {0, 0, 1}, box, t));    // au-dessus
    CHECK_NEAR(sim::distanceToAabb({0, 1, 0}, box), 2.7, 1e-4);
    CHECK_NEAR(sim::distanceToAabb({0, 1, 3}, box), 0.0, 1e-6);
    auto d = sim::lookDirection(0, 0);
    CHECK_NEAR(d.z, 1.0, 1e-6);
    d = sim::lookDirection(90, 0); // yaw 90 = -X dans la convention Minecraft
    CHECK_NEAR(d.x, -1.0, 1e-5);
}

TEST_CASE("Reach Lab : portée simulée vs vanilla") {
    sim::ReachConfig cfg; // 3,0 blocs
    CHECK(sim::evaluateAtDistance(cfg, 2.5f).inReach);
    CHECK(!sim::evaluateAtDistance(cfg, 3.6f).inReach);
    float vanillaMax = sim::maxHitDistance(cfg);
    CHECK(vanillaMax > 3.0f); // la hitbox commence 0,3 bloc avant le centre
    CHECK(vanillaMax < 3.4f);
    cfg.reach = 4.0f;
    CHECK(sim::evaluateAtDistance(cfg, 3.6f).inReach);
    CHECK(sim::maxHitDistance(cfg) > vanillaMax + 0.9f);
    cfg.reach = 3.0f;
    cfg.hitboxExpand = 0.1f;
    CHECK(sim::maxHitDistance(cfg) > vanillaMax);

    sim::ReachConfig base;
    float perfect = sim::hitRate(base, 2.5f, 0.f, 200, 1);
    CHECK(perfect == 1.f);
    float shaky = sim::hitRate(base, 2.5f, 4.f, 2000, 7);
    CHECK(shaky < 1.f);
    CHECK(shaky == sim::hitRate(base, 2.5f, 4.f, 2000, 7)); // déterministe
    CHECK(sim::hitRate(base, 6.f, 0.f, 100, 1) == 0.f);
}

TEST_CASE("Textures : décodage d'une image utilisateur (WIC)") {
    bcctest::TempDir dir;
    // Image 4x2 : moitié opaque rouge, moitié transparente.
    std::vector<uint8_t> px(4 * 2 * 4, 0);
    for (int i = 0; i < 4; ++i) {
        px[i * 4 + 2] = 255;
        px[i * 4 + 3] = 255;
    }
    auto file = dir.path() / "test.bmp";
    REQUIRE(writePremultipliedBmp(file, px.data(), 4, 2, 16));
    std::vector<unsigned char> rgba;
    int w = 0, h = 0;
    std::string err;
    REQUIRE(TextureManager::decode(file, rgba, w, h, &err));
    CHECK(w == 4);
    CHECK(h == 2);
    CHECK(rgba[0] == 255); // R du premier pixel (ligne du haut, rouge)
    CHECK(!TextureManager::decode(dir.path() / "absent.png", rgba, w, h, &err));
    CHECK(!err.empty());
    auto list = TextureManager::listImages(dir.path());
    CHECK(list.size() == 1);
}
