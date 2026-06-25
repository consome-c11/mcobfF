#include "Settings.h"
#include "mcobfF/file/FileSystem.h"
#include <windows.h>
#include <imgui.h>
#include <algorithm>
#include <cctype>

namespace mcobfF
{
    Settings& Settings::instance()
    {
        static Settings s;
        return s;
    }

    Settings::Settings()
    {
        // ===== Register new settings here =====
        // define(Key, defaultValue, min, max, description, category)
        // Use {} for monostate when min/max is not applicable

        define(SettingKey::AnimationDuration, 0.3f, 0.05f, 1.0f, "Duration of UI animations in seconds", "Animation");
        define(SettingKey::ShowIntermediaryNames, true, {}, {}, "Show intermediary mapping names in left tree view",
               "Display");
        define(SettingKey::ShowObfNames, false, {}, {}, "Show obfuscated (official/notch) names in left tree view",
               "Display");
        define(SettingKey::ShowSrgNames, true, {}, {}, "Show SRG mapping names in left tree view", "Display");
        define(SettingKey::ShowIntermediaryNamesRight, true, {}, {}, "Show intermediary names in right detail panel",
               "Display");
        define(SettingKey::ShowObfNamesRight, false, {}, {},
               "Show obfuscated (official/notch) names in right detail panel", "Display");
        define(SettingKey::ShowSrgNamesRight, true, {}, {}, "Show SRG names in right detail panel", "Display");
        //define(SettingKey::MaxSearchResults,          100,       10,    1000, "Maximum number of search results to display",                  "Search");
        //define(SettingKey::AutoFetchManifest,         true,      {},    {},   "Automatically fetch version manifest on startup",              "Network");
        define(SettingKey::Theme, Theme::Dark, {}, {}, "UI color theme", "Appearance");

        // =======================================
    }

    void Settings::define(SettingKey key, SettingValue defaultVal, SettingValue minVal, SettingValue maxVal,
                          std::string desc, std::string cat)
    {
        SettingEntry entry;
        entry.value = defaultVal;
        entry.defaultValue = defaultVal;
        entry.min = minVal;
        entry.max = maxVal;
        entry.description = std::move(desc);
        entry.category = std::move(cat);
        entries_[key] = std::move(entry);
    }

    void Settings::resetToDefaults()
    {
        for (auto& [key, entry] : entries_)
        {
            entry.value = entry.defaultValue;
        }
        dirty_ = true;
    }

