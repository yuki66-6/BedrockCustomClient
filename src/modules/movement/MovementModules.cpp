#include "modules/movement/MovementModules.h"

#include "core/Events.h"
#include "gui/ThemeManager.h"
#include "gui/Widgets.h"
#include "integration/TelemetryProvider.h"
#include "modules/HudDraw.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <format>

namespace bcc {

// ---------------------------------------------------------------------------
// Keystrokes
// ---------------------------------------------------------------------------

std::array<int, 4> KeystrokesModule::detectMovementKeys() {
    // Codes de balayage des positions physiques W, A, S, D.
    std::array<int, 4> keys{hud::layoutKey(0x11), hud::layoutKey(0x1E), hud::layoutKey(0x1F), hud::layoutKey(0x20)};
    const int fallback[4] = {'W', 'A', 'S', 'D'};
    for (int i = 0; i < 4; ++i)
        if (!keys[static_cast<size_t>(i)]) keys[static_cast<size_t>(i)] = fallback[i];
    return keys;
}

KeystrokesModule::KeystrokesModule()
    : Module("keystrokes", "Keystrokes", "Affiche tes touches de déplacement et tes clics en direct.",
             Category::Movement, true) {
    auto keys = detectMovementKeys();
    style_ = add<EnumSetting>("style", "Style", "Apparence des touches", std::vector<std::string>{"Moderne", "Contour", "Minimal"}, 0);
    showMouse_ = add<BoolSetting>("show_mouse", "Clics souris", "Affiche LMB / RMB", true);
    showCps_ = add<BoolSetting>("show_cps", "CPS sur les clics", "Affiche les CPS sous LMB / RMB", true);
    showSpace_ = add<BoolSetting>("show_space", "Barre espace", "Affiche la touche de saut", false);
    pressed_ = add<ColorSetting>("pressed_color", "Couleur d'appui", "Couleur d'une touche enfoncée", Color{0.27f, 0.54f, 1.f, 0.9f});
    layout_ = add<EnumSetting>("layout", "Disposition", "Touches de déplacement",
                               std::vector<std::string>{"Auto (clavier actuel)", "QWERTY (WASD)", "AZERTY (ZQSD)", "Personnalisée"}, 0);
    forward_ = add<KeySetting>("key_forward", "Avancer", "", keys[0]);
    left_ = add<KeySetting>("key_left", "Gauche", "", keys[1]);
    back_ = add<KeySetting>("key_back", "Reculer", "", keys[2]);
    right_ = add<KeySetting>("key_right", "Droite", "", keys[3]);
    jump_ = add<KeySetting>("key_jump", "Sauter", "", VK_SPACE);
    for (auto* k : {forward_, left_, back_, right_}) k->setVisibleIf([this] { return layout_->get() == 3; });
    layout_->setOnChange([this] {
        if (!loadingSettings()) applyLayout(layout_->get());
    });
}

void KeystrokesModule::applyLayout(int layout) {
    std::array<int, 4> keys;
    switch (layout) {
    case 0: keys = detectMovementKeys(); break;
    case 1: keys = {'W', 'A', 'S', 'D'}; break;
    case 2: keys = {'Z', 'Q', 'S', 'D'}; break;
    default: return; // personnalisée : on garde les touches choisies
    }
    forward_->set(keys[0]);
    left_->set(keys[1]);
    back_->set(keys[2]);
    right_->set(keys[3]);
}

void KeystrokesModule::draw(TelemetryProvider* telemetry, double now, bool preview) {
    float dt = lastNow_ > 0 ? static_cast<float>(now - lastNow_) : 0.f;
    lastNow_ = now;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float fs = ImGui::GetFontSize();
    float k = fs * 2.25f, gap = fs * 0.22f;
    float r = std::min(k * 0.22f, ui::theme().rounding() + 2.f);
    ImVec2 o = ImGui::GetCursorScreenPos();
    float totalW = k * 3.f + gap * 2.f;
    float alpha = ImGui::GetStyle().Alpha;

    // Aperçu dans la carte : animation de démonstration (clairement factice, pas une donnée).
    auto down = [&](int vk, int demoIndex) {
        if (preview) return std::fmod(now * 1.3 + demoIndex * 0.37, 2.0) < 0.35;
        return telemetry && telemetry->input().keyDown(vk);
    };
    auto mouse = [&](MouseButton b, int demoIndex) {
        if (preview) return std::fmod(now * 2.1 + demoIndex * 0.5, 1.0) < 0.25;
        return telemetry && telemetry->input().mouseDown(b);
    };
    ImVec4 pressed = hud::col(pressed_->get());
    int style = style_->get();
    auto keyBox = [&](ImVec2 p, ImVec2 size, const std::string& label, const std::string& sub, bool isDown, int idx) {
        float& a = anim_[static_cast<size_t>(idx)];
        a = hud::approach(a, isDown ? 1.f : 0.f, dt, 22.f);
        ImVec2 q{p.x + size.x, p.y + size.y};
        ImVec4 base{0.03f, 0.04f, 0.07f, style == 2 ? 0.f : 0.62f};
        ImVec4 bg = hud::mix(base, pressed, a);
        if (style != 1 || a > 0.01f) dl->AddRectFilled(p, q, ImGui::ColorConvertFloat4ToU32({bg.x, bg.y, bg.z, bg.w * alpha}), r);
        if (style == 1 || style == 0)
            dl->AddRect(p, q, ImGui::ColorConvertFloat4ToU32({1, 1, 1, (style == 1 ? 0.55f : 0.08f) * alpha}), r, 1.f);
        ImU32 tc = ImGui::ColorConvertFloat4ToU32(hud::mix({1, 1, 1, alpha}, {1, 1, 1, alpha}, a));
        if (sub.empty()) {
            hud::centeredText(dl, p, q, tc, label.c_str());
        } else {
            float mid = p.y + size.y * 0.42f;
            hud::centeredText(dl, p, {q.x, mid + fs * 0.35f}, tc, label.c_str());
            hud::centeredText(dl, {p.x, mid + fs * 0.3f}, q, ImGui::ColorConvertFloat4ToU32({0.85f, 0.9f, 1.f, 0.85f * alpha}),
                              sub.c_str(), fs * 0.72f);
        }
    };

    float y = o.y;
    keyBox({o.x + k + gap, y}, {k, k}, hud::shortKeyName(forward_->get()), "", down(forward_->get(), 0), 0);
    y += k + gap;
    keyBox({o.x, y}, {k, k}, hud::shortKeyName(left_->get()), "", down(left_->get(), 1), 1);
    keyBox({o.x + k + gap, y}, {k, k}, hud::shortKeyName(back_->get()), "", down(back_->get(), 2), 2);
    keyBox({o.x + (k + gap) * 2.f, y}, {k, k}, hud::shortKeyName(right_->get()), "", down(right_->get(), 3), 3);
    y += k + gap;
    if (showMouse_->get()) {
        float mw = (totalW - gap) * 0.5f, mh = showCps_->get() ? k * 1.05f : k * 0.8f;
        std::string lc, rc;
        if (showCps_->get()) {
            int l = preview ? 12 : (telemetry ? telemetry->input().cps(MouseButton::Left, now) : 0);
            int rr = preview ? 8 : (telemetry ? telemetry->input().cps(MouseButton::Right, now) : 0);
            lc = std::format("{} CPS", l);
            rc = std::format("{} CPS", rr);
        }
        keyBox({o.x, y}, {mw, mh}, "LMB", lc, mouse(MouseButton::Left, 4), 4);
        keyBox({o.x + mw + gap, y}, {mw, mh}, "RMB", rc, mouse(MouseButton::Right, 5), 5);
        y += mh + gap;
    }
    if (showSpace_->get()) {
        keyBox({o.x, y}, {totalW, k * 0.5f}, "", "", down(jump_->get(), 6), 6);
        float cy = y + k * 0.25f;
        dl->AddLine({o.x + totalW * 0.3f, cy}, {o.x + totalW * 0.7f, cy}, ImGui::ColorConvertFloat4ToU32({1, 1, 1, 0.8f * alpha}), 2.f);
        y += k * 0.5f + gap;
    }
    ImGui::Dummy({totalW, y - o.y - gap});
}

void KeystrokesModule::drawHud(HudContext& ctx) { draw(ctx.telemetry, ctx.now, false); }

void KeystrokesModule::drawCardPreview() {
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 0.72f);
    float w = ImGui::GetFontSize() * (2.25f * 3.f + 0.44f);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.f, (ImGui::GetContentRegionAvail().x - w) * 0.5f));
    draw(nullptr, ImGui::GetTime(), true);
    ImGui::PopFont();
    ImGui::TextDisabled("Aperçu animé (démonstration)");
}

