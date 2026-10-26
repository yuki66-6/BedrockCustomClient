#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace bcc::map {

// Couleur (RGBA 0xAABBGGRR, ordre mémoire R,G,B,A) d'un bloc vu du dessus.
// Table des blocs vanilla courants + heuristiques par mot-clé, et surcharges
// (extensions JSON) pour les blocs d'add-ons / mods.
class BlockColors {
public:
    BlockColors();

    uint32_t color(std::string_view name) const;
    bool isAir(std::string_view name) const;
    void setOverride(const std::string& name, uint32_t rgba) { overrides_[name] = rgba; }
    void clearOverrides() { overrides_.clear(); }
    size_t overrideCount() const { return overrides_.size(); }

    static uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        return static_cast<uint32_t>(r) | (static_cast<uint32_t>(g) << 8) | (static_cast<uint32_t>(b) << 16) |
               (static_cast<uint32_t>(a) << 24);
    }
    // « #RRGGBB » ou « #RRGGBBAA ». Retourne false si invalide.
    static bool parseHex(std::string_view text, uint32_t& out);

private:
    std::unordered_map<std::string, uint32_t> table_;
    std::unordered_map<std::string, uint32_t> overrides_;
};

} // namespace bcc::map
