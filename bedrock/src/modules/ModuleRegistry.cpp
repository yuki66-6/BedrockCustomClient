#include "modules/ModuleRegistry.h"

#include "core/ModuleManager.h"
#include "modules/combat/CombatModules.h"
#include "modules/combat/ReachLabModule.h"
#include "modules/movement/MovementModules.h"
#include "modules/player/PlayerModules.h"
#include "modules/render/RenderModules.h"

namespace bcc {

void registerAllModules(ModuleManager& m) {
    // Combat
    auto* reach = m.emplace<ReachLabModule>();
    m.emplace<CpsModule>();
    if (auto* combat = m.emplace<CombatHudModule>()) combat->setSimReachSource([reach] { return reach->simReach(); });
    // Movement
    m.emplace<KeystrokesModule>();
    m.emplace<SprintModule>();
    m.emplace<JumpCounterModule>();
    // Render
    m.emplace<CrosshairModule>();
    m.emplace<InfoBarModule>();
    m.emplace<PingModule>();
    m.emplace<FpsModule>();
    // Player
    m.emplace<SessionStatsModule>();
    m.emplace<PlayerInfoModule>();
}

} // namespace bcc
