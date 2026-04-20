    std::filesystem::path GetExeDirectory()
    {
        return app::paths::GetExecutableDirectory();
    }

    std::filesystem::path GetLegacyProfilesDirectory()
    {
        return app::paths::GetLegacyProfilesDirectory();
    }

    std::filesystem::path GetOffsetsDirectory()
    {
        return app::paths::GetOffsetsDirectory();
    }

    std::filesystem::path FindProjectRoot()
    {
        return app::paths::FindProjectRoot();
    }

    bool CopyFileToTargetIfPresent(const std::filesystem::path& sourcePath,
                                   const std::filesystem::path& targetPath)
    {
        if (sourcePath.empty() || targetPath.empty())
            return false;

        std::error_code ec;
        if (!std::filesystem::exists(sourcePath, ec))
            return false;

        std::filesystem::create_directories(targetPath.parent_path(), ec);
        ec.clear();
        std::filesystem::copy_file(sourcePath, targetPath, std::filesystem::copy_options::overwrite_existing, ec);
        return !ec && std::filesystem::exists(targetPath, ec);
    }

    std::vector<std::filesystem::path> CollectLegacyOffsetsCandidates(std::string_view fileName)
    {
        const auto legacyProfilesDir = GetLegacyProfilesDirectory();
        const auto projectRoot = FindProjectRoot();
        const auto offsetsDir = GetOffsetsDirectory();

        std::vector<std::filesystem::path> candidates;
        candidates.reserve(5);

        if (fileName == "offsets.ini")
            candidates.push_back(offsetsDir / std::string(fileName));

        candidates.push_back(legacyProfilesDir / std::string(fileName));
        candidates.push_back(GetExeDirectory() / std::string(fileName));
        candidates.push_back(projectRoot / "sdk" / std::string(fileName));
        candidates.push_back(projectRoot / "include" / "sdk" / std::string(fileName));

        return candidates;
    }

    std::filesystem::path FindLegacyOffsetsPath(std::string_view fileName)
    {
        std::error_code ec;
        for (const auto& legacyPath : CollectLegacyOffsetsCandidates(fileName))
        {
            ec.clear();
            if (std::filesystem::exists(legacyPath, ec))
                return legacyPath;
        }

        return {};
    }

    bool TryMigrateLegacyOffsetsJson(const std::filesystem::path& targetPath)
    {
        for (const auto& legacyPath : CollectLegacyOffsetsCandidates("offsets.json"))
        {
            if (!targetPath.empty() && legacyPath == targetPath)
                continue;
            if (CopyFileToTargetIfPresent(legacyPath, targetPath))
                return true;
        }

        return false;
    }

    std::filesystem::path FindOffsetsJsonPath(bool createIfMissing)
    {
        const auto offsetsDir = GetOffsetsDirectory();
        if (offsetsDir.empty())
            return {};

        const auto offsetsJson = offsetsDir / "offsets.json";
        std::error_code ec;
        if (std::filesystem::exists(offsetsJson, ec))
            return offsetsJson;

        if (TryMigrateLegacyOffsetsJson(offsetsJson))
            return offsetsJson;

        if (createIfMissing)
            return offsetsJson;

        return {};
    }

    std::filesystem::path GetOffsetsStatePath()
    {
        const auto offsetsDir = GetOffsetsDirectory();
        if (offsetsDir.empty())
            return {};
        return offsetsDir / "offsets_state.json";
    }