// ---------------------------------------------------------------------------
// Sprint
// ---------------------------------------------------------------------------

SprintModule::SprintModule()
    : Module("sprint", "Sprint Status", "Indique si tu sprintes, d'après ton clavier.", Category::Movement) {
    mode_ = add<EnumSetting>("mode", "Mode", "Comme ton réglage de sprint dans Minecraft",
                             std::vector<std::string>{"Maintenir", "Basculer (toggle)"}, 0);
    text_ = add<StringSetting>("text", "Texte", "Texte affiché pendant le sprint", "Sprint", 32);
    color_ = add<ColorSetting>("color", "Couleur", "Couleur du texte", Color{0.45f, 0.85f, 1.f, 1.f});
    sprintKey_ = add<KeySetting>("sprint_key", "Touche sprint", "", VK_LCONTROL);
    forwardKey_ = add<KeySetting>("forward_key", "Touche avancer", "", KeystrokesModule::detectMovementKeys()[0]);
}

std::string SprintModule::limitation() const {
    return "Déduit des touches : le jeu ne dit pas s'il a réellement accepté le sprint (faim, collision…).";
}

void SprintModule::onInit(ModuleContext& ctx) {
    if (!ctx.bus) return;
    sub_ = Subscription(*ctx.bus, ctx.bus->subscribe<KeyEvent>([this](const KeyEvent& e) {
        if (!e.down || e.repeat || !e.gameFocused) return;
        if (mode_->get() == 1 && e.vk == sprintKey_->get()) toggled_ = !toggled_;
    }));
}

