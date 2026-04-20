#pragma once

#include <filesystem>

namespace app::paths {

std::filesystem::path GetExecutableDirectory();
std::filesystem::path FindProjectRoot();

std::filesystem::path GetRuntimeDataDirectory();
std::filesystem::path GetConfigDirectory();
std::filesystem::path GetOffsetsDirectory();
std::filesystem::path GetSettingsDirectory();
std::filesystem::path GetLegacyProfilesDirectory();

std::filesystem::path ResolveWebRadarAssetDirectory();
std::filesystem::path ResolveWebRadarAssetPath(const std::filesystem::path& relativePath);

bool EnsureDirectory(const std::filesystem::path& directory);

} 
