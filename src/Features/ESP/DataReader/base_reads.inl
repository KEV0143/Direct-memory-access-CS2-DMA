    int localTeam = s_localTeam;
    bool localTeamLiveResolved = false;
    int localControllerTeam = 0;
    uint32_t localControllerPawnHandle = 0;
    Vector3 localPos = s_localPos;
    float sensValue = s_sensitivity;
    Vector3 minimapMins = s_minimapMins;
    Vector3 minimapMaxs = s_minimapMaxs;
    bool minimapBoundsValid = s_hasMinimapBounds;
    bool bombPlantedByRules = false;
    bool bombDroppedByRules = false;
    uint8_t bombTicking = 0;
    uint8_t bombBeingDefused = 0;
    float bombBlowTime = 0.0f;
    float bombTimerLength = 0.0f;
    float bombDefuseEndTime = 0.0f;
    float bombDefuseLength = 0.0f;
    float currentGameTime = s_lastStableGameTime;
    float intervalPerTick = s_lastStableIntervalPerTick;
    highestEntityIndex = std::max(0, s_highestEntityIdxStat.load(std::memory_order_relaxed));
    uintptr_t bombSceneNode = 0;
    Vector3 bombWorldPos = { NAN, NAN, NAN };
    Vector3 bombCollisionMins = s_bombState.boundsMins;
    Vector3 bombCollisionMaxs = s_bombState.boundsMaxs;
    uintptr_t weaponC4SceneNode = 0;
    Vector3 weaponC4WorldPos = { NAN, NAN, NAN };
    Vector3 weaponC4CollisionMins = {};
    Vector3 weaponC4CollisionMaxs = {};
    uint32_t weaponC4OwnerHandle = 0;
    bool weaponC4PosValid = false;
    char liveMapNameRaw[64] = {};

    auto sanitizePointer = [&](uintptr_t value) -> uintptr_t {
        return isLikelyGamePointer(value) ? value : 0;
    };

    auto readValue = [&](uintptr_t address, void* outValue, size_t size) -> bool {
        if (!address || !outValue || size == 0)
            return false;
        return mem.Read(address, outValue, size);
    };

    auto readPointer = [&](uintptr_t address, uintptr_t* outValue) -> bool {
        uintptr_t value = 0;
        if (!readValue(address, &value, sizeof(value))) {
            if (outValue)
                *outValue = 0;
            return false;
        }
        value = sanitizePointer(value);
        if (outValue)
            *outValue = value;
        return value != 0;
    };

    auto tryReadSaneFloat = [&](uintptr_t baseAddress,
                                const std::ptrdiff_t* candidateOffsets,
                                size_t candidateCount,
                                float minValue,
                                float maxValue,
                                float* outValue) -> bool {
        if (!baseAddress || !candidateOffsets || !candidateCount || !outValue)
            return false;

        for (size_t i = 0; i < candidateCount; ++i) {
            const std::ptrdiff_t offset = candidateOffsets[i];
            if (offset <= 0)
                continue;

            float value = 0.0f;
            if (!readValue(baseAddress + static_cast<uintptr_t>(offset), &value, sizeof(value)))
                continue;
            if (!std::isfinite(value) || value < minValue || value > maxValue)
                continue;

            *outValue = value;
            return true;
        }

        return false;
    };

    
    
    
    
    
    
    static uintptr_t s_cachedEntityList = 0;
    static uintptr_t s_cachedListEntry = 0;
    static uintptr_t s_cbpLocalPawn = 0;
    static uintptr_t s_cbpLocalController = 0;
    static uintptr_t s_cbpGameRules = 0;
    static uintptr_t s_cbpGlobalVars = 0;
    static uintptr_t s_cbpSensPtr = 0;
    static uintptr_t s_cbpPlantedC4 = 0;
    static uintptr_t s_cbpWeaponC4 = 0;
    static uintptr_t s_cbpWeaponC4Raw = 0;
    static uint64_t s_cbpResetSerial = 0;
    static uintptr_t s_pendingEntityList = 0;
    static uint32_t s_pendingEntityListConfirmCount = 0;
    static uint64_t s_lastHighestEntityRefreshUs = 0;
    static uint64_t s_lastMinimapRefreshUs = 0;
    static uint64_t s_lastSensitivityRefreshUs = 0;
    static uint64_t s_lastIntervalRefreshUs = 0;
    static uintptr_t s_pendingGameRulesCandidate = 0;
    static uint32_t s_pendingGameRulesConfirmCount = 0;
    static uint32_t s_gameRulesSanityFailureStreak = 0;
    static uint64_t s_lastGameRulesSanityLogUs = 0;
    static uint64_t s_lastGameRulesSanityRecoveryUs = 0;
    static bool s_entityHierarchyMissing = false;
    static uint32_t s_entityHierarchyMissingStreak = 0;
    static uint64_t s_lastEntityHierarchyRecoveryUs = 0;
    static uint32_t s_entityHierarchyFlatStreak = 0;
    static uint64_t s_lastEntityHierarchyFlatRecoveryUs = 0;
    static uintptr_t s_cachedMatchmakingBase = 0;
    static uint64_t s_lastMatchmakingBaseRefreshUs = 0;
    static uint64_t s_lastLiveMapNameRefreshUs = 0;
    static std::string s_cachedLiveMapKey;
    {
        const uint64_t cbpSceneSerial = s_sceneResetSerial.load(std::memory_order_relaxed);
        if (s_cbpResetSerial != cbpSceneSerial) {
            s_cbpResetSerial = cbpSceneSerial;
            s_cbpLocalPawn = 0; s_cbpLocalController = 0;
            s_cbpGameRules = 0; s_cbpGlobalVars = 0; s_cbpSensPtr = 0;
            s_cbpPlantedC4 = 0; s_cbpWeaponC4 = 0; s_cbpWeaponC4Raw = 0;
            
            
            
            
            
            
            s_cachedEntityList = 0;
            s_cachedListEntry = 0;
            s_pendingEntityList = 0;
            s_pendingEntityListConfirmCount = 0;
            s_lastHighestEntityRefreshUs = 0;
            s_lastMinimapRefreshUs = 0;
            s_lastSensitivityRefreshUs = 0;
            s_lastIntervalRefreshUs = 0;
            s_pendingGameRulesCandidate = 0;
            s_pendingGameRulesConfirmCount = 0;
            s_gameRulesSanityFailureStreak = 0;
            s_lastGameRulesSanityLogUs = 0;
            s_lastGameRulesSanityRecoveryUs = 0;
            
            
            s_entityHierarchyMissing = false;
            s_entityHierarchyMissingStreak = 0;
            s_lastEntityHierarchyRecoveryUs = 0;
            s_entityHierarchyFlatStreak = 0;
            s_lastEntityHierarchyFlatRecoveryUs = 0;
            s_cachedMatchmakingBase = 0;
            s_lastMatchmakingBaseRefreshUs = 0;
            s_lastLiveMapNameRefreshUs = 0;
            s_cachedLiveMapKey.clear();
        }
    }
    {
        const uint64_t baseNowUs = TickNowUs();
        liveMapKey = s_cachedLiveMapKey;
        const int baseSignOnState = s_engineSignOnState.load(std::memory_order_relaxed);
        const bool engineMatchLikeBase =
            s_engineInGame.load(std::memory_order_relaxed) &&
            !s_engineMenu.load(std::memory_order_relaxed) &&
            baseSignOnState == 6;
        const bool highestEntityRefreshDue =
            s_lastHighestEntityRefreshUs == 0 ||
            (baseNowUs - s_lastHighestEntityRefreshUs) >= esp::intervals::kBaseHighestEntityRefreshUs;
        const bool minimapRefreshDue =
            !s_hasMinimapBounds ||
            s_lastMinimapRefreshUs == 0 ||
            (baseNowUs - s_lastMinimapRefreshUs) >= esp::intervals::kBaseMinimapRefreshUs;
        const bool sensitivityRefreshDue =
            s_sensitivity <= 0.0f ||
            s_lastSensitivityRefreshUs == 0 ||
            (baseNowUs - s_lastSensitivityRefreshUs) >= esp::intervals::kBaseSensitivityRefreshUs;
        const bool intervalRefreshDue =
            s_lastStableIntervalPerTick <= 0.0f ||
            s_lastIntervalRefreshUs == 0 ||
            (baseNowUs - s_lastIntervalRefreshUs) >= esp::intervals::kBaseIntervalRefreshUs;
        const bool liveMapNameRefreshDue =
            s_lastLiveMapNameRefreshUs == 0 ||
            (baseNowUs - s_lastLiveMapNameRefreshUs) >= esp::intervals::kBaseMinimapRefreshUs;
        
        uintptr_t rawEntityList = 0, rawLocalPawn = 0, rawLocalController = 0;
        uintptr_t rawGameRules = 0, rawGlobalVars = 0;
        uintptr_t rawPlantedC4 = 0, rawWeaponC4 = 0, rawSensPtr = 0;
        
        uintptr_t rawListEntry = 0;
        int liveLocalTeam = 0;
        uint8_t bombPlantedFlag = 0;
        uint8_t bombDroppedFlag = 0;
        float liveSensitivity = 0.0f;
        static constexpr std::ptrdiff_t kIntervalCandidates[] = { 0x10, 0x14, 0x18, 0x1C };
        static constexpr std::ptrdiff_t kCurrentTimeCandidates[] = { 0x28, 0x2C, 0x30, 0x34, 0x38 };
        float intervalCandidateValues[std::size(kIntervalCandidates)] = {};
        float currentTimeCandidateValues[std::size(kCurrentTimeCandidates)] = {};
        uintptr_t rawPlantedC4Deref = 0;
        uintptr_t rawWeaponC4Deref = 0;


        if (ofs.dwEntityList > 0)
            mem.AddScatterReadRequest(handle, g::clientBase + ofs.dwEntityList, &rawEntityList, sizeof(rawEntityList));
        if (ofs.dwLocalPlayerPawn > 0)
            mem.AddScatterReadRequest(handle, g::clientBase + ofs.dwLocalPlayerPawn, &rawLocalPawn, sizeof(rawLocalPawn));
        if (ofs.dwLocalPlayerController > 0)
            mem.AddScatterReadRequest(handle, g::clientBase + ofs.dwLocalPlayerController, &rawLocalController, sizeof(rawLocalController));
        if (engineMatchLikeBase && ofs.dwGameRules > 0)
            mem.AddScatterReadRequest(handle, g::clientBase + ofs.dwGameRules, &rawGameRules, sizeof(rawGameRules));
        if (ofs.dwGlobalVars > 0)
            mem.AddScatterReadRequest(handle, g::clientBase + ofs.dwGlobalVars, &rawGlobalVars, sizeof(rawGlobalVars));
        if (engineMatchLikeBase && ofs.dwPlantedC4 > 0)
            mem.AddScatterReadRequest(handle, g::clientBase + ofs.dwPlantedC4, &rawPlantedC4, sizeof(rawPlantedC4));
        if (engineMatchLikeBase && ofs.dwWeaponC4 > 0)
            mem.AddScatterReadRequest(handle, g::clientBase + ofs.dwWeaponC4, &rawWeaponC4, sizeof(rawWeaponC4));
        if (ofs.dwSensitivity > 0)
            mem.AddScatterReadRequest(handle, g::clientBase + ofs.dwSensitivity, &rawSensPtr, sizeof(rawSensPtr));
        if (ofs.dwViewMatrix > 0)
            mem.AddScatterReadRequest(handle, g::clientBase + ofs.dwViewMatrix, &viewMatrix, sizeof(viewMatrix));
        if (ofs.dwViewAngles > 0)
            mem.AddScatterReadRequest(handle, g::clientBase + ofs.dwViewAngles, &viewAngles, sizeof(viewAngles));
        if (engineMatchLikeBase && liveMapNameRefreshDue && ofs.dwGameTypes > 0) {
            if (!s_cachedMatchmakingBase ||
                s_lastMatchmakingBaseRefreshUs == 0 ||
                (baseNowUs - s_lastMatchmakingBaseRefreshUs) >= 2000000u) {
                s_cachedMatchmakingBase = mem.GetBaseDaddy("matchmaking.dll");
                s_lastMatchmakingBaseRefreshUs = baseNowUs;
            }
            std::ptrdiff_t mapNameRva = ofs.dwGameTypes + static_cast<std::ptrdiff_t>(0x120);
            if (ofs.dwGameTypes_mapName > ofs.dwGameTypes)
                mapNameRva = ofs.dwGameTypes_mapName;
            else if (ofs.dwGameTypes_mapName > 0 && ofs.dwGameTypes_mapName < 0x10000)
                mapNameRva = ofs.dwGameTypes + ofs.dwGameTypes_mapName;
            if (s_cachedMatchmakingBase && mapNameRva > 0)
                mem.AddScatterReadRequest(handle, s_cachedMatchmakingBase + static_cast<uintptr_t>(mapNameRva), liveMapNameRaw, sizeof(liveMapNameRaw) - 1);
        }

        
        const bool hasCachedPointers = s_cachedEntityList != 0;
        if (hasCachedPointers) {
            mem.AddScatterReadRequest(handle, s_cachedEntityList + 0x10, &rawListEntry, sizeof(rawListEntry));
            if (highestEntityRefreshDue && ofs.dwGameEntitySystem_highestEntityIndex > 0)
                mem.AddScatterReadRequest(handle, s_cachedEntityList + static_cast<uintptr_t>(ofs.dwGameEntitySystem_highestEntityIndex), &highestEntityIndex, sizeof(highestEntityIndex));
        }
        if (s_cbpLocalController && ofs.CCSPlayerController_m_hPlayerPawn > 0)
            mem.AddScatterReadRequest(handle, s_cbpLocalController + ofs.CCSPlayerController_m_hPlayerPawn, &localControllerPawnHandle, sizeof(localControllerPawnHandle));
        if (s_cbpLocalController && ofs.C_BaseEntity_m_iTeamNum > 0)
            mem.AddScatterReadRequest(handle, s_cbpLocalController + ofs.C_BaseEntity_m_iTeamNum, &localControllerTeam, sizeof(localControllerTeam));
        if (s_cbpLocalPawn) {
            if (ofs.C_BaseEntity_m_iTeamNum > 0)
                mem.AddScatterReadRequest(handle, s_cbpLocalPawn + ofs.C_BaseEntity_m_iTeamNum, &liveLocalTeam, sizeof(liveLocalTeam));
            if (ofs.C_BasePlayerPawn_m_vOldOrigin > 0)
                mem.AddScatterReadRequest(handle, s_cbpLocalPawn + ofs.C_BasePlayerPawn_m_vOldOrigin, &localPos, sizeof(localPos));
        }
        if (engineMatchLikeBase && s_cbpGameRules) {
            if (minimapRefreshDue && ofs.C_CSGameRules_m_vMinimapMins > 0)
                mem.AddScatterReadRequest(handle, s_cbpGameRules + ofs.C_CSGameRules_m_vMinimapMins, &minimapMins, sizeof(minimapMins));
            if (minimapRefreshDue && ofs.C_CSGameRules_m_vMinimapMaxs > 0)
                mem.AddScatterReadRequest(handle, s_cbpGameRules + ofs.C_CSGameRules_m_vMinimapMaxs, &minimapMaxs, sizeof(minimapMaxs));
            if (ofs.C_CSGameRules_m_bBombPlanted > 0)
                mem.AddScatterReadRequest(handle, s_cbpGameRules + ofs.C_CSGameRules_m_bBombPlanted, &bombPlantedFlag, sizeof(bombPlantedFlag));
            if (ofs.C_CSGameRules_m_bBombDropped > 0)
                mem.AddScatterReadRequest(handle, s_cbpGameRules + ofs.C_CSGameRules_m_bBombDropped, &bombDroppedFlag, sizeof(bombDroppedFlag));
        }
        if (s_cbpSensPtr && sensitivityRefreshDue && ofs.dwSensitivity_sensitivity > 0)
            mem.AddScatterReadRequest(handle, s_cbpSensPtr + ofs.dwSensitivity_sensitivity, &liveSensitivity, sizeof(liveSensitivity));
        if (engineMatchLikeBase && s_cbpPlantedC4)
            mem.AddScatterReadRequest(handle, s_cbpPlantedC4, &rawPlantedC4Deref, sizeof(uintptr_t));
        if (engineMatchLikeBase && s_cbpWeaponC4Raw)
            mem.AddScatterReadRequest(handle, s_cbpWeaponC4Raw, &rawWeaponC4Deref, sizeof(uintptr_t));
        if (s_cbpGlobalVars) {
            for (size_t i = 0; i < std::size(kIntervalCandidates); ++i) {
                if (intervalRefreshDue && kIntervalCandidates[i] > 0)
                    mem.AddScatterReadRequest(handle, s_cbpGlobalVars + static_cast<uintptr_t>(kIntervalCandidates[i]), &intervalCandidateValues[i], sizeof(float));
            }
            for (size_t i = 0; i < std::size(kCurrentTimeCandidates); ++i) {
                if (kCurrentTimeCandidates[i] > 0)
                    mem.AddScatterReadRequest(handle, s_cbpGlobalVars + static_cast<uintptr_t>(kCurrentTimeCandidates[i]), &currentTimeCandidateValues[i], sizeof(float));
            }
        }

        if (!executeOptionalScatterRead()) {
            logUpdateDataIssue("scatter_1", "base_merged_scatter_failed");
        }
        liveMapNameRaw[sizeof(liveMapNameRaw) - 1] = '\0';
        if (liveMapNameRefreshDue) {
            std::string normalizedMap = radar::NormalizeMapName(liveMapNameRaw);
            if (normalizedMap.empty()) {
                uintptr_t mapNamePointer = 0;
                memcpy(&mapNamePointer, liveMapNameRaw, sizeof(mapNamePointer));
                const bool plausibleStringPointer =
                    mapNamePointer >= 0x10000ULL &&
                    mapNamePointer < 0x0000800000000000ULL;
                if (plausibleStringPointer) {
                    char pointedMapName[64] = {};
                    if (readValue(mapNamePointer, pointedMapName, sizeof(pointedMapName) - 1)) {
                        pointedMapName[sizeof(pointedMapName) - 1] = '\0';
                        normalizedMap = radar::NormalizeMapName(pointedMapName);
                    }
                }
            }
            if (!normalizedMap.empty()) {
                liveMapKey = normalizedMap;
                s_cachedLiveMapKey = normalizedMap;
                s_lastLiveMapNameRefreshUs = baseNowUs;
            }
        }

        
        entityList = sanitizePointer(rawEntityList);
        localPawn = sanitizePointer(rawLocalPawn);
        localController = sanitizePointer(rawLocalController);
        gameRules = engineMatchLikeBase ? sanitizePointer(rawGameRules) : 0;
        globalVars = sanitizePointer(rawGlobalVars);
        plantedC4Entity = engineMatchLikeBase ? sanitizePointer(rawPlantedC4) : 0;
        weaponC4Entity = engineMatchLikeBase ? sanitizePointer(rawWeaponC4) : 0;
        sensPtr = sanitizePointer(rawSensPtr);

        
        const bool pointersChanged =
            !hasCachedPointers ||
            entityList != s_cachedEntityList ||
            localPawn != s_cbpLocalPawn ||
            localController != s_cbpLocalController ||
            gameRules != s_cbpGameRules ||
            globalVars != s_cbpGlobalVars ||
            sensPtr != s_cbpSensPtr ||
            plantedC4Entity != s_cbpPlantedC4 ||
            weaponC4Entity != s_cbpWeaponC4;

        if (pointersChanged) {
            
            const bool highestEntityRefreshForced = highestEntityRefreshDue || entityList != s_cachedEntityList;
            const bool minimapRefreshForced = minimapRefreshDue || gameRules != s_cbpGameRules;
            const bool sensitivityRefreshForced = sensitivityRefreshDue || sensPtr != s_cbpSensPtr;
            const bool intervalRefreshForced = intervalRefreshDue || globalVars != s_cbpGlobalVars;
            rawListEntry = 0; liveLocalTeam = 0; bombPlantedFlag = 0; bombDroppedFlag = 0;
            liveSensitivity = 0.0f; rawPlantedC4Deref = 0; rawWeaponC4Deref = 0;
            localControllerPawnHandle = 0;
            localControllerTeam = 0;
            if (highestEntityRefreshForced)
                highestEntityIndex = 0;
            memset(intervalCandidateValues, 0, sizeof(intervalCandidateValues));
            memset(currentTimeCandidateValues, 0, sizeof(currentTimeCandidateValues));

            if (entityList) {
                mem.AddScatterReadRequest(handle, entityList + 0x10, &rawListEntry, sizeof(rawListEntry));
                if (highestEntityRefreshForced && ofs.dwGameEntitySystem_highestEntityIndex > 0)
                    mem.AddScatterReadRequest(handle, entityList + static_cast<uintptr_t>(ofs.dwGameEntitySystem_highestEntityIndex), &highestEntityIndex, sizeof(highestEntityIndex));
            }
            if (localController && ofs.CCSPlayerController_m_hPlayerPawn > 0)
                mem.AddScatterReadRequest(handle, localController + ofs.CCSPlayerController_m_hPlayerPawn, &localControllerPawnHandle, sizeof(localControllerPawnHandle));
            if (localController && ofs.C_BaseEntity_m_iTeamNum > 0)
                mem.AddScatterReadRequest(handle, localController + ofs.C_BaseEntity_m_iTeamNum, &localControllerTeam, sizeof(localControllerTeam));
            if (localPawn) {
                if (ofs.C_BaseEntity_m_iTeamNum > 0)
                    mem.AddScatterReadRequest(handle, localPawn + ofs.C_BaseEntity_m_iTeamNum, &liveLocalTeam, sizeof(liveLocalTeam));
                if (ofs.C_BasePlayerPawn_m_vOldOrigin > 0)
                    mem.AddScatterReadRequest(handle, localPawn + ofs.C_BasePlayerPawn_m_vOldOrigin, &localPos, sizeof(localPos));
            }
            if (gameRules) {
                if (minimapRefreshForced && ofs.C_CSGameRules_m_vMinimapMins > 0)
                    mem.AddScatterReadRequest(handle, gameRules + ofs.C_CSGameRules_m_vMinimapMins, &minimapMins, sizeof(minimapMins));
                if (minimapRefreshForced && ofs.C_CSGameRules_m_vMinimapMaxs > 0)
                    mem.AddScatterReadRequest(handle, gameRules + ofs.C_CSGameRules_m_vMinimapMaxs, &minimapMaxs, sizeof(minimapMaxs));
                if (ofs.C_CSGameRules_m_bBombPlanted > 0)
                    mem.AddScatterReadRequest(handle, gameRules + ofs.C_CSGameRules_m_bBombPlanted, &bombPlantedFlag, sizeof(bombPlantedFlag));
                if (ofs.C_CSGameRules_m_bBombDropped > 0)
                    mem.AddScatterReadRequest(handle, gameRules + ofs.C_CSGameRules_m_bBombDropped, &bombDroppedFlag, sizeof(bombDroppedFlag));
            }
            if (sensPtr && sensitivityRefreshForced && ofs.dwSensitivity_sensitivity > 0)
                mem.AddScatterReadRequest(handle, sensPtr + ofs.dwSensitivity_sensitivity, &liveSensitivity, sizeof(liveSensitivity));
            if (plantedC4Entity)
                mem.AddScatterReadRequest(handle, plantedC4Entity, &rawPlantedC4Deref, sizeof(uintptr_t));
            if (weaponC4Entity)
                mem.AddScatterReadRequest(handle, weaponC4Entity, &rawWeaponC4Deref, sizeof(uintptr_t));
            if (globalVars) {
                for (size_t i = 0; i < std::size(kIntervalCandidates); ++i) {
                    if (intervalRefreshForced && kIntervalCandidates[i] > 0)
                        mem.AddScatterReadRequest(handle, globalVars + static_cast<uintptr_t>(kIntervalCandidates[i]), &intervalCandidateValues[i], sizeof(float));
                }
                for (size_t i = 0; i < std::size(kCurrentTimeCandidates); ++i) {
                    if (kCurrentTimeCandidates[i] > 0)
                        mem.AddScatterReadRequest(handle, globalVars + static_cast<uintptr_t>(kCurrentTimeCandidates[i]), &currentTimeCandidateValues[i], sizeof(float));
                }
            }
            if (!executeOptionalScatterRead()) {
                logUpdateDataIssue("scatter_2", "dependent_data_refresh_failed");
            }
        }

        listEntry = sanitizePointer(rawListEntry);
        if (localPawn && ofs.C_BaseEntity_m_iTeamNum > 0 && (liveLocalTeam == 2 || liveLocalTeam == 3)) {
            localTeam = liveLocalTeam;
            localTeamLiveResolved = true;
        }
        if (localController && ofs.C_BaseEntity_m_iTeamNum > 0 && (localControllerTeam == 2 || localControllerTeam == 3)) {
            localTeam = localControllerTeam;
            localTeamLiveResolved = true;
        }
        if (gameRules) {
            if (ofs.C_CSGameRules_m_bBombPlanted > 0)
                bombPlantedByRules = bombPlantedFlag != 0;
            if (ofs.C_CSGameRules_m_bBombDropped > 0)
                bombDroppedByRules = bombDroppedFlag != 0;
        }
        minimapBoundsValid = IsBoundsValid(minimapMins, minimapMaxs);
        if (minimapBoundsValid)
            s_lastMinimapRefreshUs = baseNowUs;
        if (sensPtr && ofs.dwSensitivity_sensitivity > 0 &&
            std::isfinite(liveSensitivity) && liveSensitivity > 0.0f && liveSensitivity < 100.0f)
            sensValue = liveSensitivity;
        if (std::isfinite(sensValue) && sensValue > 0.0f && sensValue < 100.0f)
            s_lastSensitivityRefreshUs = baseNowUs;
        if (globalVars) {
            for (size_t i = 0; i < std::size(kIntervalCandidates); ++i) {
                const float v = intervalCandidateValues[i];
                if (std::isfinite(v) && v >= 0.001f && v <= 0.1f) {
                    intervalPerTick = v;
                    s_lastIntervalRefreshUs = baseNowUs;
                    break;
                }
            }
            for (size_t i = 0; i < std::size(kCurrentTimeCandidates); ++i) {
                const float v = currentTimeCandidateValues[i];
                if (std::isfinite(v) && v >= 0.0f && v <= 100000.0f) {
                    currentGameTime = v;
                    break;
                }
            }
        }
        if (!engineMatchLikeBase) {
            gameRules = 0;
            plantedC4Entity = 0;
            weaponC4Entity = 0;
            minimapMins = {};
            minimapMaxs = {};
            minimapBoundsValid = false;
            bombPlantedByRules = false;
            bombDroppedByRules = false;
            s_pendingGameRulesCandidate = 0;
            s_pendingGameRulesConfirmCount = 0;
        }

        auto minimapBoundsLookPlausible = [&](const Vector3& minsToCheck, const Vector3& maxsToCheck) -> bool {
            if (!std::isfinite(minsToCheck.x) || !std::isfinite(minsToCheck.y) ||
                !std::isfinite(maxsToCheck.x) || !std::isfinite(maxsToCheck.y)) {
                return false;
            }

            const float spanX = std::fabs(maxsToCheck.x - minsToCheck.x);
            const float spanY = std::fabs(maxsToCheck.y - minsToCheck.y);
            return spanX > 100.0f && spanY > 100.0f &&
                spanX < 20000.0f && spanY < 20000.0f;
        };

        const auto gameRulesWarmupState =
            static_cast<esp::SceneWarmupState>(s_sceneWarmupState.load(std::memory_order_relaxed));
        const bool stableInGameGameRulesWindow =
            s_engineInGame.load(std::memory_order_relaxed) &&
            gameRules != 0 &&
            gameRulesWarmupState == esp::SceneWarmupState::Stable &&
            (baseNowUs - s_lastSceneResetUs.load(std::memory_order_relaxed)) >= 2000000;
        const bool plantedFlagSane =
            ofs.C_CSGameRules_m_bBombPlanted <= 0 || bombPlantedFlag <= 1u;
        const bool droppedFlagSane =
            ofs.C_CSGameRules_m_bBombDropped <= 0 || bombDroppedFlag <= 1u;
        const bool minimapBoundsSane =
            (ofs.C_CSGameRules_m_vMinimapMins <= 0 || ofs.C_CSGameRules_m_vMinimapMaxs <= 0) ||
            minimapBoundsLookPlausible(minimapMins, minimapMaxs);
        const bool gameRulesLooksSane =
            plantedFlagSane &&
            droppedFlagSane &&
            minimapBoundsSane;
        const bool newGameRulesCandidate =
            stableInGameGameRulesWindow &&
            gameRules != 0 &&
            gameRulesLooksSane &&
            gameRules != s_cbpGameRules;

        if (newGameRulesCandidate) {
            if (s_pendingGameRulesCandidate != gameRules) {
                s_pendingGameRulesCandidate = gameRules;
                s_pendingGameRulesConfirmCount = 1;
            } else if (s_pendingGameRulesConfirmCount < 0xFFFFFFFFu) {
                ++s_pendingGameRulesConfirmCount;
            }

            if (s_pendingGameRulesConfirmCount < 2u) {
                gameRules = s_cbpGameRules;
                minimapMins = s_minimapMins;
                minimapMaxs = s_minimapMaxs;
                minimapBoundsValid = s_hasMinimapBounds;
                bombPlantedByRules = false;
                bombDroppedByRules = false;
            } else {
                s_pendingGameRulesCandidate = 0;
                s_pendingGameRulesConfirmCount = 0;
            }
        } else if (!gameRulesLooksSane || gameRules == 0 || gameRules == s_cbpGameRules) {
            s_pendingGameRulesCandidate = 0;
            s_pendingGameRulesConfirmCount = 0;
        }

        if (stableInGameGameRulesWindow && !gameRulesLooksSane) {
            ++s_gameRulesSanityFailureStreak;

            const bool shouldLog =
                s_gameRulesSanityFailureStreak == 1u ||
                (s_gameRulesSanityFailureStreak % 10u) == 0u ||
                (baseNowUs - s_lastGameRulesSanityLogUs) >= 3000000u;
            if (shouldLog) {
                s_lastGameRulesSanityLogUs = baseNowUs;
                DmaLogPrintf(
                    "[WARN] GameRules sanity failed: ptr=0x%llX dwGameRules=0x%llX mins=(%.1f,%.1f) maxs=(%.1f,%.1f) planted=%u dropped=%u. Suspect stale/bad output.",
                    static_cast<unsigned long long>(gameRules),
                    static_cast<unsigned long long>(ofs.dwGameRules),
                    minimapMins.x,
                    minimapMins.y,
                    minimapMaxs.x,
                    minimapMaxs.y,
                    static_cast<unsigned>(bombPlantedFlag),
                    static_cast<unsigned>(bombDroppedFlag));
            }

            if (s_gameRulesSanityFailureStreak >= 5u &&
                (baseNowUs - s_lastGameRulesSanityRecoveryUs) >= 3000000u) {
                s_lastGameRulesSanityRecoveryUs = baseNowUs;
                refreshDmaCaches("gamerules_sanity_failed", DmaRefreshTier::Repair, true);
                if (s_gameRulesSanityFailureStreak >= 10u) {
                    RequestDmaRecovery("gamerules_sanity_failed_persistent");
                }
            }

            const bool canFallbackToCachedGameRules =
                s_cbpGameRules != 0 &&
                s_cbpGameRules != gameRules &&
                s_hasMinimapBounds;
            gameRules = canFallbackToCachedGameRules ? s_cbpGameRules : 0;
            minimapMins = canFallbackToCachedGameRules ? s_minimapMins : Vector3{};
            minimapMaxs = canFallbackToCachedGameRules ? s_minimapMaxs : Vector3{};
            minimapBoundsValid = canFallbackToCachedGameRules ? s_hasMinimapBounds : false;
            bombPlantedByRules = false;
            bombDroppedByRules = false;
        } else if (!stableInGameGameRulesWindow || gameRulesLooksSane) {
            s_gameRulesSanityFailureStreak = 0;
        }

        if (!engineMatchLikeBase) {
            SetSubsystemUnknown(RuntimeSubsystem::GameRulesMap);
        } else if (!gameRulesLooksSane || !gameRules) {
            if (stableInGameGameRulesWindow)
                MarkSubsystemFailed(RuntimeSubsystem::GameRulesMap, baseNowUs);
            else
                MarkSubsystemDegraded(RuntimeSubsystem::GameRulesMap, baseNowUs);
        } else if (minimapBoundsValid || bombPlantedByRules || bombDroppedByRules) {
            MarkSubsystemHealthy(RuntimeSubsystem::GameRulesMap, baseNowUs);
        } else {
            MarkSubsystemDegraded(RuntimeSubsystem::GameRulesMap, baseNowUs);
        }

        
        s_cbpLocalPawn = localPawn;
        s_cbpLocalController = localController;
        s_cbpGameRules = gameRules;
        s_cbpGlobalVars = globalVars;
        s_cbpSensPtr = sensPtr;
        s_cbpPlantedC4 = plantedC4Entity;
        s_cbpWeaponC4 = weaponC4Entity;
        s_cbpWeaponC4Raw = sanitizePointer(rawWeaponC4);
        
        
        
        if (plantedC4Entity && bombPlantedByRules) {
            const uintptr_t plantedDirect = plantedC4Entity;
            const uintptr_t plantedDeref = sanitizePointer(rawPlantedC4Deref);
            if (plantedDeref)
                plantedC4Entity = plantedDeref;
            else if (plantedDirect)
                plantedC4Entity = plantedDirect;
            else
                plantedC4Entity = 0;
        } else if (!bombPlantedByRules) {
            plantedC4Entity = 0;
        }
        
        if (weaponC4Entity && !bombPlantedByRules) {
            const uintptr_t weaponDeref = sanitizePointer(rawWeaponC4Deref);
            if (weaponDeref)
                weaponC4Entity = weaponDeref;
        }
        if (bombPlantedByRules)
            weaponC4Entity = 0;
        if (entityList && highestEntityIndex > 0)
            s_lastHighestEntityRefreshUs = baseNowUs;
    }

    
    
    
    
    
    if (!entityList && g::clientBase && ofs.dwEntityList > 0) {
        readPointer(g::clientBase + ofs.dwEntityList, &entityList);
    }
    if (entityList && !listEntry) {
        for (int attempt = 0; attempt < 3 && !listEntry; ++attempt) {
            readPointer(entityList + 0x10, &listEntry);
        }
    }



    
    
    
    
    
    bool acceptEntityListCacheUpdate = entityList != 0;
    if (entityList && s_cachedEntityList &&
        entityList != s_cachedEntityList) {
        const auto entityListWarmupState =
            static_cast<esp::SceneWarmupState>(s_sceneWarmupState.load(std::memory_order_relaxed));
        const bool liveEntityHierarchyContext =
            s_engineInGame.load(std::memory_order_relaxed) &&
            !s_engineMenu.load(std::memory_order_relaxed) &&
            s_engineSignOnState.load(std::memory_order_relaxed) == 6;
        const bool stableInGameEntityList =
            liveEntityHierarchyContext &&
            entityListWarmupState == esp::SceneWarmupState::Stable;

        if (!liveEntityHierarchyContext) {
            s_pendingEntityList = 0;
            s_pendingEntityListConfirmCount = 0;
            acceptEntityListCacheUpdate = true;
        } else if (!stableInGameEntityList) {
            if (s_pendingEntityList != entityList) {
                s_pendingEntityList = entityList;
                s_pendingEntityListConfirmCount = 1;
            } else if (s_pendingEntityListConfirmCount < 0xFFFFFFFFu) {
                ++s_pendingEntityListConfirmCount;
            }
            if (s_pendingEntityListConfirmCount >= 2u) {
                refreshDmaCaches("entity_list_rebuilt", DmaRefreshTier::Repair, true);
                s_pendingEntityList = 0;
                s_pendingEntityListConfirmCount = 0;
                acceptEntityListCacheUpdate = true;
            } else {
                acceptEntityListCacheUpdate = false;
            }
        } else {
            acceptEntityListCacheUpdate = false;
            if (s_pendingEntityList != entityList) {
                s_pendingEntityList = entityList;
                s_pendingEntityListConfirmCount = 1;
            } else if (s_pendingEntityListConfirmCount < 0xFFFFFFFFu) {
                ++s_pendingEntityListConfirmCount;
            }

            if (s_pendingEntityListConfirmCount >= 3u) {
                refreshDmaCaches("entity_list_rebuilt_runtime", DmaRefreshTier::Repair, true);
                acceptEntityListCacheUpdate = true;
                s_pendingEntityList = 0;
                s_pendingEntityListConfirmCount = 0;
            }
        }
    } else {
        s_pendingEntityList = 0;
        s_pendingEntityListConfirmCount = 0;
    }

    if (!acceptEntityListCacheUpdate) {
        entityList = s_cachedEntityList;
        listEntry = s_cachedListEntry;
        highestEntityIndex = std::max(0, s_highestEntityIdxStat.load(std::memory_order_relaxed));
    }

    if (acceptEntityListCacheUpdate && entityList)
        s_cachedEntityList = entityList;
    if (listEntry)
        s_cachedListEntry = listEntry;

    if (!entityList)
        highestEntityIndex = 0;
    if (!listEntry)
        highestEntityIndex = 0;

    
    
    const uint64_t baseReadsSceneAge = TickNowUs() - s_lastSceneResetUs.load(std::memory_order_relaxed);
    const auto baseWarmupState =
        static_cast<esp::SceneWarmupState>(s_sceneWarmupState.load(std::memory_order_relaxed));
    const bool sceneSettling =
        baseReadsSceneAge < 2000000 ||
        (baseReadsSceneAge < 10000000 &&
         baseWarmupState != esp::SceneWarmupState::Stable &&
         baseWarmupState != esp::SceneWarmupState::Recovery);
    {
        const bool entityHierarchyReady = entityList != 0 && listEntry != 0;
        const bool liveEntityHierarchyContext =
            s_engineInGame.load(std::memory_order_relaxed) &&
            !s_engineMenu.load(std::memory_order_relaxed) &&
            s_engineSignOnState.load(std::memory_order_relaxed) == 6;
        if (!entityHierarchyReady) {
            if (!liveEntityHierarchyContext) {
                s_entityHierarchyMissing = false;
                s_entityHierarchyMissingStreak = 0;
                s_lastEntityHierarchyRecoveryUs = 0;
                s_entityHierarchyFlatStreak = 0;
                s_lastEntityHierarchyFlatRecoveryUs = 0;
            } else if (baseWarmupState == esp::SceneWarmupState::Recovery ||
                       s_dmaRecovering.load(std::memory_order_relaxed)) {
                s_entityHierarchyMissingStreak = 0;
            } else {
                if (!s_entityHierarchyMissing)
                    refreshDmaCaches("entity_hierarchy_missing", DmaRefreshTier::Probe);

                ++s_entityHierarchyMissingStreak;
                const uint64_t nowUs = TickNowUs();
                if (sceneSettling) {
                    if (s_entityHierarchyMissingStreak >= 3 &&
                        (nowUs - s_lastEntityHierarchyRecoveryUs) >= 500000) {
                        s_lastEntityHierarchyRecoveryUs = nowUs;
                        refreshDmaCaches("entity_hierarchy_settling_repair", DmaRefreshTier::Repair);
                    }
                    if (s_entityHierarchyMissingStreak >= 12 &&
                        (nowUs - s_lastEntityHierarchyRecoveryUs) >= 1000000) {
                        s_lastEntityHierarchyRecoveryUs = nowUs;
                        refreshDmaCaches("entity_hierarchy_settling_full", DmaRefreshTier::Full);
                    }
                    if (s_entityHierarchyMissingStreak >= 24 &&
                        (nowUs - s_lastEntityHierarchyRecoveryUs) >= 3000000) {
                        s_lastEntityHierarchyRecoveryUs = nowUs;
                        refreshDmaCaches("entity_hierarchy_settling_persistent", DmaRefreshTier::Full, true);
                    }
                } else {
                    const bool canAttemptRecovery =
                        s_engineInGame.load(std::memory_order_relaxed) &&
                        s_entityHierarchyMissingStreak >= 24 &&
                        (nowUs - s_lastEntityHierarchyRecoveryUs) >= 5000000;
                    if (canAttemptRecovery) {
                        s_lastEntityHierarchyRecoveryUs = nowUs;
                        refreshDmaCaches("entity_hierarchy_missing_persistent", DmaRefreshTier::Full);
                        RequestDmaRecovery("entity_hierarchy_missing_persistent");
                    }
                }
                s_entityHierarchyMissing = true;
            }
        } else {
            s_entityHierarchyMissing = false;
            s_entityHierarchyMissingStreak = 0;
            const bool flatLiveEntityHierarchy =
                liveEntityHierarchyContext &&
                baseReadsSceneAge >= 1500000u &&
                s_playerSlotScanLimitStat.load(std::memory_order_relaxed) >= 32 &&
                s_activePlayerCount.load(std::memory_order_relaxed) == 0 &&
                highestEntityIndex > 0 &&
                highestEntityIndex < 32;
            if (flatLiveEntityHierarchy &&
                baseWarmupState != esp::SceneWarmupState::Recovery &&
                !s_dmaRecovering.load(std::memory_order_relaxed)) {
                ++s_entityHierarchyFlatStreak;
                const uint64_t nowUs = TickNowUs();
                const bool flatCooldownElapsed =
                    s_lastEntityHierarchyFlatRecoveryUs == 0 ||
                    nowUs <= s_lastEntityHierarchyFlatRecoveryUs ||
                    (nowUs - s_lastEntityHierarchyFlatRecoveryUs) >= 900000u;
                if (flatCooldownElapsed && s_entityHierarchyFlatStreak >= 3u) {
                    s_lastEntityHierarchyFlatRecoveryUs = nowUs;
                    const bool repairFlat = s_entityHierarchyFlatStreak >= 8u;
                    const bool fullFlat = s_entityHierarchyFlatStreak >= 18u;
                    if (repairFlat)
                        SetSceneWarmupState(esp::SceneWarmupState::HierarchyWarming, nowUs);
                    refreshDmaCaches(
                        fullFlat ? "entity_hierarchy_flat_full" :
                        repairFlat ? "entity_hierarchy_flat_repair" :
                        "entity_hierarchy_flat_probe",
                        fullFlat ? DmaRefreshTier::Full :
                        repairFlat ? DmaRefreshTier::Repair :
                        DmaRefreshTier::Probe,
                        fullFlat);
                }
            } else {
                s_entityHierarchyFlatStreak = 0;
                s_lastEntityHierarchyFlatRecoveryUs = 0;
            }
        }
    }

    

    const bool canReadEntityHierarchy = entityList != 0 && listEntry != 0;
