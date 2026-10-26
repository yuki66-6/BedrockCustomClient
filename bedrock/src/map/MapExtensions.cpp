#include "map/MapExtensions.h"

#include "core/Json.h"
#include "map/BlockColors.h"
#include "utils/Log.h"
#include "utils/Paths.h"
#include "utils/Strings.h"

#include <algorithm>
#include <format>
#include <fstream>
#include <sstream>

namespace bcc::map {
namespace {

int dimensionFromName(const std::string& s) {
    if (s == "nether") return 1;
    if (s == "end" || s == "the_end") return 2;
    return 0;
}

bool readJsonFile(const std::filesystem::path& file, Json& out, std::string& err) {
    std::ifstream f(file, std::ios::binary);
    if (!f) {
        err = "illisible";
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    out = Json::parse(ss.str(), nullptr, false, true);
    if (out.is_discarded() || !out.is_object()) {
        err = "JSON invalide";
        return false;
    }
    return true;
}

} // namespace

std::filesystem::path MapExtensions::folder() { return paths::dataDir() / L"map-extensions"; }

Icon MapExtensions::iconFromName(const std::string& name) {
    if (name == "home") return Icon::Home;
    if (name == "star") return Icon::Star;
    if (name == "flag") return Icon::Flag;
    if (name == "skull") return Icon::Skull;
    return Icon::Pin;
}

const char* MapExtensions::iconName(Icon icon) {
    switch (icon) {
    case Icon::Home: return "home";
    case Icon::Star: return "star";
    case Icon::Flag: return "flag";
    case Icon::Skull: return "skull";
    default: return "pin";
    }
}

std::string MapExtensions::colorToHex(uint32_t c) {
    return std::format("#{:02X}{:02X}{:02X}{:02X}", c & 0xff, (c >> 8) & 0xff, (c >> 16) & 0xff, (c >> 24) & 0xff);
}

std::vector<std::string> MapExtensions::load(BlockColors& colors, const std::filesystem::path& dir) {
    std::vector<std::string> errors;
    markers_.clear();
    areas_.clear();
    files_.clear();
    colors.clearOverrides();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::vector<std::filesystem::path> list;
    for (auto& e : std::filesystem::directory_iterator(dir, ec))
        if (e.is_regular_file() && e.path().extension() == L".json") list.push_back(e.path());
    std::sort(list.begin(), list.end());
    for (auto& file : list) {
        std::string fname = toUtf8(file.filename().wstring());
        if (fname == kLegacyWaypointsFile) continue; // ancien format, migré par WaypointStore
        Json j;
        std::string err;
        if (!readJsonFile(file, j, err)) {
            errors.push_back(fname + " : " + err);
            continue;
        }
        files_.push_back(fname);
        if (auto it = j.find("markers"); it != j.end() && it->is_array()) {
            for (auto& m : *it) {
                if (!m.is_object() || !m.contains("x") || !m.contains("z") || !m["x"].is_number() || !m["z"].is_number()) {
                    errors.push_back(fname + " : repère invalide (x et z numériques requis)");
                    continue;
                }
                MapMarker mk;
                mk.label = m.value("label", std::string("Repère"));
                mk.x = m["x"].get<int>();
                mk.z = m["z"].get<int>();
                mk.icon = iconFromName(m.value("icon", std::string("pin")));
                mk.dimension = dimensionFromName(m.value("dimension", std::string("overworld")));
                uint32_t c;
                if (m.contains("color") && m["color"].is_string() && BlockColors::parseHex(m["color"].get<std::string>(), c))
                    mk.color = c;
                mk.source = fname;
                markers_.push_back(std::move(mk));
            }
        }
        if (auto it = j.find("areas"); it != j.end() && it->is_array()) {
            for (auto& a : *it) {
                if (!a.is_object() || !a.contains("x1") || !a.contains("z1") || !a.contains("x2") || !a.contains("z2")) {
                    errors.push_back(fname + " : zone invalide (x1, z1, x2, z2 requis)");
                    continue;
                }
                MapArea ar;
                ar.label = a.value("label", std::string("Zone"));
                ar.x1 = a.value("x1", 0);
                ar.z1 = a.value("z1", 0);
                ar.x2 = a.value("x2", 0);
                ar.z2 = a.value("z2", 0);
                ar.dimension = dimensionFromName(a.value("dimension", std::string("overworld")));
                uint32_t c;
                if (a.contains("color") && a["color"].is_string() && BlockColors::parseHex(a["color"].get<std::string>(), c))
                    ar.color = c;
                ar.source = fname;
                areas_.push_back(std::move(ar));
            }
        }
        if (auto it = j.find("blockColors"); it != j.end() && it->is_object()) {
            for (auto& [name, value] : it->items()) {
                uint32_t c;
                if (value.is_string() && BlockColors::parseHex(value.get<std::string>(), c)) colors.setOverride(name, c);
                else errors.push_back(fname + " : couleur invalide pour " + name);
            }
        }
    }
    for (auto& e : errors) log::warn("Extension de carte : {}", e);
    log::info("Extensions de carte : {} fichier(s), {} repère(s), {} zone(s), {} couleur(s) de bloc", files_.size(),
              markers_.size(), areas_.size(), colors.overrideCount());
    return errors;
}

} // namespace bcc::map
