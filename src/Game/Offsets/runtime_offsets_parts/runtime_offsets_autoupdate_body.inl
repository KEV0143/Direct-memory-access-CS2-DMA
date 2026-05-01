    try
    {
        if (report)
            *report = runtime_offsets::AutoUpdateReport{};

        const auto jsonPath = FindOffsetsJsonPath(true);
        if (jsonPath.empty())
        {
            if (message)
                *message = "Offset sync skipped: offsets path is unavailable.";
            return false;
        }

        Values cachedValues = {};
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
                LoadValuesFromJson(root, cachedValues, &missingKeys, &invalidKeys);
            else
                invalidJson = true;
        }
        else
        {
            const auto legacyIniPath = FindLegacyOffsetsPath("offsets.ini");
            if (!legacyIniPath.empty())
            {
                LoadValuesFromLegacyIni(ParseLegacyIniFile(legacyIniPath), cachedValues, &missingKeys, &invalidKeys);
                loadedFromLegacyIni = true;
            }
        }

        const OffsetState storedState = ReadOffsetState(jsonPath, GetOffsetsStatePath());
        const runtime_offsets::PatchInfo patchBaseline =
            !storedState.lastSeenPatch.patchVersion.empty()
            ? storedState.lastSeenPatch
            : storedState.offsetsPatch;
        if (report)
        {
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

        std::vector<UpstreamRef> candidates;
        candidates.push_back(UpstreamRef{ "a2x/cs2-dumper", "main", "a2x/cs2-dumper@main", {}, 0 });
        std::string prListError;
        AppendOpenPullRequestRefs(candidates, &prListError);

        UpstreamRef* best = nullptr;
        for (auto& ref : candidates)
        {
            std::string err;
            if (!FetchRefInfoJson(ref, &err))
                continue;
            if (best == nullptr || ref.timestamp > best->timestamp)
                best = &ref;
        }

        const bool cacheRequiredOk =
            jsonExistedBeforeSync &&
            !invalidJson &&
            ValidateLoadedValues(cachedValues, true).empty();

        if (best == nullptr)
        {
            if (cacheRequiredOk)
            {
                if (report)
                {
                    report->offsetSource = storedState.selectedSource;
                    report->offsetSourceTimestamp = storedState.selectedSourceTimestamp;
                    report->offsetSourceBuildNumber = storedState.selectedSourceBuildNumber;
                    if (hasCurrentPatch)
                    {
                        report->currentPatchVersionDate = currentPatch.versionDate;
                        report->currentPatchVersionTime = currentPatch.versionTime;
                    }
                }
                if (message)
                    *message = "Offset sync: upstream unreachable" +
                               (prListError.empty() ? std::string() : (" (" + prListError + ")")) +
                               ". Using cached offsets.";
                return false;
            }
            if (message)
                *message = "Offset sync failed: upstream unreachable and no usable cache.";
            return false;
        }

        const std::string sourceDescription = best->label;
        const std::string selectedSourceTimestamp = best->timestamp;
        const int selectedSourceBuildNumber = best->buildNumber;

        const bool selectedSourceUnchanged =
            !storedState.selectedSourceTimestamp.empty() &&
            selectedSourceTimestamp == storedState.selectedSourceTimestamp &&
            sourceDescription == storedState.selectedSource;

        Values updated = cachedValues;
        bool fetchedNewHeaders = false;

        if (!selectedSourceUnchanged || !cacheRequiredOk)
        {
            const auto tempRoot = std::filesystem::temp_directory_path() / "kevqdma_offsets" / "remote_output";
            {
                std::error_code rec;
                std::filesystem::remove_all(tempRoot, rec);
            }

            std::string error;
            if (!DownloadHeadersFromRef(*best, tempRoot, &error))
            {
                if (cacheRequiredOk)
                {
                    if (message)
                        *message = "Offset sync: download from " + sourceDescription +
                                   " failed (" + error + "). Using cached offsets.";
                    return false;
                }
                if (message)
                    *message = "Offset sync failed: " + error + ".";
                return false;
            }

            std::unordered_map<std::string, std::ptrdiff_t> parsedMap;
            if (!ParseOutputDirectory(tempRoot, parsedMap, &error))
            {
                if (message)
                    *message = "Offset sync failed: " + error + ".";
                return false;
            }

            Values fresh = {};
            if (!ExtractRequiredValues(parsedMap, fresh, &error))
            {
                if (message)
                    *message = "Offset sync failed: " + error + ".";
                return false;
            }
            ExtractOptionalValues(parsedMap, fresh);
            updated = fresh;
            fetchedNewHeaders = true;
        }

        const size_t changedKeys = CountValueDifferences(cachedValues, updated);
        const bool needsNormalization = !missingKeys.empty() || !invalidKeys.empty() || invalidJson;
        const bool needsWrite =
            changedKeys > 0 ||
            needsNormalization ||
            !jsonExistedBeforeSync ||
            loadedFromLegacyIni;

        const bool sourcePredatesCurrentPatch =
            hasCurrentPatch &&
            !selectedSourceTimestamp.empty() &&
            SourceTimestampDefinitelyPredatesCurrentPatch(selectedSourceTimestamp, currentPatch);
        const bool currentPatchKnown = hasCurrentPatch && currentPatch.patchBuildNumber > 0;
        const bool offsetsCompatibleWithCurrentPatch =
            currentPatchKnown &&
            selectedSourceBuildNumber > 0 &&
            selectedSourceBuildNumber == currentPatch.patchBuildNumber &&
            !sourcePredatesCurrentPatch;

        if (report)
        {
            report->offsetsUpdated = needsWrite && fetchedNewHeaders;
            report->offsetSource = sourceDescription;
            report->offsetSourceTimestamp = selectedSourceTimestamp;
            report->offsetSourceBuildNumber = selectedSourceBuildNumber;
            report->currentPatchVersionDate = currentPatch.versionDate;
            report->currentPatchVersionTime = currentPatch.versionTime;
            report->offsetSourcePredatesCurrentPatch = sourcePredatesCurrentPatch;
            report->offsetsCompatibleWithCurrentPatch = offsetsCompatibleWithCurrentPatch;
        }

        OffsetState nextState = storedState;
        if (hasCurrentPatch)
            nextState.lastSeenPatch = currentPatch.patch;
        if (offsetsCompatibleWithCurrentPatch)
            nextState.offsetsPatch = currentPatch.patch;
        nextState.selectedSource = sourceDescription;
        nextState.selectedSourceTimestamp = selectedSourceTimestamp;
        nextState.remoteOutputTimestamp = selectedSourceTimestamp;
        nextState.selectedSourceBuildNumber = selectedSourceBuildNumber;

        if (!needsWrite)
        {
            CleanupObsoleteOffsetsIniFiles();
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

        if (!WriteOffsetsJson(jsonPath, updated, &nextState))
        {
            if (message)
                *message = "Offset sync failed: cannot write " + jsonPath.string() + ".";
            return false;
        }
        CleanupObsoleteOffsetsIniFiles();

        if (message)
        {
            std::ostringstream oss;
            oss << "Offset sync: applied " << changedKeys << " updated key(s) from " << sourceDescription << ".";
            if (needsNormalization)
                oss << " Local file was normalized.";
            else if (loadedFromLegacyIni)
                oss << " Legacy offsets.ini was migrated to offsets.json.";
            else if (!jsonExistedBeforeSync)
                oss << " Local offsets.json was created.";
            if (!selectedSourceTimestamp.empty())
                oss << " Source timestamp: " << selectedSourceTimestamp << ".";
            if (hasCurrentPatch && !currentPatch.patch.patchVersion.empty())
                oss << " Patch " << currentPatch.patch.patchVersion << ".";
            else if (!steamPatchError.empty())
                oss << " Steam patch lookup unavailable.";
            *message = oss.str();
        }
        return true;
    }
    catch (const std::exception& e)
    {
        if (message)
            *message = std::string("Offset sync failed: ") + e.what() + ". Using cached offsets.";
        return false;
    }
    catch (...)
    {
        if (message)
            *message = "Offset sync failed. Using cached offsets.";
        return false;
    }
