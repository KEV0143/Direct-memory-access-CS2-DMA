#include "app/Config/config.h"
#include "app/Core/globals.h"
#include "app/Config/project_paths.h"
#include "app/Config/user_state.h"
#include "Features/WebRadar/webradar.h"

#include <json/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

namespace {
    using json = nlohmann::json;
    using IniSection = std::unordered_map<std::string, std::string>;
    using IniDocument = std::unordered_map<std::string, IniSection>;

    std::string s_activeProfile = "KevqDefault";

    std::string ToLower(std::string_view value)
    {
        std::string lowered;
        lowered.reserve(value.size());

        for (const unsigned char c : value)
            lowered.push_back(static_cast<char>(std::tolower(c)));

        return lowered;
    }

    std::string Trim(std::string_view value)
    {
        size_t begin = 0;
        size_t end = value.size();

        while (begin < end && std::isspace(static_cast<unsigned char>(value[begin])) != 0)
            ++begin;
        while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
            --end;

        return std::string(value.substr(begin, end - begin));
    }

    bool IsReservedProfileName(const std::string& name)
    {
        const std::string lowered = ToLower(name);
        return lowered == "offsets" || lowered == "imgui" || lowered == "radar_maps";
    }

    std::filesystem::path GetConfigDirectory()
    {
        return app::paths::GetConfigDirectory();
    }

    std::filesystem::path GetLegacyProfilesDirectory()
    {
        return app::paths::GetLegacyProfilesDirectory();
    }

    std::filesystem::path GetSettingsDirectory()
    {
        return app::paths::GetSettingsDirectory();
    }

