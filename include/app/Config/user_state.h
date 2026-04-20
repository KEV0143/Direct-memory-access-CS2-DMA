#pragma once

#include "app/Config/project_paths.h"

#include <json/json.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace app::user_state
{
    inline constexpr std::string_view kDefaultActiveProfile = "KevqDefault";

    struct RadarCalibrationRecord
    {
        float rotationDeg = 0.0f;
        float scale = 1.0f;
        float baseOffsetX = 0.0f;
        float baseOffsetY = 0.0f;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        bool hasOverview = false;
        float overviewPosX = 0.0f;
        float overviewPosY = 0.0f;
        float overviewScale = 0.0f;
    };

    inline std::filesystem::path GetPath()
    {
        const auto settingsDir = app::paths::GetSettingsDirectory();
        if (settingsDir.empty())
            return {};
        return settingsDir / "user_state.json";
    }

    inline std::filesystem::path GetLegacyActiveProfilePath()
    {
        const auto settingsDir = app::paths::GetSettingsDirectory();
        if (settingsDir.empty())
            return {};
        return settingsDir / "active_profile.json";
    }

    inline nlohmann::ordered_json ReadRoot()
    {
        const auto path = GetPath();
        if (path.empty())
            return nlohmann::ordered_json::object();

        std::ifstream input(path, std::ios::binary);
        if (!input.is_open())
            return nlohmann::ordered_json::object();

        nlohmann::ordered_json root = nlohmann::ordered_json::parse(input, nullptr, false);
        if (root.is_discarded() || !root.is_object())
            return nlohmann::ordered_json::object();

        return root;
    }

    inline bool WriteRoot(const nlohmann::ordered_json& root)
    {
        const auto path = GetPath();
        if (path.empty())
            return false;

        if (root.empty()) {
            std::error_code ec;
            std::filesystem::remove(path, ec);
            return true;
        }

        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);

        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output.is_open())
            return false;

        output << root.dump(4) << '\n';
        return output.good();
    }

    inline void RemoveLegacyActiveProfileFile()
    {
        const auto path = GetLegacyActiveProfilePath();
        if (path.empty())
            return;

        std::error_code ec;
        std::filesystem::remove(path, ec);
    }

    inline bool IsDefaultRadarCalibration(const RadarCalibrationRecord& record)
    {
        constexpr float kEpsilon = 0.0001f;
        return std::fabs(record.rotationDeg) <= kEpsilon &&
               std::fabs(record.scale - 1.0f) <= kEpsilon &&
               std::fabs(record.baseOffsetX) <= kEpsilon &&
               std::fabs(record.baseOffsetY) <= kEpsilon &&
               std::fabs(record.offsetX) <= kEpsilon &&
               std::fabs(record.offsetY) <= kEpsilon &&
               !record.hasOverview &&
               std::fabs(record.overviewPosX) <= kEpsilon &&
               std::fabs(record.overviewPosY) <= kEpsilon &&
               std::fabs(record.overviewScale) <= kEpsilon;
    }

    inline void PruneRoot(nlohmann::ordered_json& root)
    {
        auto activeIt = root.find("active_profile");
        if (activeIt != root.end()) {
            if (!activeIt->is_string() || activeIt->get<std::string>().empty() ||
                activeIt->get<std::string>() == kDefaultActiveProfile) {
                root.erase(activeIt);
            }
        }

        auto radarIt = root.find("radar_maps");
        if (radarIt != root.end()) {
            if (!radarIt->is_object() || radarIt->empty())
                root.erase(radarIt);
        }
    }

    inline bool SaveActiveProfile(std::string_view profileName)
    {
        nlohmann::ordered_json root = ReadRoot();
        if (profileName.empty() || profileName == kDefaultActiveProfile)
            root.erase("active_profile");
        else
            root["active_profile"] = std::string(profileName);

        PruneRoot(root);
        if (!WriteRoot(root))
            return false;

        RemoveLegacyActiveProfileFile();
        return true;
    }

    inline bool LoadActiveProfile(std::string* outProfile)
    {
        if (!outProfile)
            return false;

        nlohmann::ordered_json root = ReadRoot();
        auto it = root.find("active_profile");
        if (it != root.end() && it->is_string()) {
            *outProfile = it->get<std::string>();
            return !outProfile->empty();
        }

        const auto legacyPath = GetLegacyActiveProfilePath();
        if (legacyPath.empty())
            return false;

        std::ifstream input(legacyPath, std::ios::binary);
        if (!input.is_open())
            return false;

        nlohmann::ordered_json legacyRoot = nlohmann::ordered_json::parse(input, nullptr, false);
        if (legacyRoot.is_discarded() || !legacyRoot.is_object())
            return false;

        auto legacyIt = legacyRoot.find("active_profile");
        if (legacyIt == legacyRoot.end() || !legacyIt->is_string())
            return false;

        *outProfile = legacyIt->get<std::string>();
        if (outProfile->empty())
            return false;

        SaveActiveProfile(*outProfile);
        return true;
    }

    inline bool LoadRadarCalibration(const std::string& mapKey, RadarCalibrationRecord* outRecord)
    {
        if (mapKey.empty() || !outRecord)
            return false;

        const nlohmann::ordered_json root = ReadRoot();
        auto radarIt = root.find("radar_maps");
        if (radarIt == root.end() || !radarIt->is_object())
            return false;

        auto mapIt = radarIt->find(mapKey);
        if (mapIt == radarIt->end() || !mapIt->is_object())
            return false;

        const auto readFloat = [&](const char* key, float fallback) -> float {
            auto valueIt = mapIt->find(key);
            if (valueIt == mapIt->end() || !valueIt->is_number())
                return fallback;
            const float value = valueIt->get<float>();
            return std::isfinite(value) ? value : fallback;
        };

        outRecord->rotationDeg = std::clamp(readFloat("rotation_deg", 0.0f), -180.0f, 180.0f);
        outRecord->scale = std::clamp(readFloat("scale", 1.0f), 0.50f, 1.50f);
        outRecord->baseOffsetX = std::clamp(readFloat("base_offset_x", 0.0f), -0.25f, 0.25f);
        outRecord->baseOffsetY = std::clamp(readFloat("base_offset_y", 0.0f), -0.25f, 0.25f);
        outRecord->offsetX = std::clamp(readFloat("offset_x", 0.0f), -0.25f, 0.25f);
        outRecord->offsetY = std::clamp(readFloat("offset_y", 0.0f), -0.25f, 0.25f);
        outRecord->overviewPosX = readFloat("overview_pos_x", 0.0f);
        outRecord->overviewPosY = readFloat("overview_pos_y", 0.0f);
        outRecord->overviewScale = readFloat("overview_scale", 0.0f);
        outRecord->hasOverview = outRecord->overviewScale > 0.01f;
        return true;
    }

    inline bool SaveRadarCalibration(const std::string& mapKey, const RadarCalibrationRecord& record)
    {
        if (mapKey.empty())
            return false;

        nlohmann::ordered_json root = ReadRoot();
        nlohmann::ordered_json& radarMaps = root["radar_maps"];
        if (!radarMaps.is_object())
            radarMaps = nlohmann::ordered_json::object();

        RadarCalibrationRecord clamped = record;
        clamped.rotationDeg = std::clamp(record.rotationDeg, -180.0f, 180.0f);
        clamped.scale = std::clamp(record.scale, 0.50f, 1.50f);
        clamped.baseOffsetX = std::clamp(record.baseOffsetX, -0.25f, 0.25f);
        clamped.baseOffsetY = std::clamp(record.baseOffsetY, -0.25f, 0.25f);
        clamped.offsetX = std::clamp(record.offsetX, -0.25f, 0.25f);
        clamped.offsetY = std::clamp(record.offsetY, -0.25f, 0.25f);
        clamped.overviewScale = record.hasOverview ? record.overviewScale : 0.0f;
        clamped.hasOverview = record.hasOverview && std::fabs(clamped.overviewScale) > 0.0001f;

        if (IsDefaultRadarCalibration(clamped)) {
            radarMaps.erase(mapKey);
        }
        else {
            radarMaps[mapKey] = {
                { "rotation_deg", clamped.rotationDeg },
                { "scale", clamped.scale },
                { "base_offset_x", clamped.baseOffsetX },
                { "base_offset_y", clamped.baseOffsetY },
                { "offset_x", clamped.offsetX },
                { "offset_y", clamped.offsetY },
                { "overview_pos_x", clamped.overviewPosX },
                { "overview_pos_y", clamped.overviewPosY },
                { "overview_scale", clamped.hasOverview ? clamped.overviewScale : 0.0f },
            };
        }

        PruneRoot(root);

        return WriteRoot(root);
    }
}
