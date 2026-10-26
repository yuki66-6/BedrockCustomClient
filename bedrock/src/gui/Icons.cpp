#include "gui/Icons.h"

#include <algorithm>
#include <cmath>

namespace bcc::ui {

void drawIcon(ImDrawList* dl, Icon icon, ImVec2 c, float size, ImU32 col) {
    const float s = size * 0.5f;          // demi-côté
    const float t = std::max(1.2f, size * 0.085f); // épaisseur de trait
    auto P = [&](float x, float y) { return ImVec2{c.x + x * s, c.y + y * s}; };
    constexpr float kPi = 3.14159265f;

    switch (icon) {
    case Icon::Logo: { // flocon à six branches
        for (int i = 0; i < 6; ++i) {
            float a = kPi / 3.f * static_cast<float>(i) - kPi / 2.f;
            ImVec2 d{std::cos(a), std::sin(a)};
            dl->AddLine(c, P(d.x * 0.95f, d.y * 0.95f), col, t);
            ImVec2 m = P(d.x * 0.55f, d.y * 0.55f);
            ImVec2 n{-d.y, d.x};
            dl->AddLine(m, {m.x + (d.x * 0.25f + n.x * 0.25f) * s, m.y + (d.y * 0.25f + n.y * 0.25f) * s}, col, t);
            dl->AddLine(m, {m.x + (d.x * 0.25f - n.x * 0.25f) * s, m.y + (d.y * 0.25f - n.y * 0.25f) * s}, col, t);
        }
        dl->AddCircleFilled(c, t * 1.3f, col);
        break;
    }
    case Icon::Sword: // deux épées croisées
        for (int k = -1; k <= 1; k += 2) {
            float kx = static_cast<float>(k);
            dl->AddLine(P(-0.8f * kx, -0.8f), P(0.55f * kx, 0.55f), col, t * 1.3f);
            dl->AddLine(P(0.3f * kx, 0.75f), P(0.75f * kx, 0.3f), col, t);
            dl->AddLine(P(0.6f * kx, 0.6f), P(0.85f * kx, 0.85f), col, t * 1.3f);
        }
        break;
    case Icon::Target:
        dl->AddCircle(c, s * 0.75f, col, 0, t);
        dl->AddCircleFilled(c, s * 0.22f, col);
        dl->AddLine(P(0, -1), P(0, -0.45f), col, t);
        dl->AddLine(P(0, 1), P(0, 0.45f), col, t);
        dl->AddLine(P(-1, 0), P(-0.45f, 0), col, t);
        dl->AddLine(P(1, 0), P(0.45f, 0), col, t);
        break;
    case Icon::Keyboard:
        dl->AddRect(P(-0.95f, -0.6f), P(0.95f, 0.6f), col, s * 0.15f, t);
        for (int r = 0; r < 2; ++r)
            for (int i = 0; i < 4; ++i) {
                float x = -0.6f + 0.4f * static_cast<float>(i), y = -0.25f + 0.3f * static_cast<float>(r);
                dl->AddRectFilled(P(x - 0.1f, y - 0.08f), P(x + 0.1f, y + 0.08f), col);
            }
        dl->AddRectFilled(P(-0.45f, 0.28f), P(0.45f, 0.38f), col);
        break;
    case Icon::Run:
        dl->AddCircleFilled(P(0.25f, -0.75f), s * 0.17f, col);
        dl->AddLine(P(0.1f, -0.45f), P(-0.15f, 0.15f), col, t * 1.4f);
        dl->AddLine(P(-0.15f, 0.15f), P(0.35f, 0.45f), col, t * 1.2f);
        dl->AddLine(P(0.35f, 0.45f), P(0.25f, 0.95f), col, t * 1.2f);
        dl->AddLine(P(-0.15f, 0.15f), P(-0.55f, 0.85f), col, t * 1.2f);
        dl->AddLine(P(0.05f, -0.3f), P(0.6f, -0.1f), col, t);
        dl->AddLine(P(0.05f, -0.3f), P(-0.55f, -0.2f), col, t);
        break;
    case Icon::Speed:
        dl->PathArcTo(P(0, 0.25f), s * 0.8f, kPi, 2.f * kPi);
        dl->PathStroke(col, 0, t * 1.2f);
        dl->AddLine(P(0, 0.25f), P(0.45f, -0.25f), col, t * 1.3f);
        dl->AddCircleFilled(P(0, 0.25f), t * 1.4f, col);
        break;
    case Icon::Jump:
        dl->AddTriangleFilled(P(0, -0.9f), P(-0.55f, -0.25f), P(0.55f, -0.25f), col);
        dl->AddLine(P(0, -0.3f), P(0, 0.45f), col, t * 1.4f);
        dl->AddLine(P(-0.8f, 0.85f), P(0.8f, 0.85f), col, t * 1.2f);
        break;
    case Icon::Crosshair:
        dl->AddLine(P(0, -0.95f), P(0, -0.3f), col, t * 1.2f);
        dl->AddLine(P(0, 0.95f), P(0, 0.3f), col, t * 1.2f);
        dl->AddLine(P(-0.95f, 0), P(-0.3f, 0), col, t * 1.2f);
        dl->AddLine(P(0.95f, 0), P(0.3f, 0), col, t * 1.2f);
        dl->AddCircle(c, s * 0.6f, col, 0, t * 0.8f);
        break;
    case Icon::Monitor:
        dl->AddRect(P(-0.95f, -0.75f), P(0.95f, 0.45f), col, s * 0.1f, t);
        dl->AddLine(P(0, 0.45f), P(0, 0.8f), col, t);
        dl->AddLine(P(-0.45f, 0.85f), P(0.45f, 0.85f), col, t * 1.2f);
        break;
    case Icon::Chart:
        for (int i = 0; i < 3; ++i) {
            float x = -0.6f + 0.6f * static_cast<float>(i), h = 0.45f + 0.4f * static_cast<float>(i);
            dl->AddRectFilled(P(x - 0.18f, 0.85f - h * 1.2f), P(x + 0.18f, 0.85f), col, s * 0.05f);
        }
        break;
    case Icon::Signal:
        for (int i = 0; i < 4; ++i) {
            float x = -0.7f + 0.47f * static_cast<float>(i), h = 0.35f + 0.42f * static_cast<float>(i);
            dl->AddRectFilled(P(x - 0.13f, 0.85f - h), P(x + 0.13f, 0.85f), col, s * 0.04f);
        }
        break;
    case Icon::Person:
        dl->AddCircle(P(0, -0.4f), s * 0.38f, col, 0, t);
        dl->PathArcTo(P(0, 0.95f), s * 0.8f, kPi * 1.05f, kPi * 1.95f);
        dl->PathStroke(col, 0, t);
        break;
    case Icon::Clock:
        dl->AddCircle(c, s * 0.85f, col, 0, t);
        dl->AddLine(c, P(0, -0.55f), col, t);
        dl->AddLine(c, P(0.4f, 0.15f), col, t);
        break;
    case Icon::Shield:
        dl->PathLineTo(P(0, -0.9f));
        dl->PathLineTo(P(0.75f, -0.6f));
        dl->PathLineTo(P(0.65f, 0.25f));
        dl->PathLineTo(P(0, 0.9f));
        dl->PathLineTo(P(-0.65f, 0.25f));
        dl->PathLineTo(P(-0.75f, -0.6f));
        dl->PathStroke(col, ImDrawFlags_Closed, t);
        break;
    case Icon::Gear: {
        dl->AddCircle(c, s * 0.32f, col, 0, t);
        int teeth = 8;
        for (int i = 0; i < teeth; ++i) {
            float a = 2.f * kPi * static_cast<float>(i) / static_cast<float>(teeth);
            dl->AddLine(P(std::cos(a) * 0.55f, std::sin(a) * 0.55f), P(std::cos(a) * 0.9f, std::sin(a) * 0.9f), col,
                        t * 1.8f);
        }
        dl->AddCircle(c, s * 0.62f, col, 0, t);
        break;
    }
    case Icon::Folder:
        dl->PathLineTo(P(-0.95f, -0.65f));
        dl->PathLineTo(P(-0.3f, -0.65f));
        dl->PathLineTo(P(-0.1f, -0.4f));
        dl->PathLineTo(P(0.95f, -0.4f));
        dl->PathLineTo(P(0.95f, 0.75f));
        dl->PathLineTo(P(-0.95f, 0.75f));
        dl->PathStroke(col, ImDrawFlags_Closed, t);
        break;
    case Icon::Ruler:
        dl->AddRect(P(-0.95f, -0.35f), P(0.95f, 0.35f), col, s * 0.08f, t);
        for (int i = 0; i < 5; ++i) {
            float x = -0.6f + 0.3f * static_cast<float>(i);
            dl->AddLine(P(x, -0.35f), P(x, i % 2 ? -0.05f : 0.1f), col, t * 0.8f);
        }
        break;
    case Icon::Map: // carte pliée
        dl->PathLineTo(P(-0.9f, -0.7f));
        dl->PathLineTo(P(-0.3f, -0.9f));
        dl->PathLineTo(P(0.3f, -0.7f));
        dl->PathLineTo(P(0.9f, -0.9f));
        dl->PathLineTo(P(0.9f, 0.7f));
        dl->PathLineTo(P(0.3f, 0.9f));
        dl->PathLineTo(P(-0.3f, 0.7f));
        dl->PathLineTo(P(-0.9f, 0.9f));
        dl->PathStroke(col, ImDrawFlags_Closed, t);
        dl->AddLine(P(-0.3f, -0.9f), P(-0.3f, 0.7f), col, t * 0.8f);
        dl->AddLine(P(0.3f, -0.7f), P(0.3f, 0.9f), col, t * 0.8f);
        break;
    case Icon::Star: {
        for (int i = 0; i < 10; ++i) {
            float a = kPi / 5.f * static_cast<float>(i) - kPi / 2.f;
            float rr = i % 2 ? 0.42f : 0.95f;
            dl->PathLineTo(P(std::cos(a) * rr, std::sin(a) * rr));
        }
        dl->PathFillConcave(col);
        break;
    }
    case Icon::Home:
        dl->AddTriangleFilled(P(0, -0.95f), P(-0.95f, -0.05f), P(0.95f, -0.05f), col);
        dl->AddRectFilled(P(-0.65f, -0.1f), P(0.65f, 0.9f), col);
        break;
    case Icon::Flag:
        dl->AddLine(P(-0.6f, -0.95f), P(-0.6f, 0.95f), col, t * 1.4f);
        dl->AddTriangleFilled(P(-0.55f, -0.9f), P(0.85f, -0.5f), P(-0.55f, -0.1f), col);
        break;
    case Icon::Skull:
        dl->AddCircleFilled(P(0, -0.15f), s * 0.72f, col);
        dl->AddRectFilled(P(-0.4f, 0.3f), P(0.4f, 0.85f), col);
        break;
    case Icon::Pin:
        dl->AddCircleFilled(P(0, -0.35f), s * 0.55f, col);
        dl->AddTriangleFilled(P(-0.42f, -0.1f), P(0.42f, -0.1f), P(0, 0.95f), col);
        break;
    case Icon::Generic:
    default:
        dl->AddRect(P(-0.7f, -0.7f), P(0.7f, 0.7f), col, s * 0.2f, t);
        dl->AddCircleFilled(c, s * 0.2f, col);
        break;
    }
}

} // namespace bcc::ui
