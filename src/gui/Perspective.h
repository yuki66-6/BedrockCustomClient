#pragma once

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace bcc {

// Projection perspective d'un plan (le menu) pivoté autour de son centre :
// lacet `yaw` (axe vertical) et tangage `pitch` (axe horizontal), en radians.
// forward() transforme un point du menu « à plat » vers l'écran ; inverse()
// retrouve le point à plat sous la souris (intersection rayon/plan), ce qui
// permet de garder des clics exacts malgré l'inclinaison.
struct Perspective {
    ImVec2 center{0, 0};
    float yaw = 0.f, pitch = 0.f;
    float focal = 1600.f;
    float scale = 1.f;

    ImVec2 forward(ImVec2 p) const {
        float u = p.x - center.x, v = p.y - center.y;
        float cy = std::cos(yaw), sy = std::sin(yaw), cp = std::cos(pitch), sp = std::sin(pitch);
        float x = u * cy, z = u * sy;
        float y2 = v * cp - z * sp, z2 = v * sp + z * cp;
        float k = focal / std::max(1.f, focal + z2) * scale;
        return {center.x + x * k, center.y + y2 * k};
    }

    bool inverse(ImVec2 s, ImVec2* out) const {
        float sx = (s.x - center.x) / scale, sy = (s.y - center.y) / scale;
        float cy = std::cos(yaw), syw = std::sin(yaw), cp = std::cos(pitch), sp = std::sin(pitch);
        // Base orthonormée du plan après rotation.
        float ex[3] = {cy, -syw * sp, syw * cp};
        float ey[3] = {0.f, cp, sp};
        float n[3] = {ex[1] * ey[2] - ex[2] * ey[1], ex[2] * ey[0] - ex[0] * ey[2], ex[0] * ey[1] - ex[1] * ey[0]};
        // Rayon depuis l'œil (0,0,-f) vers le point écran (sx, sy, 0).
        float e[3] = {0.f, 0.f, -focal};
        float d[3] = {sx, sy, focal};
        float nd = n[0] * d[0] + n[1] * d[1] + n[2] * d[2];
        if (std::fabs(nd) < 1e-6f) return false;
        float t = -(n[0] * e[0] + n[1] * e[1] + n[2] * e[2]) / nd;
        if (t <= 0.f) return false;
        float w[3] = {e[0] + t * d[0], e[1] + t * d[1], e[2] + t * d[2]};
        float u = w[0] * ex[0] + w[1] * ex[1] + w[2] * ex[2];
        float v = w[0] * ey[0] + w[1] * ey[1] + w[2] * ey[2];
        *out = {center.x + u, center.y + v};
        return true;
    }

    bool identity() const { return yaw == 0.f && pitch == 0.f && scale == 1.f; }
};

} // namespace bcc