    std::string SanitizeProfileName(const std::string& rawName)
    {
        std::string out;
        out.reserve(rawName.size());

        for (char c : rawName) {
            const bool valid =
                (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') ||
                c == '_' || c == '-';
            out.push_back(valid ? c : '_');
        }

        while (!out.empty() && (out.back() == ' ' || out.back() == '.'))
            out.pop_back();

        if (out.empty())
            out = "KevqDefault";

        return out;
    }

    std::filesystem::path BuildProfilePathObject(const std::string& profileName)
    {
        const std::string safeName = SanitizeProfileName(profileName);
        return GetConfigDirectory() / (safeName + ".json");
    }

    std::filesystem::path BuildLegacyProfilePathObject(const std::string& profileName)
    {
        const std::string safeName = SanitizeProfileName(profileName);
        return GetLegacyProfilesDirectory() / (safeName + ".ini");
    }

    std::vector<std::filesystem::path> CollectLegacyProfilePathCandidates(const std::string& profileName)
    {
        const std::string safeName = SanitizeProfileName(profileName);
        return {
            GetConfigDirectory() / (safeName + ".ini"),
            GetLegacyProfilesDirectory() / (safeName + ".ini"),
        };
    }

    std::string BuildProfilePath(const std::string& profileName)
    {
        return BuildProfilePathObject(profileName).string();
    }

    std::array<float, 4> CopyColor(const float src[4])
    {
        return { src[0], src[1], src[2], src[3] };
    }

    void CopyColor(float dst[4], const std::array<float, 4>& src)
    {
        for (size_t i = 0; i < src.size(); ++i)
            dst[i] = src[i];
    }

    struct ScreenConfigSnapshot {
        bool vsyncEnabled = true;
        int fpsLimit = 0;
    };

    struct ConfigSnapshot {
        app::state::EspSettings esp = {};
        app::state::RadarSettings radar = {};
        app::state::WebRadarSettings webRadar = {};
        app::state::UiSettings ui = {};
        ScreenConfigSnapshot screen = {};
    };

    ConfigSnapshot CaptureCurrentConfig()
    {
        return ConfigSnapshot {
            g::espSettings,
            g::radarSettings,
            g::webRadarSettings,
            g::uiSettings,
            { g::vsyncEnabled, g::fpsLimit }
        };
    }

    void ApplyConfig(const ConfigSnapshot& snapshot)
    {
        g::espSettings = snapshot.esp;
        g::radarSettings = snapshot.radar;
        g::webRadarSettings = snapshot.webRadar;
        g::uiSettings = snapshot.ui;
        g::webRadarIntervalMs = 4;
        g::vsyncEnabled = snapshot.screen.vsyncEnabled;
        g::fpsLimit = snapshot.screen.fpsLimit;
    }

    const ConfigSnapshot& GetDefaultConfig()
    {
        static const ConfigSnapshot defaults = CaptureCurrentConfig();
        return defaults;
    }

    bool SaveJsonConfig(const std::string& jsonPath);
    bool LoadJsonConfig(const std::string& jsonPath);
    bool LoadFromLegacyPath(const std::filesystem::path& path);

    bool RemoveFileIfExists(const std::filesystem::path& path)
    {
        if (path.empty())
            return false;

        std::error_code ec;
        if (!std::filesystem::exists(path, ec))
            return false;

        ec.clear();
        return std::filesystem::remove(path, ec) && !ec;
    }

    void DeleteLegacyProfileFiles(const std::string& profileName)
    {
        std::unordered_set<std::string> seen;
        for (const auto& path : CollectLegacyProfilePathCandidates(profileName)) {
            const std::string key = path.lexically_normal().generic_string();
            if (!seen.insert(key).second)
                continue;
            RemoveFileIfExists(path);
        }
    }

    bool SaveToPath(const std::string& iniPath)
    {
        return SaveJsonConfig(iniPath);
    }

    bool LoadFromPath(const std::string& iniPath)
    {
        return LoadJsonConfig(iniPath);
    }

    void PersistActiveProfileName(const std::string& profileName)
    {
        app::user_state::SaveActiveProfile(SanitizeProfileName(profileName));
    }

    std::string LoadPersistedActiveProfileName()
    {
        std::string profileName;
        if (!app::user_state::LoadActiveProfile(&profileName))
            return {};

        const std::string name = SanitizeProfileName(profileName);
        return name.empty() ? std::string{} : name;
    }

    json& EnsureSection(json& root, std::string_view sectionName)
    {
        json& section = root[std::string(sectionName)];
        if (!section.is_object())
            section = json::object();
        return section;
    }

    const json* FindSection(const json& root, std::string_view sectionName)
    {
        const auto it = root.find(std::string(sectionName));
        if (it == root.end() || !it->is_object())
            return nullptr;
        return &(*it);
    }

    bool ParseBoolString(std::string_view value, bool fallback)
    {
        const std::string lowered = ToLower(Trim(value));
        if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on")
            return true;
        if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off")
            return false;
        return fallback;
    }

    int ParseIntString(std::string_view value, int fallback)
    {
        try {
            return std::stoi(Trim(value));
        } catch (...) {
            return fallback;
        }
    }

    float ParseFloatString(std::string_view value, float fallback)
    {
        try {
            return std::stof(Trim(value));
        } catch (...) {
            return fallback;
        }
    }

    void SaveColor(json& section, std::string_view key, const float color[4])
    {
        section[std::string(key)] = json::array({ color[0], color[1], color[2], color[3] });
    }

    void LoadBool(const json& root, std::string_view sectionName, std::string_view key, bool& value)
    {
        const json* section = FindSection(root, sectionName);
        if (!section)
            return;

        const auto it = section->find(std::string(key));
        if (it == section->end())
            return;

        if (it->is_boolean())
            value = it->get<bool>();
        else if (it->is_number_integer())
            value = it->get<int>() != 0;
        else if (it->is_string())
            value = ParseBoolString(it->get_ref<const std::string&>(), value);
    }

    void LoadInt(const json& root, std::string_view sectionName, std::string_view key, int& value)
    {
        const json* section = FindSection(root, sectionName);
        if (!section)
            return;

        const auto it = section->find(std::string(key));
        if (it == section->end())
            return;

        if (it->is_number_integer())
            value = it->get<int>();
        else if (it->is_boolean())
            value = it->get<bool>() ? 1 : 0;
        else if (it->is_string())
            value = ParseIntString(it->get_ref<const std::string&>(), value);
    }

    void LoadFloat(const json& root, std::string_view sectionName, std::string_view key, float& value)
    {
        const json* section = FindSection(root, sectionName);
        if (!section)
            return;

        const auto it = section->find(std::string(key));
        if (it == section->end())
            return;

        if (it->is_number())
            value = it->get<float>();
        else if (it->is_string())
            value = ParseFloatString(it->get_ref<const std::string&>(), value);
    }

    void LoadString(const json& root, std::string_view sectionName, std::string_view key, std::string& value)
    {
        const json* section = FindSection(root, sectionName);
        if (!section)
            return;

        const auto it = section->find(std::string(key));
        if (it != section->end() && it->is_string())
            value = it->get<std::string>();
    }

    void LoadColor(const json& root, std::string_view sectionName, std::string_view key, float color[4])
    {
        const json* section = FindSection(root, sectionName);
        if (!section)
            return;

        const auto it = section->find(std::string(key));
        if (it == section->end() || !it->is_array() || it->size() != 4)
            return;

        for (size_t i = 0; i < 4; ++i) {
            if (!(*it)[i].is_number())
                return;
            color[i] = (*it)[i].get<float>();
        }
    }

    void LoadDisabledItemIds(const json& root, std::string_view sectionName, std::string_view key, std::bitset<1200>& mask)
    {
        const json* section = FindSection(root, sectionName);
        if (!section)
            return;

        const auto it = section->find(std::string(key));
        if (it == section->end() || !it->is_array())
            return;

        mask = app::state::CreateDefaultItemEspMask();
        for (const auto& value : *it) {
            if (!value.is_number_unsigned() && !value.is_number_integer())
                continue;
            const int itemId = value.get<int>();
            if (itemId > 0 && itemId < 1200)
                mask.reset(static_cast<size_t>(itemId));
        }
    }

    const std::string* FindIniValue(const IniDocument& ini, std::string_view sectionName, std::string_view key)
    {
        const auto secIt = ini.find(std::string(sectionName));
        if (secIt == ini.end())
            return nullptr;

        const auto keyIt = secIt->second.find(std::string(key));
        if (keyIt == secIt->second.end())
            return nullptr;

        return &keyIt->second;
    }

    void LoadBool(const IniDocument& ini, std::string_view sectionName, std::string_view key, bool& value)
    {
        const std::string* raw = FindIniValue(ini, sectionName, key);
        if (raw != nullptr)
            value = ParseBoolString(*raw, value);
    }

    void LoadInt(const IniDocument& ini, std::string_view sectionName, std::string_view key, int& value)
    {
        const std::string* raw = FindIniValue(ini, sectionName, key);
        if (raw != nullptr)
            value = ParseIntString(*raw, value);
    }

    void LoadFloat(const IniDocument& ini, std::string_view sectionName, std::string_view key, float& value)
    {
        const std::string* raw = FindIniValue(ini, sectionName, key);
        if (raw != nullptr)
            value = ParseFloatString(*raw, value);
    }

    void LoadString(const IniDocument& ini, std::string_view sectionName, std::string_view key, std::string& value)
    {
        const std::string* raw = FindIniValue(ini, sectionName, key);
        if (raw != nullptr)
            value = *raw;
    }

    void LoadColor(const IniDocument& ini, std::string_view sectionName, std::string_view key, float color[4])
    {
        const std::string* raw = FindIniValue(ini, sectionName, key);
        if (raw == nullptr)
            return;

        const std::string_view value = *raw;
        size_t start = 0;
        for (size_t i = 0; i < 4; ++i) {
            const size_t comma = value.find(',', start);
            const size_t len = (comma == std::string_view::npos) ? value.size() - start : comma - start;
            color[i] = ParseFloatString(value.substr(start, len), color[i]);

            if (comma == std::string_view::npos) {
                if (i != 3)
                    return;
                break;
            }
            start = comma + 1;
        }
    }

    void LoadDisabledItemIds(const IniDocument& ini, std::string_view sectionName, std::string_view key, std::bitset<1200>& mask)
    {
        const std::string* raw = FindIniValue(ini, sectionName, key);
        if (raw == nullptr)
            return;

        mask = app::state::CreateDefaultItemEspMask();
        const std::string_view value = *raw;
        size_t start = 0;
        while (start < value.size()) {
            const size_t comma = value.find(',', start);
            const size_t len = (comma == std::string_view::npos) ? (value.size() - start) : (comma - start);
            const int itemId = ParseIntString(value.substr(start, len), -1);
            if (itemId > 0 && itemId < 1200)
                mask.reset(static_cast<size_t>(itemId));
            if (comma == std::string_view::npos)
                break;
            start = comma + 1;
        }
    }

    void ValidateLoadedValues()
    {
        constexpr int kVkEnd = 0x23;
        constexpr int kVkInsert = 0x2D;
        const ConfigSnapshot& defaults = GetDefaultConfig();

        if (g::radarMode < 0 || g::radarMode > 1)
            g::radarMode = defaults.radar.mode;
        if (g::radarSize < 100.0f || g::radarSize > 700.0f)
            g::radarSize = defaults.radar.size;
        if (g::radarDotSize < 2.0f || g::radarDotSize > 8.0f)
            g::radarDotSize = defaults.radar.dotSize;
        if (g::espOffscreenSize < 6.0f || g::espOffscreenSize > 36.0f)
            g::espOffscreenSize = defaults.esp.offscreenSize;
        if (g::radarWorldRotationDeg < -180.0f || g::radarWorldRotationDeg > 180.0f)
            g::radarWorldRotationDeg = defaults.radar.worldRotationDeg;
        if (g::radarWorldScale < 0.50f || g::radarWorldScale > 1.50f)
            g::radarWorldScale = defaults.radar.worldScale;
        if (g::radarWorldOffsetX < -0.25f || g::radarWorldOffsetX > 0.25f)
            g::radarWorldOffsetX = defaults.radar.worldOffsetX;
        if (g::radarWorldOffsetY < -0.25f || g::radarWorldOffsetY > 0.25f)
            g::radarWorldOffsetY = defaults.radar.worldOffsetY;
        if (g::webRadarPort < 1025 || g::webRadarPort > 65535)
            g::webRadarPort = static_cast<int>(webradar::cfg::kDefaultListenPort);
        if (g::fpsLimit < 0)
            g::fpsLimit = 0;
        if (g::fpsLimit > 500)
            g::fpsLimit = 500;
        auto sanitizeOverlayPos = [](float& x, float& y, float defaultX, float defaultY) {
            constexpr float kMaxCoord = 12000.0f;
            if (!std::isfinite(x) || std::fabs(x) > kMaxCoord)
                x = defaultX;
            if (!std::isfinite(y) || std::fabs(y) > kMaxCoord)
                y = defaultY;
        };
        sanitizeOverlayPos(g::radarSpectatorListX, g::radarSpectatorListY, defaults.radar.spectatorListX, defaults.radar.spectatorListY);
        sanitizeOverlayPos(g::espBombTimerX, g::espBombTimerY, defaults.esp.bombTimerX, defaults.esp.bombTimerY);
        if (g::menuToggleKey < 0x08 || g::menuToggleKey > 0xFE || g::menuToggleKey == kVkEnd || g::menuToggleKey == kVkInsert)
            g::menuToggleKey = defaults.ui.menuToggleKey;
        g::espItemEnabledMask.set(0, false);
        for (uint16_t id = 1; id < 1200; ++id) {
            if (app::state::IsKnifeItemId(id))
                g::espItemEnabledMask.set(id, false);
        }
        g::webRadarIntervalMs = 4;
    }

    void ApplyLoadedConfig(const json& root)
    {
        #include "config_parts/config_apply_json_body.inl"
    }

    void ApplyLoadedConfig(const IniDocument& ini)
    {
        LoadBool(ini, "ESP", "Enabled", g::espEnabled);
        LoadBool(ini, "ESP", "Box", g::espBox);
        LoadBool(ini, "ESP", "Health", g::espHealth);
        LoadBool(ini, "ESP", "HealthText", g::espHealthText);
        LoadBool(ini, "ESP", "Armor", g::espArmor);
        LoadBool(ini, "ESP", "ArmorText", g::espArmorText);
        LoadBool(ini, "ESP", "Name", g::espName);
        LoadFloat(ini, "ESP", "NameFontSize", g::espNameFontSize);
        LoadBool(ini, "ESP", "Weapon", g::espWeapon);
        LoadBool(ini, "ESP", "WeaponText", g::espWeaponText);
        LoadFloat(ini, "ESP", "WeaponTextSize", g::espWeaponTextSize);
        LoadColor(ini, "ESP", "WeaponTextColor", g::espWeaponTextColor);
        LoadBool(ini, "ESP", "WeaponIcon", g::espWeaponIcon);
        LoadBool(ini, "ESP", "WeaponIconNoKnife", g::espWeaponIconNoKnife);
        LoadFloat(ini, "ESP", "WeaponIconSize", g::espWeaponIconSize);
        LoadColor(ini, "ESP", "WeaponIconColor", g::espWeaponIconColor);
        LoadBool(ini, "ESP", "WeaponAmmo", g::espWeaponAmmo);
        LoadFloat(ini, "ESP", "WeaponAmmoSize", g::espWeaponAmmoSize);
        LoadColor(ini, "ESP", "WeaponAmmoColor", g::espWeaponAmmoColor);
        LoadBool(ini, "ESP", "Distance", g::espDistance);
        LoadFloat(ini, "ESP", "DistanceSize", g::espDistanceSize);
        LoadBool(ini, "ESP", "Skeleton", g::espSkeleton);
        LoadBool(ini, "ESP", "SkeletonDots", g::espSkeletonDots);
        LoadBool(ini, "ESP", "Snaplines", g::espSnaplines);
        LoadBool(ini, "ESP", "SnapFromTop", g::espSnaplineFromTop);
        LoadBool(ini, "ESP", "VisibilityColoring", g::espVisibilityColoring);
        LoadBool(ini, "ESP", "OffscreenArrows", g::espOffscreenArrows);
        LoadBool(ini, "ESP", "Sound", g::espSound);
        LoadBool(ini, "ESP", "Flags", g::espFlags);
        LoadBool(ini, "ESP", "Item", g::espItem);
        LoadBool(ini, "ESP", "FlagBlind", g::espFlagBlind);
        LoadColor(ini, "ESP", "FlagBlindColor", g::espFlagBlindColor);
        LoadFloat(ini, "ESP", "FlagBlindSize", g::espFlagBlindSize);
        LoadBool(ini, "ESP", "FlagScoped", g::espFlagScoped);
        LoadColor(ini, "ESP", "FlagScopedColor", g::espFlagScopedColor);
        LoadFloat(ini, "ESP", "FlagScopedSize", g::espFlagScopedSize);
        LoadBool(ini, "ESP", "FlagDefusing", g::espFlagDefusing);
        LoadColor(ini, "ESP", "FlagDefusingColor", g::espFlagDefusingColor);
        LoadFloat(ini, "ESP", "FlagDefusingSize", g::espFlagDefusingSize);
        LoadBool(ini, "ESP", "FlagKit", g::espFlagKit);
        LoadColor(ini, "ESP", "FlagKitColor", g::espFlagKitColor);
        LoadFloat(ini, "ESP", "FlagKitSize", g::espFlagKitSize);
        LoadBool(ini, "ESP", "FlagMoney", g::espFlagMoney);
        LoadColor(ini, "ESP", "FlagMoneyColor", g::espFlagMoneyColor);
        LoadFloat(ini, "ESP", "FlagMoneySize", g::espFlagMoneySize);
        LoadBool(ini, "ESP", "World", g::espWorld);
        LoadBool(ini, "ESP", "WorldProjectiles", g::espWorldProjectiles);
        LoadBool(ini, "ESP", "WorldSmokeTimer", g::espWorldSmokeTimer);
        LoadBool(ini, "ESP", "WorldInfernoTimer", g::espWorldInfernoTimer);
        LoadBool(ini, "ESP", "WorldDecoyTimer", g::espWorldDecoyTimer);
        LoadBool(ini, "ESP", "WorldExplosiveTimer", g::espWorldExplosiveTimer);
        LoadBool(ini, "ESP", "BombInfo", g::espBombInfo);
        LoadBool(ini, "ESP", "BombText", g::espBombText);
        LoadBool(ini, "ESP", "BombTime", g::espBombTime);
        LoadFloat(ini, "ESP", "BombTextSize", g::espBombTextSize);
        LoadFloat(ini, "ESP", "BombTimerX", g::espBombTimerX);
        LoadFloat(ini, "ESP", "BombTimerY", g::espBombTimerY);
        LoadDisabledItemIds(ini, "ESP", "ItemHiddenIds", g::espItemEnabledMask);
        LoadFloat(ini, "ESP", "OffscreenSize", g::espOffscreenSize);
        LoadColor(ini, "ESP", "BoxColor", g::espBoxColor);
        LoadColor(ini, "ESP", "HealthColor", g::espHealthColor);
        LoadColor(ini, "ESP", "VisibleColor", g::espVisibleColor);
        LoadColor(ini, "ESP", "HiddenColor", g::espHiddenColor);
        LoadColor(ini, "ESP", "ArmorColor", g::espArmorColor);
        LoadColor(ini, "ESP", "NameColor", g::espNameColor);
        LoadColor(ini, "ESP", "DistanceColor", g::espDistanceColor);
        LoadColor(ini, "ESP", "SkeletonColor", g::espSkeletonColor);
        LoadColor(ini, "ESP", "SnaplineColor", g::espSnaplineColor);
        LoadColor(ini, "ESP", "OffscreenColor", g::espOffscreenColor);
        LoadColor(ini, "ESP", "FlagColor", g::espFlagColor);
        LoadColor(ini, "ESP", "WorldColor", g::espWorldColor);
        LoadColor(ini, "ESP", "BombColor", g::espBombColor);
        LoadColor(ini, "ESP", "SoundColor", g::espSoundColor);

        LoadBool(ini, "Radar", "Enabled", g::radarEnabled);
        LoadInt(ini, "Radar", "Mode", g::radarMode);
        LoadBool(ini, "Radar", "ShowLocalDot", g::radarShowLocalDot);
        LoadBool(ini, "Radar", "ShowAngles", g::radarShowAngles);
        LoadBool(ini, "Radar", "ShowCrosshair", g::radarShowCrosshair);
        LoadBool(ini, "Radar", "ShowBomb", g::radarShowBomb);
        LoadFloat(ini, "Radar", "Size", g::radarSize);
        LoadFloat(ini, "Radar", "DotSize", g::radarDotSize);
        LoadFloat(ini, "Radar", "WorldRotationDeg", g::radarWorldRotationDeg);
        LoadFloat(ini, "Radar", "WorldScale", g::radarWorldScale);
        LoadFloat(ini, "Radar", "WorldOffsetX", g::radarWorldOffsetX);
        LoadFloat(ini, "Radar", "WorldOffsetY", g::radarWorldOffsetY);
        LoadBool(ini, "Radar", "StaticFlipX", g::radarStaticFlipX);
        LoadColor(ini, "Radar", "BgColor", g::radarBgColor);
        LoadColor(ini, "Radar", "BorderColor", g::radarBorderColor);
        LoadColor(ini, "Radar", "DotColor", g::radarDotColor);
        LoadColor(ini, "Radar", "BombColor", g::radarBombColor);
        LoadColor(ini, "Radar", "AngleColor", g::radarAngleColor);
        LoadBool(ini, "Radar", "SpectatorList", g::radarSpectatorList);
        LoadFloat(ini, "Radar", "SpectatorListX", g::radarSpectatorListX);
        LoadFloat(ini, "Radar", "SpectatorListY", g::radarSpectatorListY);

        LoadBool(ini, "WEBRadar", "Enabled", g::webRadarEnabled);
        LoadInt(ini, "WEBRadar", "Port", g::webRadarPort);
        LoadString(ini, "WEBRadar", "MapOverride", g::webRadarMapOverride);

        LoadBool(ini, "Screen", "VSync", g::vsyncEnabled);
        LoadInt(ini, "Screen", "FPSLimit", g::fpsLimit);

        LoadBool(ini, "UI", "EspPreviewOpen", g::espPreviewOpen);
        LoadBool(ini, "UI", "RadarCalibrationOpen", g::radarCalibrationOpen);
        LoadBool(ini, "UI", "WebRadarQrOpen", g::webRadarQrOpen);
        LoadBool(ini, "UI", "WebRadarDebugOpen", g::webRadarDebugOpen);
        LoadInt(ini, "UI", "MenuToggleKey", g::menuToggleKey);
    }

    IniDocument ParseIniFile(const std::filesystem::path& path)
    {
        IniDocument ini;
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            return ini;

        std::string currentSection;
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF) {
                line.erase(0, 3);
            }

            const std::string trimmed = Trim(line);
            if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#')
                continue;

            if (trimmed.front() == '[' && trimmed.back() == ']') {
                currentSection = Trim(std::string_view(trimmed).substr(1, trimmed.size() - 2));
                continue;
            }

            const size_t eq = trimmed.find('=');
            if (eq == std::string::npos)
                continue;

            const std::string key = Trim(std::string_view(trimmed).substr(0, eq));
            const std::string value = Trim(std::string_view(trimmed).substr(eq + 1));
            if (!key.empty())
                ini[currentSection][key] = value;
        }