    nlohmann::json Settings::valueToJson(const SettingValue& v)
    {
        return std::visit([](auto&& arg) -> nlohmann::json
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>)
            {
                return nullptr;
            }
            else if constexpr (std::is_same_v<T, Theme>)
            {
                return std::string(magic_enum::enum_name(arg));
            }
            else
            {
                return arg;
            }
        }, v);
    }

    SettingValue Settings::jsonToValue(const nlohmann::json& j, const SettingValue& typeRef)
    {
        return std::visit([&j](auto&& arg) -> SettingValue
        {
            using T = std::decay_t<decltype(arg)>;
            try
            {
                if constexpr (std::is_same_v<T, std::monostate>)
                {
                    return std::monostate{};
                }
                else if constexpr (std::is_same_v<T, bool>)
                {
                    return j.get<bool>();
                }
                else if constexpr (std::is_same_v<T, int>)
                {
                    return j.get<int>();
                }
                else if constexpr (std::is_same_v<T, float>)
                {
                    return j.get<float>();
                }
                else if constexpr (std::is_same_v<T, std::string>)
                {
                    return j.get<std::string>();
                }
                else if constexpr (std::is_same_v<T, Theme>)
                {
                    auto val = magic_enum::enum_cast<Theme>(j.get<std::string>());
                    return val.value_or(Theme::Dark);
                }
            }
            catch (...)
            {
                return arg;
            }
            return arg;
        }, typeRef);
    }

    void Settings::save(const std::string& path) const
    {
        nlohmann::json j;
        for (const auto& [key, entry] : entries_)
        {
            j[std::string(magic_enum::enum_name(key))] = valueToJson(entry.value);
        }
        std::string dir = path.substr(0, path.find_last_of("\\/"));
        if (!dir.empty())
        {
            FileSystem::createDirectories(dir);
        }
        FileSystem::writeFile(path, j.dump(4));
    }

    void Settings::load(const std::string& path)
    {
        auto content = FileSystem::readFile(path);
        if (!content) return;
        try
        {
            nlohmann::json j = nlohmann::json::parse(*content);
            for (auto& [key, entry] : entries_)
            {
                std::string keyName(magic_enum::enum_name(key));
                if (j.contains(keyName))
                {
                    entry.value = jsonToValue(j[keyName], entry.defaultValue);
                }
            }
        }
        catch (...)
        {
        }
    }

    void Settings::renderAll()
    {
        std::map<std::string, std::vector<SettingKey>> categories;
        for (const auto& [key, entry] : entries_)
        {
            categories[entry.category].push_back(key);
        }

        for (auto& [cat, keys] : categories)
        {
            if (ImGui::CollapsingHeader(cat.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                for (auto key : keys)
                {
                    renderEntry(key, entries_.at(key));
                }
            }
        }
    }

    static std::string prettifyEnumName(std::string_view name)
    {
        std::string result;
        for (size_t i = 0; i < name.size(); i++)
        {
            char c = name[i];
            if (i > 0 && std::isupper(static_cast<unsigned char>(c))
                && std::islower(static_cast<unsigned char>(name[i - 1])))
            {
                result += ' ';
            }
            result += c;
        }
        return result;
    }

    Theme Settings::getEffectiveTheme() const
    {
        Theme current = get<Theme>(SettingKey::Theme);
        if (current != Theme::System) return current;

        bool isSystemLight = false;
        HKEY hKey;
        DWORD dwValue = 1;
        DWORD dwSize = sizeof(dwValue);
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0,
                          KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            if (RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr, reinterpret_cast<LPBYTE>(&dwValue),
                                 &dwSize) == ERROR_SUCCESS)
            {
                isSystemLight = (dwValue == 1);
            }
            RegCloseKey(hKey);
        }
        return isSystemLight ? Theme::Light : Theme::Dark;
    }

    void Settings::applyThemeIfNeeded()
    {
        Theme effectiveTheme = getEffectiveTheme();

        if (effectiveTheme == lastAppliedTheme_ && get<Theme>(SettingKey::Theme) != Theme::System) return;
        lastAppliedTheme_ = effectiveTheme;

        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        if (effectiveTheme == Theme::Light)
        {
            colors[ImGuiCol_Text] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
            colors[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
            colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.95f, 1.00f);
            colors[ImGuiCol_ChildBg] = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);
            colors[ImGuiCol_PopupBg] = ImVec4(0.96f, 0.96f, 0.97f, 0.95f);
            colors[ImGuiCol_Border] = ImVec4(0.75f, 0.76f, 0.80f, 0.60f);
            colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
            colors[ImGuiCol_FrameBg] = ImVec4(0.85f, 0.86f, 0.88f, 1.00f);
            colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.45f, 0.78f, 0.40f);
            colors[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.45f, 0.78f, 0.60f);
            colors[ImGuiCol_TitleBg] = ImVec4(0.90f, 0.91f, 0.93f, 1.00f);
            colors[ImGuiCol_TitleBgActive] = ImVec4(0.82f, 0.83f, 0.86f, 1.00f);
            colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.90f, 0.91f, 0.93f, 0.75f);
            colors[ImGuiCol_MenuBarBg] = ImVec4(0.88f, 0.89f, 0.91f, 1.00f);
            colors[ImGuiCol_ScrollbarBg] = ImVec4(0.94f, 0.94f, 0.95f, 0.50f);
            colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.72f, 0.73f, 0.78f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.62f, 0.63f, 0.68f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.52f, 0.53f, 0.58f, 1.00f);
            colors[ImGuiCol_CheckMark] = ImVec4(0.22f, 0.45f, 0.78f, 1.00f);
            colors[ImGuiCol_SliderGrab] = ImVec4(0.22f, 0.45f, 0.78f, 1.00f);
            colors[ImGuiCol_SliderGrabActive] = ImVec4(0.30f, 0.55f, 0.88f, 1.00f);
            colors[ImGuiCol_Button] = ImVec4(0.22f, 0.45f, 0.78f, 0.60f);
            colors[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.45f, 0.78f, 0.80f);
            colors[ImGuiCol_ButtonActive] = ImVec4(0.22f, 0.45f, 0.78f, 1.00f);
            colors[ImGuiCol_Header] = ImVec4(0.22f, 0.45f, 0.78f, 0.35f);
            colors[ImGuiCol_HeaderHovered] = ImVec4(0.22f, 0.45f, 0.78f, 0.55f);
            colors[ImGuiCol_HeaderActive] = ImVec4(0.22f, 0.45f, 0.78f, 0.70f);
            colors[ImGuiCol_Separator] = ImVec4(0.75f, 0.76f, 0.80f, 0.60f);
            colors[ImGuiCol_SeparatorHovered] = ImVec4(0.22f, 0.45f, 0.78f, 0.75f);
            colors[ImGuiCol_SeparatorActive] = ImVec4(0.22f, 0.45f, 0.78f, 1.00f);
            colors[ImGuiCol_ResizeGrip] = ImVec4(0.22f, 0.45f, 0.78f, 0.20f);
            colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.22f, 0.45f, 0.78f, 0.65f);
            colors[ImGuiCol_ResizeGripActive] = ImVec4(0.22f, 0.45f, 0.78f, 0.90f);
            colors[ImGuiCol_Tab] = ImVec4(0.70f, 0.82f, 0.95f, 0.80f);
            colors[ImGuiCol_TabHovered] = ImVec4(0.22f, 0.45f, 0.78f, 0.80f);
            colors[ImGuiCol_TableHeaderBg] = ImVec4(0.85f, 0.87f, 0.90f, 1.00f);
            colors[ImGuiCol_TableBorderStrong] = ImVec4(0.75f, 0.76f, 0.80f, 1.00f);
            colors[ImGuiCol_TableBorderLight] = ImVec4(0.80f, 0.81f, 0.85f, 0.60f);
            colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
            colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.00f, 0.00f, 0.00f, 0.05f);
        }
        else
        {
            colors[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
            colors[ImGuiCol_TextDisabled] = ImVec4(0.52f, 0.52f, 0.52f, 1.00f);
            colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.12f, 0.14f, 1.00f);
            colors[ImGuiCol_ChildBg] = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
            colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.11f, 0.13f, 0.95f);
            colors[ImGuiCol_Border] = ImVec4(0.25f, 0.26f, 0.30f, 0.60f);
            colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
            colors[ImGuiCol_FrameBg] = ImVec4(0.17f, 0.18f, 0.22f, 1.00f);
            colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.45f, 0.78f, 0.40f);
            colors[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.45f, 0.78f, 0.60f);
            colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.09f, 0.11f, 1.00f);
            colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.13f, 0.16f, 1.00f);
            colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.09f, 0.11f, 0.75f);
            colors[ImGuiCol_MenuBarBg] = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
            colors[ImGuiCol_ScrollbarBg] = ImVec4(0.11f, 0.12f, 0.14f, 0.50f);
            colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.32f, 0.33f, 0.38f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.42f, 0.43f, 0.48f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.52f, 0.53f, 0.58f, 1.00f);
            colors[ImGuiCol_CheckMark] = ImVec4(0.30f, 0.65f, 1.00f, 1.00f);
            colors[ImGuiCol_SliderGrab] = ImVec4(0.30f, 0.65f, 1.00f, 1.00f);
            colors[ImGuiCol_SliderGrabActive] = ImVec4(0.40f, 0.75f, 1.00f, 1.00f);
            colors[ImGuiCol_Button] = ImVec4(0.22f, 0.45f, 0.78f, 0.60f);
            colors[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.45f, 0.78f, 0.80f);
            colors[ImGuiCol_ButtonActive] = ImVec4(0.22f, 0.45f, 0.78f, 1.00f);
            colors[ImGuiCol_Header] = ImVec4(0.22f, 0.45f, 0.78f, 0.45f);
            colors[ImGuiCol_HeaderHovered] = ImVec4(0.22f, 0.45f, 0.78f, 0.65f);
            colors[ImGuiCol_HeaderActive] = ImVec4(0.22f, 0.45f, 0.78f, 0.80f);
            colors[ImGuiCol_Separator] = ImVec4(0.25f, 0.26f, 0.30f, 0.60f);
            colors[ImGuiCol_SeparatorHovered] = ImVec4(0.30f, 0.65f, 1.00f, 0.75f);
            colors[ImGuiCol_SeparatorActive] = ImVec4(0.30f, 0.65f, 1.00f, 1.00f);
            colors[ImGuiCol_ResizeGrip] = ImVec4(0.30f, 0.65f, 1.00f, 0.20f);
            colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.30f, 0.65f, 1.00f, 0.65f);
            colors[ImGuiCol_ResizeGripActive] = ImVec4(0.30f, 0.65f, 1.00f, 0.90f);
            colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.32f, 0.58f, 0.80f);
            colors[ImGuiCol_TabHovered] = ImVec4(0.22f, 0.45f, 0.78f, 0.80f);
            colors[ImGuiCol_TableHeaderBg] = ImVec4(0.18f, 0.20f, 0.25f, 1.00f);
            colors[ImGuiCol_TableBorderStrong] = ImVec4(0.25f, 0.26f, 0.30f, 1.00f);
            colors[ImGuiCol_TableBorderLight] = ImVec4(0.20f, 0.21f, 0.25f, 0.60f);
            colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
            colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
        }
    }

    void Settings::renderEntry(SettingKey key, SettingEntry& entry)
    {
        std::string label = prettifyEnumName(magic_enum::enum_name(key));

        std::visit([&](auto&& arg)
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>)
            {
                ImGui::TextUnformatted(label.c_str());
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                if (ImGui::Checkbox(label.c_str(), &arg))
                {
                    entry.value = arg;
                    dirty_ = true;
                }
            }
            else if constexpr (std::is_same_v<T, int>)
            {
                int minVal = 0, maxVal = 1000;
                if (const auto* m = std::get_if<int>(&entry.min)) minVal = *m;
                if (const auto* m = std::get_if<int>(&entry.max)) maxVal = *m;
                if (ImGui::SliderInt(label.c_str(), &arg, minVal, maxVal))
                {
                    entry.value = arg;
                    dirty_ = true;
                }
            }
            else if constexpr (std::is_same_v<T, float>)
            {
                float minVal = 0.0f, maxVal = 1.0f;
                if (const auto* m = std::get_if<float>(&entry.min)) minVal = *m;
                if (const auto* m = std::get_if<float>(&entry.max)) maxVal = *m;
                if (ImGui::SliderFloat(label.c_str(), &arg, minVal, maxVal, "%.2f"))
                {
                    entry.value = arg;
                    dirty_ = true;
                }
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                // TODO: InputText for string settings
            }
            else if constexpr (std::is_same_v<T, Theme>)
            {
                int current = static_cast<int>(arg);
                const char* items[] = {"Dark", "Light", "System"};
                if (ImGui::Combo(label.c_str(), &current, items, IM_ARRAYSIZE(items)))
                {
                    arg = static_cast<Theme>(current);
                    entry.value = arg;
                    dirty_ = true;
                }
            }
        }, entry.value);

        if (!entry.description.empty() && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", entry.description.c_str());
        }
    }
} // namespace mcobfF
