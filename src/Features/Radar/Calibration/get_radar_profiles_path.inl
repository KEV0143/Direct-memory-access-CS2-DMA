#include "app/Config/project_paths.h"

static std::filesystem::path FindLegacyRadarProfilesPath()
{
    std::error_code ec;
    const std::filesystem::path candidates[] = {
        app::paths::GetSettingsDirectory() / "radar_maps.ini",
        app::paths::GetExecutableDirectory() / "radar_maps.ini",
        app::paths::GetLegacyProfilesDirectory() / "radar_maps.ini",
    };

    for (const auto& candidate : candidates) {
        if (candidate.empty())
            continue;
        if (std::filesystem::exists(candidate, ec))
            return candidate;
        ec.clear();
    }

    return {};
}

static bool LoadLegacyRadarCalibrationFromIniPath(
    const std::filesystem::path& profilePath,
    const std::string& mapKey,
    app::user_state::RadarCalibrationRecord* outRecord)
{
    if (profilePath.empty() || mapKey.empty() || !outRecord)
        return false;
    if (!std::filesystem::exists(profilePath))
        return false;

    const float rotation = ReadIniFloat(mapKey.c_str(), "RotationDeg", 0.0f, profilePath);
    const float scale = ReadIniFloat(mapKey.c_str(), "Scale", 1.0f, profilePath);
    const bool hasBaseOffset =
        HasIniKey(mapKey.c_str(), "BaseOffsetX", profilePath) &&
        HasIniKey(mapKey.c_str(), "BaseOffsetY", profilePath);
    const float storedOffsetX = ReadIniFloat(mapKey.c_str(), "OffsetX", 0.0f, profilePath);
    const float storedOffsetY = ReadIniFloat(mapKey.c_str(), "OffsetY", 0.0f, profilePath);
    const float baseOffsetX = hasBaseOffset ? ReadIniFloat(mapKey.c_str(), "BaseOffsetX", 0.0f, profilePath) : storedOffsetX;
    const float baseOffsetY = hasBaseOffset ? ReadIniFloat(mapKey.c_str(), "BaseOffsetY", 0.0f, profilePath) : storedOffsetY;
    const float userOffsetX = hasBaseOffset ? storedOffsetX : 0.0f;
    const float userOffsetY = hasBaseOffset ? storedOffsetY : 0.0f;
    const bool hasOverview =
        HasIniKey(mapKey.c_str(), "OverviewPosX", profilePath) &&
        HasIniKey(mapKey.c_str(), "OverviewPosY", profilePath) &&
        HasIniKey(mapKey.c_str(), "OverviewScale", profilePath);
    const float overviewPosX = ReadIniFloat(mapKey.c_str(), "OverviewPosX", 0.0f, profilePath);
    const float overviewPosY = ReadIniFloat(mapKey.c_str(), "OverviewPosY", 0.0f, profilePath);
    const float overviewScale = ReadIniFloat(mapKey.c_str(), "OverviewScale", 0.0f, profilePath);

    if (!HasIniKey(mapKey.c_str(), "RotationDeg", profilePath))
        return false;

    outRecord->rotationDeg = std::clamp(rotation, -180.0f, 180.0f);
    outRecord->scale = std::clamp(scale, 0.50f, 1.50f);
    outRecord->baseOffsetX = std::clamp(baseOffsetX, -0.25f, 0.25f);
    outRecord->baseOffsetY = std::clamp(baseOffsetY, -0.25f, 0.25f);
    outRecord->offsetX = std::clamp(userOffsetX, -0.25f, 0.25f);
    outRecord->offsetY = std::clamp(userOffsetY, -0.25f, 0.25f);
    outRecord->hasOverview = hasOverview && overviewScale > 0.01f;
    outRecord->overviewPosX = overviewPosX;
    outRecord->overviewPosY = overviewPosY;
    outRecord->overviewScale = overviewScale;
    return true;
}

static void MigrateLegacyRadarProfilesToUserStateIfNeeded()
{
    static bool attempted = false;
    if (attempted)
        return;
    attempted = true;

    const auto legacyPath = FindLegacyRadarProfilesPath();
    if (legacyPath.empty())
        return;

    std::vector<char> sectionNames(32768, '\0');
    const DWORD copiedChars = GetPrivateProfileSectionNamesA(
        sectionNames.data(),
        static_cast<DWORD>(sectionNames.size()),
        legacyPath.string().c_str());
    if (copiedChars == 0)
        return;

    bool migratedAny = false;
    for (const char* section = sectionNames.data(); *section != '\0'; section += std::strlen(section) + 1) {
        app::user_state::RadarCalibrationRecord existing = {};
        if (app::user_state::LoadRadarCalibration(section, &existing))
            continue;

        app::user_state::RadarCalibrationRecord legacyRecord = {};
        if (!LoadLegacyRadarCalibrationFromIniPath(legacyPath, section, &legacyRecord))
            continue;

        if (app::user_state::SaveRadarCalibration(section, legacyRecord))
            migratedAny = true;
    }

    if (!migratedAny)
        return;

    std::error_code ec;
    std::filesystem::remove(legacyPath, ec);
}
