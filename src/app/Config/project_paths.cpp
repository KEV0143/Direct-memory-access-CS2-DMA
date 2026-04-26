#include "app/Config/project_paths.h"

#include <Windows.h>

#include <array>
#include <system_error>

namespace
{
    bool EnsureDirectoryImpl(const std::filesystem::path& directory)
    {
        if (directory.empty())
            return false;

        std::error_code ec;
        std::filesystem::create_directories(directory, ec);
        return !ec || std::filesystem::exists(directory, ec);
    }

    bool LooksLikeProjectRoot(const std::filesystem::path& directory)
    {
        if (directory.empty())
            return false;

        std::error_code ec;
        const bool hasProjectFile = std::filesystem::exists(directory / "KevqDMA.vcxproj", ec);
        ec.clear();
        const bool hasSourceLayout =
            std::filesystem::exists(directory / "src", ec) &&
            std::filesystem::exists(directory / "include", ec) &&
            std::filesystem::exists(directory / "src" / "assets", ec);
        return hasProjectFile || hasSourceLayout;
    }

    std::filesystem::path FindProjectRootFrom(std::filesystem::path current)
    {
        for (int depth = 0; depth < 8 && !current.empty(); ++depth)
        {
            if (LooksLikeProjectRoot(current))
                return current;
            current = current.parent_path();
        }

        return {};
    }
}

std::filesystem::path app::paths::GetExecutableDirectory()
{
    static const std::filesystem::path kExecutableDirectory = []() -> std::filesystem::path {
        wchar_t exePath[MAX_PATH] = {};
        const DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        if (len == 0 || len == MAX_PATH)
            return {};
        return std::filesystem::path(exePath).parent_path();
    }();

    return kExecutableDirectory;
}

std::filesystem::path app::paths::FindProjectRoot()
{
    static const std::filesystem::path kProjectRoot = []() -> std::filesystem::path {
        const auto fromExe = FindProjectRootFrom(GetExecutableDirectory());
        if (!fromExe.empty())
            return fromExe;

        std::error_code ec;
        const auto cwd = std::filesystem::current_path(ec);
        if (!ec)
            return FindProjectRootFrom(cwd);

        return {};
    }();

    return kProjectRoot;
}

std::filesystem::path app::paths::GetRuntimeDataDirectory()
{
    static const std::filesystem::path kDataDirectory = []() -> std::filesystem::path {
        const auto exeDir = GetExecutableDirectory();
        if (exeDir.empty())
            return {};

        const auto dataDir = exeDir / "data";
        EnsureDirectoryImpl(dataDir);
        return dataDir;
    }();

    return kDataDirectory;
}

std::filesystem::path app::paths::GetConfigDirectory()
{
    static const std::filesystem::path kConfigDirectory = []() -> std::filesystem::path {
        const auto directory = GetRuntimeDataDirectory() / "config";
        EnsureDirectoryImpl(directory);
        return directory;
    }();

    return kConfigDirectory;
}

std::filesystem::path app::paths::GetOffsetsDirectory()
{
    static const std::filesystem::path kOffsetsDirectory = []() -> std::filesystem::path {
        const auto directory = GetRuntimeDataDirectory() / "offsets";
        EnsureDirectoryImpl(directory);
        return directory;
    }();

    return kOffsetsDirectory;
}

std::filesystem::path app::paths::GetSettingsDirectory()
{
    static const std::filesystem::path kSettingsDirectory = []() -> std::filesystem::path {
        const auto directory = GetRuntimeDataDirectory() / "settings";
        EnsureDirectoryImpl(directory);
        return directory;
    }();

    return kSettingsDirectory;
}

std::filesystem::path app::paths::GetLegacyProfilesDirectory()
{
    const auto exeDir = GetExecutableDirectory();
    if (exeDir.empty())
        return {};

    return exeDir / "profiles";
}

std::filesystem::path app::paths::ResolveWebRadarAssetDirectory()
{
    static const std::filesystem::path kWebRadarAssetDirectory = []() -> std::filesystem::path {
        const auto exeDir = GetExecutableDirectory();
        const auto projectRoot = FindProjectRoot();

        const std::array<std::filesystem::path, 4> candidates = {
            exeDir / "assets" / "webradar",
            exeDir / ".." / "assets" / "webradar",
            exeDir / ".." / ".." / "assets" / "webradar",
            projectRoot / "src" / "Features" / "WebRadar" / "Assets",
        };

        for (const auto& rawCandidate : candidates)
        {
            if (rawCandidate.empty())
                continue;

            std::error_code ec;
            const auto candidate = std::filesystem::weakly_canonical(rawCandidate, ec);
            const auto& path = ec ? rawCandidate : candidate;
            if (std::filesystem::exists(path, ec) && std::filesystem::is_directory(path, ec))
                return path;
        }

        return {};
    }();

    return kWebRadarAssetDirectory;
}

std::filesystem::path app::paths::ResolveWebRadarAssetPath(const std::filesystem::path& relativePath)
{
    const auto directory = ResolveWebRadarAssetDirectory();
    if (directory.empty())
        return {};

    return directory / relativePath;
}

bool app::paths::EnsureDirectory(const std::filesystem::path& directory)
{
    return EnsureDirectoryImpl(directory);
}