        return ini;
    }

    bool SaveJsonConfig(const std::string& jsonPath)
    {
        #include "config_parts/config_save_json_body.inl"
    }

    bool LoadJsonConfig(const std::string& jsonPath)
    {
        std::ifstream file(jsonPath, std::ios::binary);
        if (!file.is_open())
            return false;

        ApplyConfig(GetDefaultConfig());

        json root = json::parse(file, nullptr, false);
        if (root.is_discarded() || !root.is_object())
            return false;

        ApplyLoadedConfig(root);
        ValidateLoadedValues();
        return true;
    }

    bool LoadFromLegacyPath(const std::filesystem::path& path)
    {
        const IniDocument ini = ParseIniFile(path);
        if (ini.empty())
            return false;

        ApplyConfig(GetDefaultConfig());
        ApplyLoadedConfig(ini);
        ValidateLoadedValues();
        return true;
    }

    bool MigrateLegacyProfileToJson(const std::string& profileName)
    {
        const std::string cleanName = SanitizeProfileName(profileName);
        const std::filesystem::path jsonPath = BuildProfilePathObject(cleanName);

        std::error_code ec;
        if (std::filesystem::exists(jsonPath, ec)) {
            DeleteLegacyProfileFiles(cleanName);
            return true;
        }

        std::filesystem::path legacySource;
        std::unordered_set<std::string> seen;
        for (const auto& candidate : CollectLegacyProfilePathCandidates(cleanName)) {
            const std::string key = candidate.lexically_normal().generic_string();
            if (!seen.insert(key).second)
                continue;

            ec.clear();
            if (std::filesystem::exists(candidate, ec)) {
                legacySource = candidate;
                break;
            }
        }

        if (legacySource.empty())
            return false;

        const ConfigSnapshot savedSnapshot = CaptureCurrentConfig();
        const std::string savedActiveProfile = s_activeProfile;

        const bool migrated = LoadFromLegacyPath(legacySource) && SaveToPath(jsonPath.string());

        ApplyConfig(savedSnapshot);
        s_activeProfile = savedActiveProfile;

        if (migrated)
            DeleteLegacyProfileFiles(cleanName);

        return migrated;
    }

    void MigrateAllLegacyProfilesToJson()
    {
        std::unordered_set<std::string> profileNames;
        std::error_code ec;

        const std::filesystem::path directories[] = {
            GetConfigDirectory(),
            GetLegacyProfilesDirectory(),
        };

        for (const auto& directory : directories) {
            ec.clear();
            if (directory.empty() || !std::filesystem::exists(directory, ec))
                continue;

            for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
                if (ec || !entry.is_regular_file())
                    continue;

                const std::filesystem::path path = entry.path();
                if (ToLower(path.extension().string()) != ".ini")
                    continue;

                const std::string name = path.stem().string();
                if (!name.empty() && !IsReservedProfileName(name))
                    profileNames.insert(name);
            }
        }

        for (const auto& name : profileNames)
            MigrateLegacyProfileToJson(name);
    }
}

