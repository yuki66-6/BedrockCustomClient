#pragma once

#include "modules/Module.h"
#include "sim/ReachSim.h"

namespace bcc {

// Laboratoire de portée : simulateur local de rayons de collision et de portée.
// N'agit PAS sur le jeu ; distingue clairement distance affichée, portée du
// simulateur et portée autorisée par un serveur.
class ReachLabModule : public Module {
public:
    static constexpr float kVanillaReach = 3.0f; // référence survie (Java/Bedrock, souris-clavier)

    ReachLabModule();
    Icon icon() const override { return Icon::Target; }
    bool hasHud() const override { return true; }
    ImVec2 defaultHudPosition() const override { return {0.40f, 0.72f}; }
    int primarySettingCount() const override { return 3; }
    void drawHud(HudContext& ctx) override;
    void drawCardPreview() override;
    void drawExtraSettings() override;
    std::string limitation() const override;

    sim::ReachConfig config() const;
    float simReach() const { return reach_->get(); }
    float targetDistance() const { return distance_->get(); }

private:
    void drawTopView(ImVec2 size);
    void applyPreset(int index);

    FloatSetting* reach_;
    FloatSetting* distance_;
    EnumSetting* preset_;
    FloatSetting* expand_;
    FloatSetting* jitter_;
    BoolSetting* showVanilla_;
    FloatSetting* presetA_;
    FloatSetting* presetB_;
    FloatSetting* presetC_;
    bool applyingPreset_ = false;
    float lastHitRate_ = -1.f, lastVanillaRate_ = -1.f;
};

} // namespace bcc
