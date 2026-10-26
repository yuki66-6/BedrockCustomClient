#include "map/BlockColors.h"

#include <cctype>

namespace bcc::map {

BlockColors::BlockColors() {
    struct E {
        const char* name;
        uint8_t r, g, b;
    };
    static constexpr E kVanilla[] = {
        {"grass_block", 106, 170, 64},   {"grass", 106, 170, 64},           {"dirt", 134, 96, 67},
        {"coarse_dirt", 119, 85, 59},    {"podzol", 106, 76, 40},           {"mycelium", 111, 99, 107},
        {"dirt_with_roots", 130, 94, 66}, {"mud", 60, 57, 60},              {"farmland", 110, 75, 45},
        {"grass_path", 148, 122, 65},    {"dirt_path", 148, 122, 65},       {"stone", 125, 125, 125},
        {"cobblestone", 118, 118, 118},  {"deepslate", 80, 80, 82},         {"granite", 153, 114, 99},
        {"diorite", 188, 188, 188},      {"andesite", 136, 136, 136},       {"tuff", 108, 109, 102},
        {"calcite", 223, 224, 220},      {"gravel", 136, 126, 126},         {"sand", 219, 207, 163},
        {"red_sand", 190, 102, 33},      {"sandstone", 216, 203, 155},      {"red_sandstone", 186, 99, 29},
        {"clay", 160, 166, 179},         {"water", 52, 92, 212},            {"flowing_water", 52, 92, 212},
        {"lava", 220, 90, 20},           {"flowing_lava", 220, 90, 20},     {"ice", 145, 183, 253},
        {"packed_ice", 141, 180, 250},   {"blue_ice", 116, 167, 253},       {"snow", 249, 254, 254},
        {"snow_layer", 240, 251, 251},   {"powder_snow", 248, 253, 253},    {"oak_leaves", 72, 125, 40},
        {"spruce_leaves", 52, 86, 52},   {"birch_leaves", 102, 140, 60},    {"jungle_leaves", 60, 130, 30},
        {"acacia_leaves", 90, 125, 40},  {"dark_oak_leaves", 50, 100, 25},  {"mangrove_leaves", 70, 120, 40},
        {"cherry_leaves", 235, 170, 200}, {"azalea_leaves", 90, 120, 45},   {"leaves", 72, 125, 40},
        {"leaves2", 72, 125, 40},        {"cactus", 85, 127, 43},           {"pumpkin", 198, 118, 24},
        {"melon_block", 111, 145, 30},   {"hay_block", 166, 136, 38},       {"bedrock", 85, 85, 85},
        {"obsidian", 21, 18, 30},        {"netherrack", 98, 38, 38},        {"soul_sand", 81, 62, 50},
        {"soul_soil", 75, 57, 46},       {"basalt", 80, 80, 86},            {"blackstone", 42, 36, 41},
        {"end_stone", 219, 222, 158},    {"moss_block", 89, 109, 45},       {"terracotta", 152, 94, 67},
        {"hardened_clay", 152, 94, 67},  {"oak_planks", 162, 130, 78},      {"planks", 162, 130, 78},
        {"oak_log", 109, 85, 50},        {"spruce_log", 58, 37, 16},        {"birch_log", 216, 215, 210},
        {"log", 109, 85, 50},            {"log2", 109, 85, 50},             {"glass", 200, 230, 240},
        {"bricks", 150, 97, 83},         {"brick_block", 150, 97, 83},      {"stone_bricks", 122, 121, 122},
        {"stonebrick", 122, 121, 122},   {"iron_block", 220, 220, 220},     {"gold_block", 246, 208, 61},
        {"diamond_block", 98, 237, 228}, {"emerald_block", 42, 203, 87},    {"redstone_block", 175, 24, 5},
        {"lapis_block", 30, 67, 140},    {"coal_block", 16, 15, 15},        {"quartz_block", 235, 229, 222},
        {"bamboo", 95, 140, 30},         {"sugar_cane", 148, 192, 101},     {"reeds", 148, 192, 101},
        {"kelp", 50, 120, 40},           {"seagrass", 40, 110, 40},         {"lily_pad", 32, 128, 48},
        {"waterlily", 32, 128, 48},      {"tall_grass", 100, 160, 60},      {"tallgrass", 100, 160, 60},
        {"short_grass", 100, 160, 60},   {"fern", 90, 150, 60},             {"sweet_berry_bush", 70, 110, 40},
        {"crimson_nylium", 130, 31, 31}, {"warped_nylium", 43, 114, 101},   {"magma", 142, 63, 31},
        {"prismarine", 99, 156, 151},    {"sculk", 12, 29, 36},             {"mangrove_roots", 74, 59, 38},
    };
    for (auto& e : kVanilla) table_[std::string("minecraft:") + e.name] = rgba(e.r, e.g, e.b);
}

bool BlockColors::isAir(std::string_view name) const {
    return name == "minecraft:air" || name == "minecraft:cave_air" || name == "minecraft:void_air" ||
           name == "minecraft:structure_void" || name == "minecraft:light_block" || name.empty();
}

uint32_t BlockColors::color(std::string_view name) const {
    std::string key(name);
    if (auto it = overrides_.find(key); it != overrides_.end()) return it->second;
    if (auto it = table_.find(key); it != table_.end()) return it->second;
    // Heuristiques par mot-clé (blocs vanilla rares, variantes, blocs de mods).
    auto has = [&](const char* w) { return name.find(w) != std::string_view::npos; };
    if (has("water")) return rgba(52, 92, 212);
    if (has("lava") || has("magma")) return rgba(220, 90, 20);
    if (has("leaves") || has("vine")) return rgba(70, 120, 40);
    if (has("grass") || has("moss") || has("fern") || has("bush")) return rgba(100, 160, 60);
    if (has("flower") || has("tulip") || has("rose") || has("poppy") || has("dandelion")) return rgba(200, 80, 120);
    if (has("snow") || has("powder")) return rgba(245, 252, 252);
    if (has("ice")) return rgba(145, 183, 253);
    if (has("sand")) return rgba(216, 203, 155);
    if (has("log") || has("wood") || has("stem") || has("planks") || has("fence") || has("door") || has("stairs"))
        return rgba(140, 105, 65);
    if (has("ore")) return rgba(130, 130, 130);
    if (has("deepslate") || has("blackstone") || has("basalt")) return rgba(70, 70, 75);
    if (has("stone") || has("brick") || has("cobble") || has("slab") || has("wall")) return rgba(125, 125, 125);
    if (has("dirt") || has("mud") || has("soil") || has("path")) return rgba(134, 96, 67);
    if (has("terracotta") || has("clay")) return rgba(152, 94, 67);
    if (has("glass")) return rgba(200, 230, 240);
    if (has("wool") || has("carpet") || has("concrete")) return rgba(210, 210, 210);
    if (has("nether")) return rgba(98, 38, 38);
    return rgba(150, 150, 150); // inconnu (ex. bloc de mod sans couleur déclarée)
}

bool BlockColors::parseHex(std::string_view text, uint32_t& out) {
    if (text.empty() || text[0] != '#' || (text.size() != 7 && text.size() != 9)) return false;
    auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    };
    uint8_t v[4] = {0, 0, 0, 255};
    for (size_t i = 0; i < (text.size() - 1) / 2; ++i) {
        int hi = hex(text[1 + i * 2]), lo = hex(text[2 + i * 2]);
        if (hi < 0 || lo < 0) return false;
        v[i] = static_cast<uint8_t>(hi * 16 + lo);
    }
    out = rgba(v[0], v[1], v[2], v[3]);
    return true;
}

} // namespace bcc::map
