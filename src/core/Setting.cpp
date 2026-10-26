#include "core/Setting.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <format>

namespace bcc {
namespace {
std::atomic<uint64_t> g_revision{1};
}

Setting::Setting(std::string id, std::string name, std::string description)
    : id_(std::move(id)), name_(std::move(name)), description_(std::move(description)) {}

uint64_t Setting::revision() { return g_revision.load(); }
void Setting::bumpRevision() { ++g_revision; }

void Setting::changed() {
    ++g_revision;
    if (onChange_) onChange_();
}

// ---- Bool ----
BoolSetting::BoolSetting(std::string id, std::string name, std::string desc, bool def)
    : Setting(std::move(id), std::move(name), std::move(desc)), value_(def), default_(def) {}

bool BoolSetting::fromJson(const Json& j) {
    if (!j.is_boolean()) return false;
    set(j.get<bool>());
    return true;
}

void BoolSetting::set(bool v) {
    if (v == value_) return;
    value_ = v;
    changed();
}

// ---- Int ----
IntSetting::IntSetting(std::string id, std::string name, std::string desc, int def, int min, int max, std::string format)
    : Setting(std::move(id), std::move(name), std::move(desc)), value_(std::clamp(def, min, max)),
      default_(value_), min_(min), max_(max), format_(std::move(format)) {}

bool IntSetting::fromJson(const Json& j) {
    if (!j.is_number()) return false;
    double d = j.get<double>();
    if (!std::isfinite(d)) return false;
    set(static_cast<int>(std::lround(std::clamp(d, static_cast<double>(min_), static_cast<double>(max_)))));
    return true;
}

void IntSetting::set(int v) {
    v = std::clamp(v, min_, max_);
    if (v == value_) return;
    value_ = v;
    changed();
}

// ---- Float ----
FloatSetting::FloatSetting(std::string id, std::string name, std::string desc, float def, float min, float max,
                           std::string format)
    : Setting(std::move(id), std::move(name), std::move(desc)), value_(std::clamp(def, min, max)),
      default_(value_), min_(min), max_(max), format_(std::move(format)) {}

bool FloatSetting::fromJson(const Json& j) {
    if (!j.is_number()) return false;
    double d = j.get<double>();
    if (!std::isfinite(d)) return false;
    set(static_cast<float>(d));
    return true;
}

void FloatSetting::set(float v) {
    if (!std::isfinite(v)) return;
    v = std::clamp(v, min_, max_);
    if (v == value_) return;
    value_ = v;
    changed();
}

// ---- Enum ----
EnumSetting::EnumSetting(std::string id, std::string name, std::string desc, std::vector<std::string> options, int def)
    : Setting(std::move(id), std::move(name), std::move(desc)), options_(std::move(options)) {
    if (options_.empty()) options_.push_back("-");
    index_ = default_ = std::clamp(def, 0, static_cast<int>(options_.size()) - 1);
}

bool EnumSetting::fromJson(const Json& j) {
    if (j.is_string()) {
        auto it = std::find(options_.begin(), options_.end(), j.get<std::string>());
        if (it == options_.end()) return false;
        set(static_cast<int>(it - options_.begin()));
        return true;
    }
    if (j.is_number_integer()) {
        int i = j.get<int>();
        if (i < 0 || i >= static_cast<int>(options_.size())) return false;
        set(i);
        return true;
    }
    return false;
}

void EnumSetting::set(int index) {
    index = std::clamp(index, 0, static_cast<int>(options_.size()) - 1);
    if (index == index_) return;
    index_ = index;
    changed();
}

// ---- Color ----
ColorSetting::ColorSetting(std::string id, std::string name, std::string desc, Color def)
    : Setting(std::move(id), std::move(name), std::move(desc)), value_(def), default_(def) {}

bool ColorSetting::fromJson(const Json& j) {
    if (!j.is_array() || j.size() != 4) return false;
    float c[4];
    for (size_t i = 0; i < 4; ++i) {
        if (!j[i].is_number()) return false;
        double d = j[i].get<double>();
        if (!std::isfinite(d)) return false;
        c[i] = static_cast<float>(std::clamp(d, 0.0, 1.0));
    }
    set({c[0], c[1], c[2], c[3]});
    return true;
}

void ColorSetting::set(const Color& c) {
    Color clamped{std::clamp(c.r, 0.f, 1.f), std::clamp(c.g, 0.f, 1.f), std::clamp(c.b, 0.f, 1.f),
                  std::clamp(c.a, 0.f, 1.f)};
    if (clamped == value_) return;
    value_ = clamped;
    changed();
}

// ---- Key ----
KeySetting::KeySetting(std::string id, std::string name, std::string desc, int defVk)
    : Setting(std::move(id), std::move(name), std::move(desc)), value_(defVk), default_(defVk) {}

bool KeySetting::fromJson(const Json& j) {
    if (!j.is_number_integer()) return false;
    int vk = j.get<int>();
    if (vk < 0 || vk > 255) return false;
    set(vk);
    return true;
}

void KeySetting::set(int vk) {
    vk = std::clamp(vk, 0, 255);
    if (vk == value_) return;
    value_ = vk;
    changed();
}

// ---- String ----
StringSetting::StringSetting(std::string id, std::string name, std::string desc, std::string def, size_t maxLength)
    : Setting(std::move(id), std::move(name), std::move(desc)), value_(def.substr(0, maxLength)), default_(value_),
      maxLength_(maxLength) {}

bool StringSetting::fromJson(const Json& j) {
    if (!j.is_string()) return false;
    set(j.get<std::string>());
    return true;
}

void StringSetting::set(std::string v) {
    if (v.size() > maxLength_) v.resize(maxLength_);
    if (v == value_) return;
    value_ = std::move(v);
    changed();
}

// ---- Action ----
ActionSetting::ActionSetting(std::string id, std::string name, std::string desc, std::function<void()> action)
    : Setting(std::move(id), std::move(name), std::move(desc)), action_(std::move(action)) {}

// ---- Group ----
Setting* SettingsGroup::findSetting(std::string_view id) const {
    for (auto& s : settings_)
        if (s->id() == id) return s.get();
    return nullptr;
}

Json SettingsGroup::settingsToJson() const {
    Json j = Json::object();
    for (auto& s : settings_)
        if (s->persistent()) j[s->id()] = s->toJson();
    return j;
}

std::vector<std::string> SettingsGroup::settingsFromJson(const Json& j, std::string_view context) {
    std::vector<std::string> warnings;
    if (!j.is_object()) {
        warnings.push_back(std::format("{} : section de réglages invalide (objet attendu)", context));
        return warnings;
    }
    loading_ = true;
    for (auto& [key, value] : j.items()) {
        Setting* s = findSetting(key);
        if (!s) {
            warnings.push_back(std::format("{} : réglage inconnu « {} » ignoré", context, key));
            continue;
        }
        if (!s->fromJson(value))
            warnings.push_back(std::format("{} : valeur invalide pour « {} », valeur conservée", context, key));
    }
    loading_ = false;
    return warnings;
}

void SettingsGroup::resetSettings() {
    loading_ = true;
    for (auto& s : settings_) s->reset();
    loading_ = false;
}

} // namespace bcc