void config::Save()
{
    SaveNamed(s_activeProfile);
}

void config::Load()
{
    MigrateAllLegacyProfilesToJson();

    const std::string persistedProfile = LoadPersistedActiveProfileName();
    if (!persistedProfile.empty())
        s_activeProfile = persistedProfile;

    if (!LoadNamed(s_activeProfile) && s_activeProfile != "KevqDefault")
        LoadNamed("KevqDefault");
}

bool config::SaveNamed(const std::string& profileName)
{
    const std::string cleanName = SanitizeProfileName(profileName);
    if (!SaveToPath(BuildProfilePath(cleanName)))
        return false;

    DeleteLegacyProfileFiles(cleanName);
    s_activeProfile = cleanName;
    PersistActiveProfileName(s_activeProfile);
    return true;
}

bool config::LoadNamed(const std::string& profileName)
{
    const std::string cleanName = SanitizeProfileName(profileName);
    MigrateLegacyProfileToJson(cleanName);
    const std::string path = BuildProfilePath(cleanName);
    if (!LoadFromPath(path))
        return false;

    s_activeProfile = cleanName;
    PersistActiveProfileName(s_activeProfile);
    return true;
}

std::vector<std::string> config::ListProfiles()
{
    MigrateAllLegacyProfilesToJson();

    std::vector<std::string> result;
    std::error_code ec;

    const std::filesystem::path configDir(GetConfigDirectory());
    if (std::filesystem::exists(configDir, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(configDir, ec)) {
            if (ec || !entry.is_regular_file())
                continue;

            const std::filesystem::path path = entry.path();
            if (ToLower(path.extension().string()) != ".json")
                continue;

            const std::string name = path.stem().string();
            if (!name.empty() && !IsReservedProfileName(name))
                result.push_back(name);
        }
    }

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

const std::string& config::GetActiveProfile()
{
    return s_activeProfile;
}