void SprintModule::drawHud(HudContext& ctx) {
    bool forward = ctx.telemetry && ctx.telemetry->input().keyDown(forwardKey_->get());
    bool sprintKey = ctx.telemetry && ctx.telemetry->input().keyDown(sprintKey_->get());
    bool sprinting = forward && (mode_->get() == 1 ? toggled_ : sprintKey);
    if (!forward && mode_->get() == 0) toggled_ = false;
    if (sprinting) {
        ImGui::TextColored(hud::col(color_->get()), "%s", text_->get().c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("(clavier)");
    } else if (ctx.editMode) {
        ImGui::TextDisabled("%s (inactif)", text_->get().c_str());
    } else {
        ImGui::TextDisabled("Marche");
    }
}

// ---------------------------------------------------------------------------
// Vitesse
// ---------------------------------------------------------------------------

SpeedModule::SpeedModule()
    : Module("speed", "Speed", "Vitesse de déplacement du joueur.", Category::Movement) {}

std::string SpeedModule::limitation() const { return TelemetryProvider::kPlayerDataUnavailable; }

void SpeedModule::drawHud(HudContext&) { hud::unavailable("Vitesse", TelemetryProvider::kPlayerDataUnavailable); }

// ---------------------------------------------------------------------------
// Sauts
// ---------------------------------------------------------------------------

JumpCounterModule::JumpCounterModule()
    : Module("jumps", "Jump Counter", "Compte tes appuis sur la touche Saut en jeu.", Category::Movement) {
    jumpKey_ = add<KeySetting>("jump_key", "Touche saut", "", VK_SPACE);
    showSession_ = add<BoolSetting>("show_label", "Libellé", "Affiche « Sauts » devant le nombre", true);
    add<ActionSetting>("reset", "Remettre à zéro", "Remet le compteur à 0", [this] { count_ = 0; });
}

std::string JumpCounterModule::limitation() const {
    return "Compte les appuis sur Saut, pas les sauts confirmés par le jeu (en l'air, dans l'eau…).";
}

void JumpCounterModule::onInit(ModuleContext& ctx) {
    if (!ctx.bus) return;
    sub_ = Subscription(*ctx.bus, ctx.bus->subscribe<KeyEvent>([this](const KeyEvent& e) {
        if (enabled() && e.down && !e.repeat && e.gameFocused && e.vk == jumpKey_->get()) ++count_;
    }));
}

void JumpCounterModule::drawHud(HudContext&) {
    if (showSession_->get()) hud::line("Sauts", std::format("{}", count_));
    else ImGui::Text("%llu", static_cast<unsigned long long>(count_));
}

} // namespace bcc
