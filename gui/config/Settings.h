#pragma once
#include <string>
#include <string_view>
#include <variant>
#include <map>
#include <vector>
#include <magic_enum/magic_enum.hpp>
#include <nlohmann/json.hpp>

namespace mcobfF {

// Add new setting keys here
enum class SettingKey {
    AnimationDuration,
    ShowIntermediaryNames,
    ShowSrgNames,
    ShowIntermediaryNamesRight,
    ShowSrgNamesRight,
    MaxSearchResults,
    AutoFetchManifest,
    Theme,
};

enum class Theme {
    Dark,
    Light,
    System
};

using SettingValue = std::variant<std::monostate, bool, int, float, std::string, Theme>;

struct SettingEntry {
    SettingValue value;
    SettingValue defaultValue;
    SettingValue min;
    SettingValue max;
    std::string description;
    std::string category;
};

class Settings {
public:
    static Settings& instance();

    template<typename T>
    T get(SettingKey key) const {
        auto it = entries_.find(key);
        if (it == entries_.end()) return T{};
        if (const auto* v = std::get_if<T>(&it->second.value)) return *v;
        if (const auto* v = std::get_if<T>(&it->second.defaultValue)) return *v;
        return T{};
    }

    template<typename T>
    void set(SettingKey key, const T& value) {
        auto it = entries_.find(key);
        if (it != entries_.end()) {
            it->second.value = value;
            dirty_ = true;
        }
    }

    void resetToDefaults();

    void save(const std::string& path) const;
    void load(const std::string& path);

    void renderAll();

    bool isDirty() const { return dirty_; }
    void clearDirty() { dirty_ = false; }

    void applyThemeIfNeeded();
    Theme getEffectiveTheme() const;

    const std::map<SettingKey, SettingEntry>& entries() const { return entries_; }

private:
    Settings();
    std::map<SettingKey, SettingEntry> entries_;
    bool dirty_ = false;

    void define(SettingKey key, SettingValue defaultVal, SettingValue minVal, SettingValue maxVal,
                std::string desc, std::string cat);
    void renderEntry(SettingKey key, SettingEntry& entry);

    Theme lastAppliedTheme_ = Theme::Dark;

    static nlohmann::json valueToJson(const SettingValue& v);
    static SettingValue jsonToValue(const nlohmann::json& j, const SettingValue& typeRef);
};

} // namespace mcobfF
