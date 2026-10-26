#pragma once

#include "core/Json.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace bcc {

struct Color {
    float r = 1, g = 1, b = 1, a = 1;
    bool operator==(const Color&) const = default;
};

enum class SettingType { Bool, Int, Float, Enum, Color, Key, String, Action };

// Paramètre individuel d'un module ou d'un groupe de réglages. Toute
// modification incrémente un compteur global de révision, ce qui permet au
// ConfigManager de détecter les changements pour la sauvegarde automatique.
class Setting {
public:
    Setting(std::string id, std::string name, std::string description);
    virtual ~Setting() = default;
    Setting(const Setting&) = delete;
    Setting& operator=(const Setting&) = delete;

    virtual SettingType type() const = 0;
    virtual Json toJson() const = 0;
    // Retourne false si la valeur JSON est invalide ; la valeur actuelle est conservée.
    virtual bool fromJson(const Json& j) = 0;
    virtual void reset() = 0;
    virtual bool persistent() const { return true; }

    const std::string& id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& description() const { return description_; }

    // Affichage conditionnel dans l'interface (ex. réglage dépendant d'un mode).
    void setVisibleIf(std::function<bool()> predicate) { visibleIf_ = std::move(predicate); }
    bool visible() const { return !visibleIf_ || visibleIf_(); }

    void setOnChange(std::function<void()> callback) { onChange_ = std::move(callback); }

    static uint64_t revision();
    // Signale un changement d'état persistant qui n'est pas un Setting
    // (position HUD, état d'un module...).
    static void bumpRevision();

protected:
    void changed();

private:
    std::string id_, name_, description_;
    std::function<bool()> visibleIf_;
    std::function<void()> onChange_;
};

class BoolSetting final : public Setting {
public:
    BoolSetting(std::string id, std::string name, std::string desc, bool def);
    SettingType type() const override { return SettingType::Bool; }
    Json toJson() const override { return value_; }
    bool fromJson(const Json& j) override;
    void reset() override { set(default_); }
    bool get() const { return value_; }
    void set(bool v);
    bool defaultValue() const { return default_; }

private:
    bool value_, default_;
};

class IntSetting final : public Setting {
public:
    IntSetting(std::string id, std::string name, std::string desc, int def, int min, int max, std::string format = "%d");
    SettingType type() const override { return SettingType::Int; }
    Json toJson() const override { return value_; }
    bool fromJson(const Json& j) override;
    void reset() override { set(default_); }
    int get() const { return value_; }
    void set(int v);
    int min() const { return min_; }
    int max() const { return max_; }
    const std::string& format() const { return format_; }

private:
    int value_, default_, min_, max_;
    std::string format_;
};

class FloatSetting final : public Setting {
public:
    FloatSetting(std::string id, std::string name, std::string desc, float def, float min, float max,
                 std::string format = "%.2f");
    SettingType type() const override { return SettingType::Float; }
    Json toJson() const override { return value_; }
    bool fromJson(const Json& j) override;
    void reset() override { set(default_); }
    float get() const { return value_; }
    void set(float v);
    float min() const { return min_; }
    float max() const { return max_; }
    const std::string& format() const { return format_; }

private:
    float value_, default_, min_, max_;
    std::string format_;
};

// Liste d'options ; sérialisée par nom pour rester stable si l'ordre change.
class EnumSetting final : public Setting {
public:
    EnumSetting(std::string id, std::string name, std::string desc, std::vector<std::string> options, int def);
    SettingType type() const override { return SettingType::Enum; }
    Json toJson() const override { return options_[static_cast<size_t>(index_)]; }
    bool fromJson(const Json& j) override;
    void reset() override { set(default_); }
    int get() const { return index_; }
    void set(int index);
    const std::string& current() const { return options_[static_cast<size_t>(index_)]; }
    const std::vector<std::string>& options() const { return options_; }

private:
    std::vector<std::string> options_;
    int index_, default_;
};

class ColorSetting final : public Setting {
public:
    ColorSetting(std::string id, std::string name, std::string desc, Color def);
    SettingType type() const override { return SettingType::Color; }
    Json toJson() const override { return Json::array({value_.r, value_.g, value_.b, value_.a}); }
    bool fromJson(const Json& j) override;
    void reset() override { set(default_); }
    const Color& get() const { return value_; }
    void set(const Color& c);

private:
    Color value_, default_;
};

// Touche virtuelle Windows (VK_*). 0 = aucune touche.
class KeySetting final : public Setting {
public:
    KeySetting(std::string id, std::string name, std::string desc, int defVk);
    SettingType type() const override { return SettingType::Key; }
    Json toJson() const override { return value_; }
    bool fromJson(const Json& j) override;
    void reset() override { set(default_); }
    int get() const { return value_; }
    void set(int vk);
    int defaultValue() const { return default_; }

private:
    int value_, default_;
};

class StringSetting final : public Setting {
public:
    StringSetting(std::string id, std::string name, std::string desc, std::string def, size_t maxLength = 128);
    SettingType type() const override { return SettingType::String; }
    Json toJson() const override { return value_; }
    bool fromJson(const Json& j) override;
    void reset() override { set(default_); }
    const std::string& get() const { return value_; }
    void set(std::string v);
    size_t maxLength() const { return maxLength_; }

private:
    std::string value_, default_;
    size_t maxLength_;
};

// Bouton d'action dans l'interface (non sauvegardé).
class ActionSetting final : public Setting {
public:
    ActionSetting(std::string id, std::string name, std::string desc, std::function<void()> action);
    SettingType type() const override { return SettingType::Action; }
    Json toJson() const override { return nullptr; }
    bool fromJson(const Json&) override { return true; }
    void reset() override {}
    bool persistent() const override { return false; }
    void trigger() const { if (action_) action_(); }

private:
    std::function<void()> action_;
};

// Conteneur ordonné de paramètres (base des modules et des groupes de réglages).
class SettingsGroup {
public:
    virtual ~SettingsGroup() = default;

    template <class T, class... Args>
    T* add(Args&&... args) {
        auto setting = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = setting.get();
        settings_.push_back(std::move(setting));
        return raw;
    }

    const std::vector<std::unique_ptr<Setting>>& settings() const { return settings_; }
    Setting* findSetting(std::string_view id) const;

    Json settingsToJson() const;
    // Applique les valeurs connues ; retourne la liste des avertissements.
    std::vector<std::string> settingsFromJson(const Json& j, std::string_view context);
    void resetSettings();
    // Vrai pendant settingsFromJson / resetSettings : les callbacks de réglages
    // dépendants (presets...) doivent alors s'abstenir de modifier d'autres réglages.
    bool loadingSettings() const { return loading_; }

private:
    std::vector<std::unique_ptr<Setting>> settings_;
    bool loading_ = false;
};

} // namespace bcc
