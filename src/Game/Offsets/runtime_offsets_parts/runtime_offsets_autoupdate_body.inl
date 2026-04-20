    try
    {
        if (report)
            *report = runtime_offsets::AutoUpdateReport{};

        const Values defaults = BuildDefaults();
        const auto jsonPath = FindOffsetsJsonPath(true);
        if (jsonPath.empty())
        {
            if (message)
                *message = "Offset sync skipped: offsets path is unavailable.";
            return false;
        }

        Values localValues = defaults;
        std::vector<std::string> missingKeys;
        std::vector<std::string> invalidKeys;
        std::error_code ec;
        const bool jsonExistedBeforeSync = std::filesystem::exists(jsonPath, ec);
        bool loadedFromLegacyIni = false;
        bool invalidJson = false;

        if (jsonExistedBeforeSync)
        {
            json root;
            if (TryParseOffsetsJsonFile(jsonPath, root))
            {
                LoadValuesFromJson(root, defaults, localValues, &missingKeys, &invalidKeys);
            }
            else
            {
                invalidJson = true;
            }
        }
        else
        {
            const auto legacyIniPath = FindLegacyOffsetsPath("offsets.ini");
            if (!legacyIniPath.empty())
            {
                LoadValuesFromLegacyIni(ParseLegacyIniFile(legacyIniPath), defaults, localValues, &missingKeys, &invalidKeys);
                loadedFromLegacyIni = true;
            }
        }

        const OffsetState storedState = ReadOffsetState(jsonPath, GetOffsetsStatePath());
        const runtime_offsets::PatchInfo patchBaseline =
            !storedState.lastSeenPatch.patchVersion.empty()
            ? storedState.lastSeenPatch
            : storedState.offsetsPatch;
        if (report) {
            report->previousOffsetsPatch = storedState.offsetsPatch;
            report->previousLastSeenPatch = storedState.lastSeenPatch;
        }

        SteamInfSnapshot currentPatch = {};
        std::string steamPatchError;
        const bool hasCurrentPatch = FetchSteamInfSnapshot(currentPatch, &steamPatchError);
        if (report && hasCurrentPatch)
            report->currentPatch = currentPatch.patch;
        if (report && hasCurrentPatch && !patchBaseline.patchVersion.empty())
            report->patchChanged = !PatchInfoEquals(patchBaseline, currentPatch.patch);

        const auto localOutputCandidates = CollectLocalOutputCandidates();
        const auto* preferredLocalCandidate = FindPreferredLocalOutputCandidate(localOutputCandidates);
        const bool hasLocalDump = preferredLocalCandidate != nullptr;
        const std::string localTimestamp = hasLocalDump
            ? preferredLocalCandidate->timestamp
            : std::string();
        const int localBuildNumber = hasLocalDump
            ? preferredLocalCandidate->buildNumber
            : 0;

        std::unordered_map<std::string, std::ptrdiff_t> parsedMap;
        std::string error;
        const auto tempRoot = std::filesystem::temp_directory_path() / "kevqdma_offsets";
        const auto remoteInfoDir = tempRoot / "remote_info";
        std::string remoteTimestamp;
        int remoteBuildNumber = 0;
        {
            std::error_code ec;
            std::filesystem::remove_all(remoteInfoDir, ec);
        }
        if (DownloadOutputInfoFile(remoteInfoDir, &error))
        {
            remoteTimestamp = ReadOutputDirectoryTimestamp(remoteInfoDir);
            remoteBuildNumber = ReadOutputDirectoryBuildNumber(remoteInfoDir);
        }
        error.clear();

        bool useLocalDump = hasLocalDump;
        const bool currentPatchKnown = hasCurrentPatch && currentPatch.patchBuildNumber > 0;
        const bool localMatchesCurrentPatch =
            currentPatchKnown &&
            localBuildNumber > 0 &&
            localBuildNumber == currentPatch.patchBuildNumber;
        const bool remoteMatchesCurrentPatch =
            currentPatchKnown &&
            remoteBuildNumber > 0 &&
            remoteBuildNumber == currentPatch.patchBuildNumber;

        if (localMatchesCurrentPatch != remoteMatchesCurrentPatch)
        {
            useLocalDump = localMatchesCurrentPatch;
        }
        else if (hasLocalDump && !localTimestamp.empty() && !remoteTimestamp.empty())
            useLocalDump = localTimestamp >= remoteTimestamp;
        else if (!hasLocalDump)
            useLocalDump = false;
        else if (localTimestamp.empty() && !remoteTimestamp.empty())
            useLocalDump = false;

        std::string sourceDescription = "local dump";
        std::string selectedSourceTimestamp = localTimestamp;
        int selectedSourceBuildNumber = localBuildNumber;
        if (useLocalDump)
        {
            if (!ParseOutputDirectory(preferredLocalCandidate->directory, parsedMap, &error))
            {
                if (message)
                {
                    std::ostringstream oss;
                    oss << "Offset sync failed from "
                        << DescribeOutputDirectoryCandidate(*preferredLocalCandidate)
                        << ": " << error << ". Using local offsets.";
                    *message = oss.str();
                }
                return false;
            }

            sourceDescription = DescribeOutputDirectoryCandidate(*preferredLocalCandidate);
        }
        else
        {
            const auto remoteOutputDir = tempRoot / "remote_output";
            {
                std::error_code ec;
                std::filesystem::remove_all(remoteOutputDir, ec);
            }
            if (!DownloadOutputDirectoryFiles(remoteOutputDir, true, &error))
            {
                if (hasLocalDump && ParseOutputDirectory(preferredLocalCandidate->directory, parsedMap, &error))
                {
                    sourceDescription = DescribeOutputDirectoryCandidate(*preferredLocalCandidate) + " fallback";
                    selectedSourceTimestamp = localTimestamp;
                    selectedSourceBuildNumber = localBuildNumber;
                }
                else
                {
                    if (message)
                        *message = "Offset sync failed. Using local offsets.";
                    return false;
                }
            }
            else
            {
                if (!ParseOutputDirectory(remoteOutputDir, parsedMap, &error))
                {
                    if (message)
                        *message = "Offset sync failed. Using local offsets.";
                    return false;
                }

                sourceDescription = "GitHub output";
                selectedSourceTimestamp = remoteTimestamp;
                selectedSourceBuildNumber = remoteBuildNumber;
            }
        }

        Values updated = defaults;
        if (!ExtractRequiredValues(parsedMap, updated, &error))
        {
            if (message)
                *message = "Offset sync failed. Using local offsets.";
            return false;
        }
        ExtractOptionalValues(parsedMap, updated);

        const size_t changedKeys = CountValueDifferences(localValues, updated);
        const bool needsNormalization = !missingKeys.empty() || !invalidKeys.empty() || invalidJson;
        const bool needsWrite = changedKeys > 0 || needsNormalization || !jsonExistedBeforeSync || loadedFromLegacyIni;
        const bool sourcePredatesCurrentPatch =
            hasCurrentPatch &&
            !selectedSourceTimestamp.empty() &&
            SourceTimestampDefinitelyPredatesCurrentPatch(selectedSourceTimestamp, currentPatch);
        const bool offsetsCompatibleWithCurrentPatch =
            currentPatchKnown &&
            selectedSourceBuildNumber > 0 &&
            selectedSourceBuildNumber == currentPatch.patchBuildNumber &&
            !sourcePredatesCurrentPatch;
        if (report)
        {
            report->offsetsUpdated = needsWrite;
            report->offsetSource = sourceDescription;
            report->offsetSourceTimestamp = selectedSourceTimestamp;
            report->offsetSourceBuildNumber = selectedSourceBuildNumber;
            report->currentPatchVersionDate = currentPatch.versionDate;
            report->currentPatchVersionTime = currentPatch.versionTime;
            report->offsetSourcePredatesCurrentPatch = sourcePredatesCurrentPatch;
            report->offsetsCompatibleWithCurrentPatch = offsetsCompatibleWithCurrentPatch;
        }

        if (!needsWrite)
        {
            CleanupObsoleteOffsetsIniFiles();
            OffsetState nextState = storedState;
            if (hasCurrentPatch)
                nextState.lastSeenPatch = currentPatch.patch;
            nextState.selectedSource = sourceDescription;
            nextState.selectedSourceTimestamp = selectedSourceTimestamp;
            nextState.remoteOutputTimestamp = remoteTimestamp;
            nextState.selectedSourceBuildNumber = selectedSourceBuildNumber;
            WriteOffsetState(jsonPath, nextState);

            if (message)
            {
                std::ostringstream oss;
                oss << "Offset check complete: " << sourceDescription << " is up to date.";
                if (hasCurrentPatch && !currentPatch.patch.patchVersion.empty())
                    oss << " Patch " << currentPatch.patch.patchVersion << ".";
                else if (!steamPatchError.empty())
                    oss << " Steam patch lookup unavailable.";
                *message = oss.str();
            }
            return true;
        }

        OffsetState nextState = storedState;
        if (hasCurrentPatch)
            nextState.lastSeenPatch = currentPatch.patch;
        if (offsetsCompatibleWithCurrentPatch)
            nextState.offsetsPatch = currentPatch.patch;
        nextState.selectedSource = sourceDescription;
        nextState.selectedSourceTimestamp = selectedSourceTimestamp;
        nextState.remoteOutputTimestamp = remoteTimestamp;
        nextState.selectedSourceBuildNumber = selectedSourceBuildNumber;
        if (!WriteOffsetsJson(jsonPath, updated, &nextState))
        {
            if (message)
                *message = "Offset sync failed. Using local offsets.";
            return false;
        }

        CleanupObsoleteOffsetsIniFiles();
        const bool savedState = true;

        if (message)
        {
            std::ostringstream oss;
            oss << "Offset sync: auto-applied " << changedKeys << " updated key(s) from " << sourceDescription << ".";
            if (needsNormalization)
                oss << " Local file was normalized.";
            else if (loadedFromLegacyIni)
                oss << " Legacy offsets.ini was migrated to offsets.json.";
            else if (!jsonExistedBeforeSync)
                oss << " Local offsets.json was created.";
            if (!remoteTimestamp.empty())
                oss << " Remote timestamp: " << remoteTimestamp << ".";
            if (hasCurrentPatch && !currentPatch.patch.patchVersion.empty())
                oss << " Patch " << currentPatch.patch.patchVersion << ".";
            else if (!steamPatchError.empty())
                oss << " Steam patch lookup unavailable.";
            if (!savedState)
                oss << " Offsets state manifest could not be saved.";
            *message = oss.str();
        }
        return true;
    }
    catch (...)
    {
        if (message)
            *message = "Offset sync failed. Using local offsets.";
        return false;
    }
