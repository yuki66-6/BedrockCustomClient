#include "core/ConfigManager.h"

#include "core/Setting.h"
#include "utils/Log.h"
#include "utils/Strings.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <format>
#include <fstream>
#include <sstream>

namespace bcc {
namespace {

std::string timestamp() {
    SYSTEMTIME t;
    GetLocalTime(&t);
    return std::format("{:04}{:02}{:02}-{:02}{:02}{:02}", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
}

std::filesystem::path u8path(std::string_view s) { return std::filesystem::path(toWide(s)); }

} // namespace

ConfigManager::ConfigManager(std::filesystem::path dir) : dir_(std::move(dir)) {}

void ConfigManager::addSection(ConfigSection section) { sections_.push_back(std::move(section)); }

bool ConfigManager::isValidProfileName(std::string_view name, std::string* why) {
    auto fail = [&](const char* msg) {
        if (why) *why = msg;
        return false;
    };
    if (name.empty()) return fail("Le nom est vide.");
    if (name.size() > 32) return fail("32 caractères maximum.");
    if (name.front() == ' ' || name.back() == ' ' || name.back() == '.' || name.front() == '.')
        return fail("Pas d'espace ni de point au début ou à la fin.");
    for (unsigned char c : name) {
        if (c < 32 || std::string_view("<>:\"/\\|?*").find(static_cast<char>(c)) != std::string_view::npos)
            return fail("Caractères interdits : < > : \" / \\ | ? *");
    }
    std::string upper;
    for (unsigned char c : name) upper += static_cast<char>(std::toupper(c));
    static const char* reserved[] = {"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6",
                                     "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7",
                                     "LPT8", "LPT9", "CLIENT"};
    for (auto r : reserved)
        if (upper == r) return fail("Nom réservé par Windows.");
    return true;
}

std::filesystem::path ConfigManager::profilePath(std::string_view name) const {
    return dir_ / L"profiles" / (u8path(name).wstring() + L".json");
}

bool ConfigManager::profileExists(std::string_view name) const {
    std::error_code ec;
    return std::filesystem::exists(profilePath(name), ec);
}

std::vector<std::string> ConfigManager::listProfiles() const {
    std::vector<std::string> out;
    std::error_code ec;
    for (auto& entry : std::filesystem::directory_iterator(dir_ / L"profiles", ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != L".json") continue;
        std::wstring stem = entry.path().stem().wstring();
        if (stem.find(L".corrupt-") != std::wstring::npos) continue;
        out.push_back(toUtf8(stem));
    }
    std::sort(out.begin(), out.end());
    return out;
}

namespace {

// Pointeur JSON simple « a/b » vers un sous-objet (nullptr si absent).
const Json* atPath(const Json& doc, const std::string& path) {
    const Json* cur = &doc;
    size_t start = 0;
    while (start <= path.size()) {
        size_t end = path.find('/', start);
        std::string part = path.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!cur->is_object()) return nullptr;
        auto it = cur->find(part);
        if (it == cur->end()) return nullptr;
        cur = &*it;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return cur;
}

void setPath(Json& doc, const std::string& path, Json value) {
    Json* cur = &doc;
    size_t start = 0;
    while (true) {
        size_t end = path.find('/', start);
        std::string part = path.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (end == std::string::npos) {
            (*cur)[part] = std::move(value);
            return;
        }
        if (!(*cur)[part].is_object()) (*cur)[part] = Json::object();
        cur = &(*cur)[part];
        start = end + 1;
    }
}

} // namespace

bool ConfigManager::isProfileDocument(const Json& doc) {
    if (!doc.is_object()) return false;
    if (doc.value("format", Json()).is_string()) return doc.value("format", std::string()) == "umc-profile";
    return doc.contains("sections") && doc["sections"].is_object(); // v1 (Bedrock Custom Client 1.0)
}

Json ConfigManager::buildDocument() const {
    Json doc = {{"format", "umc-profile"}, {"formatVersion", kFormatVersion}, {"name", active_},
                {"edition", "bedrock"}, {"clientVersion", BCC_VERSION}};
    for (auto& s : sections_)
        if (s.save) setPath(doc, s.path.empty() ? s.key : s.path, s.save());
    return doc;
}

std::vector<std::string> ConfigManager::applyDocument(const Json& doc, bool* migrated) {
    std::vector<std::string> warnings;
    if (migrated) *migrated = false;
    if (!isProfileDocument(doc)) return {"document invalide (ni profil umc-profile v2, ni profil v1)"};
    bool v1 = !doc["format"].is_string();
    const Json* v1Sections = nullptr;
    if (v1) {
        v1Sections = &doc["sections"];
        if (migrated) *migrated = true;
        if (doc["format"].is_number() && doc["format"].get<double>() > 1)
            warnings.push_back(std::format("format de profil inconnu ({}) lu comme un profil v1", doc["format"].dump()));
    } else if (doc.value("formatVersion", 0) > kFormatVersion) {
        warnings.push_back(std::format("profil créé par une version plus récente (format {})", doc.value("formatVersion", 0)));
    }
    for (auto& s : sections_) {
        const Json* value = nullptr;
        if (v1) {
            auto it = v1Sections->find(s.key);
            if (it != v1Sections->end()) value = &*it;
        } else {
            value = atPath(doc, s.path.empty() ? s.key : s.path);
        }
        // Le profil décrit un état complet : section remise à zéro avant application.
        if (s.reset) s.reset();
        if (value && s.load) {
            auto w = s.load(*value);
            warnings.insert(warnings.end(), w.begin(), w.end());
        }
    }
    return warnings;
}

void ConfigManager::resetAll() {
    for (auto& s : sections_)
        if (s.reset) s.reset();
}

OpResult ConfigManager::readJson(const std::filesystem::path& file, Json& out) const {
    std::ifstream f(file, std::ios::binary);
    if (!f) return {false, "fichier illisible"};
    std::stringstream ss;
    ss << f.rdbuf();
    out = Json::parse(ss.str(), nullptr, false, true);
    if (out.is_discarded()) return {false, "JSON invalide"};
    return {};
}

OpResult ConfigManager::writeAtomic(const std::filesystem::path& file, const std::string& text) const {
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    auto tmp = file;
    tmp += L".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return {false, "écriture impossible : " + toUtf8(tmp.wstring())};
        f << text;
        f.flush();
        if (!f) return {false, "écriture incomplète"};
    }
    if (!MoveFileExW(tmp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DWORD err = GetLastError();
        std::filesystem::remove(tmp, ec);
        return {false, "remplacement impossible : " + win32ErrorMessage(err)};
    }
    return {};
}

void ConfigManager::writeClientState() const {
    writeAtomic(dir_ / L"client.json", Json{{"active_profile", active_}}.dump(2));
}

void ConfigManager::markClean() {
    savedRevision_ = seenRevision_ = Setting::revision();
}

OpResult ConfigManager::save() {
    OpResult r = writeAtomic(profilePath(active_), buildDocument().dump(2));
    if (r.ok) {
        markClean();
        writeClientState();
        r.message = std::format("Profil « {} » enregistré", active_);
    } else {
        log::error("Sauvegarde du profil {} : {}", active_, r.message);
    }
    return r;
}

OpResult ConfigManager::load(const std::string& name) {
    std::string why;
    if (!isValidProfileName(name, &why)) return {false, why};
    auto file = profilePath(name);
    Json doc;
    OpResult r = readJson(file, doc);
    if (!r.ok) {
        std::error_code ec;
        if (!std::filesystem::exists(file, ec)) return {false, std::format("Profil « {} » introuvable", name)};
        // Fichier corrompu : on le met de côté et on repart des valeurs par défaut.
        auto backup = file.parent_path() / (u8path(name).wstring() + L".corrupt-" + toWide(timestamp()) + L".json");
        std::filesystem::rename(file, backup, ec);
        log::error("Profil {} corrompu ({}), copie : {}", name, r.message, toUtf8(backup.wstring()));
        resetAll();
        active_ = name;
        save();
        return {false, std::format("Profil « {} » corrompu : sauvegardé sous {} et réinitialisé", name,
                                   toUtf8(backup.filename().wstring()))};
    }
    bool migrated = false;
    warnings_ = applyDocument(doc, &migrated);
    for (auto& w : warnings_) log::warn("Profil {} : {}", name, w);
    active_ = name;
    if (migrated) {
        // Copie de l'ancien fichier avant réécriture au format v2.
        std::error_code ec;
        auto backupDir = dir_ / L"profiles" / L".backup-v1";
        std::filesystem::create_directories(backupDir, ec);
        std::filesystem::copy_file(file, backupDir / file.filename(), std::filesystem::copy_options::overwrite_existing, ec);
        log::info("Profil {} migré du format v1 vers v2 (copie : {})", name, toUtf8((backupDir / file.filename()).wstring()));
        save();
    }
    markClean();
    writeClientState();
    if (!warnings_.empty())
        return {true, std::format("Profil « {} » chargé ({} avertissement(s), voir le journal)", name, warnings_.size())};
    return {true, std::format("Profil « {} » chargé", name)};
}

OpResult ConfigManager::loadStartupProfile() {
    std::string name = "default";
    Json state;
    if (readJson(dir_ / L"client.json", state).ok && state.is_object()) {
        std::string stored = state.value("active_profile", "default");
        if (isValidProfileName(stored) && profileExists(stored)) name = stored;
    }
    if (!profileExists(name)) {
        active_ = name;
        resetAll();
        OpResult r = save();
        if (r.ok) r.message = std::format("Profil « {} » créé", name);
        return r;
    }
    return load(name);
}

OpResult ConfigManager::create(const std::string& name) {
    std::string why;
    if (!isValidProfileName(name, &why)) return {false, why};
    if (profileExists(name)) return {false, std::format("Le profil « {} » existe déjà", name)};
    if (dirty()) save();
    resetAll();
    active_ = name;
    OpResult r = save();
    if (r.ok) r.message = std::format("Profil « {} » créé (valeurs par défaut)", name);
    return r;
}

OpResult ConfigManager::duplicate(const std::string& name) {
    std::string why;
    if (!isValidProfileName(name, &why)) return {false, why};
    if (profileExists(name)) return {false, std::format("Le profil « {} » existe déjà", name)};
    if (dirty()) save();
    std::string from = active_;
    active_ = name;
    OpResult r = save();
    if (r.ok) r.message = std::format("Profil « {} » créé à partir de « {} »", name, from);
    else active_ = from;
    return r;
}

OpResult ConfigManager::remove(const std::string& name) {
    if (!profileExists(name)) return {false, std::format("Profil « {} » introuvable", name)};
    std::error_code ec;
    auto trash = dir_ / L".trash";
    std::filesystem::create_directories(trash, ec);
    auto dest = trash / (u8path(name).wstring() + L"-" + toWide(timestamp()) + L".json");
    std::filesystem::rename(profilePath(name), dest, ec);
    if (ec) return {false, "Suppression impossible : " + ec.message()};
    log::info("Profil {} déplacé vers {}", name, toUtf8(dest.wstring()));
    std::string msg = std::format("Profil « {} » déplacé dans la corbeille (.trash)", name);
    if (name == active_) {
        auto remaining = listProfiles();
        if (remaining.empty()) {
            resetAll();
            active_ = "default";
            save();
        } else {
            load(remaining.front());
        }
        msg += std::format(" — profil actif : « {} »", active_);
    }
    return {true, msg};
}

OpResult ConfigManager::exportTo(const std::filesystem::path& file) const {
    OpResult r = writeAtomic(file, buildDocument().dump(2));
    if (r.ok) r.message = "Réglages exportés vers " + toUtf8(file.filename().wstring());
    return r;
}

OpResult ConfigManager::importFrom(const std::filesystem::path& file, std::string name) {
    Json doc;
    OpResult r = readJson(file, doc);
    if (!r.ok) return {false, "Import impossible : " + r.message};
    if (!isProfileDocument(doc)) return {false, "Ce fichier n'est pas un profil Universal Minecraft Client"};
    if (name.empty()) name = toUtf8(file.stem().wstring());
    std::string why;
    if (!isValidProfileName(name, &why)) return {false, why};
    std::string base = name;
    for (int i = 2; profileExists(name); ++i) name = std::format("{} ({})", base, i);
    if (dirty()) save();
    warnings_ = applyDocument(doc);
    active_ = name;
    r = save();
    if (r.ok)
        r.message = std::format("Profil « {} » importé{}", name,
                                warnings_.empty() ? "" : std::format(" ({} avertissement(s))", warnings_.size()));
    return r;
}

std::vector<BuiltinProfile> ConfigManager::listBuiltinProfiles(const std::filesystem::path& dir) {
    std::vector<BuiltinProfile> out;
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator(dir, ec)) {
        if (!e.is_regular_file() || e.path().extension() != L".json") continue;
        std::ifstream f(e.path(), std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        Json j = Json::parse(ss.str(), nullptr, false, true);
        if (!isProfileDocument(j)) continue;
        BuiltinProfile p;
        p.file = e.path();
        p.name = j.value("name", toUtf8(e.path().stem().wstring()));
        p.description = j.value("description", std::string());
        out.push_back(std::move(p));
    }
    std::sort(out.begin(), out.end(), [](const BuiltinProfile& a, const BuiltinProfile& b) { return a.name < b.name; });
    return out;
}

OpResult ConfigManager::useBuiltin(const BuiltinProfile& profile) { return importFrom(profile.file, profile.name); }

void ConfigManager::setAutosave(bool enabled, double delaySeconds) {
    autosave_ = enabled;
    autosaveDelay_ = delaySeconds;
}

bool ConfigManager::dirty() const { return Setting::revision() != savedRevision_; }

void ConfigManager::update(double now) {
    uint64_t rev = Setting::revision();
    if (rev != seenRevision_) {
        seenRevision_ = rev;
        lastChange_ = now;
    }
    if (autosave_ && rev != savedRevision_ && now - lastChange_ >= autosaveDelay_) save();
}

} // namespace bcc
