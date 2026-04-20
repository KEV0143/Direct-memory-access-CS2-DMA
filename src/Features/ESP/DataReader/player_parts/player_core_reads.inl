    const uint64_t coreNowUs = TickNowUs();
    constexpr uint64_t kCoreTeamRefreshIntervalUs = 250000;
    const bool previousLocalTeamValid = (s_localTeam == 2 || s_localTeam == 3);
    int currentResolvedLocalTeam = 0;
    if (localTeamLiveResolved && (localTeam == 2 || localTeam == 3)) {
        currentResolvedLocalTeam = localTeam;
    } else if (localControllerTeam == 2 || localControllerTeam == 3) {
        currentResolvedLocalTeam = localControllerTeam;
    } else if (localMaskBit >= 0 && localMaskBit < 64 && teamLooksValid(teams[localMaskBit])) {
        currentResolvedLocalTeam = teams[localMaskBit];
    } else if (localControllerMaskBit > 0 &&
               localControllerMaskBit <= 64 &&
               teamLooksValid(teams[localControllerMaskBit - 1])) {
        currentResolvedLocalTeam = teams[localControllerMaskBit - 1];
    }
    const bool currentLocalTeamValid = (currentResolvedLocalTeam == 2 || currentResolvedLocalTeam == 3);
    const bool localTeamLikelySwitched =
        previousLocalTeamValid &&
        currentLocalTeamValid &&
        currentResolvedLocalTeam != s_localTeam;
    if (localTeamLikelySwitched) {
        static uint64_t s_lastHandledLocalTeamSwitchUs = 0;
        static int s_lastHandledLocalTeamFrom = 0;
        static int s_lastHandledLocalTeamTo = 0;
        const bool newTeamSwitchEdge =
            s_lastHandledLocalTeamSwitchUs == 0 ||
            (coreNowUs - s_lastHandledLocalTeamSwitchUs) > 2000000u ||
            s_lastHandledLocalTeamFrom != s_localTeam ||
            s_lastHandledLocalTeamTo != currentResolvedLocalTeam;
        if (newTeamSwitchEdge) {
            s_lastHandledLocalTeamSwitchUs = coreNowUs;
            s_lastHandledLocalTeamFrom = s_localTeam;
            s_lastHandledLocalTeamTo = currentResolvedLocalTeam;
            s_sceneResetSerial.fetch_add(1, std::memory_order_relaxed);
            s_lastSceneResetUs.store(coreNowUs, std::memory_order_relaxed);
            setSceneWarmupState(esp::SceneWarmupState::SceneTransition);
            refreshDmaCaches("local_team_switch", DmaRefreshTier::Repair, true);
            memset(s_playerInvalidReadStreak, 0, sizeof(s_playerInvalidReadStreak));
            memset(s_playerDeathConfirmCount, 0, sizeof(s_playerDeathConfirmCount));
            memset(s_prevRawPlayerPosReady, 0, sizeof(s_prevRawPlayerPosReady));
            memset(s_prevRawPlayerPos, 0, sizeof(s_prevRawPlayerPos));
            DmaLogPrintf(
                "[INFO] Local team switch detected (%d -> %d), rewarming player caches",
                s_localTeam,
                currentResolvedLocalTeam);
        }
        memset(s_cachedCoreTeams, 0, sizeof(s_cachedCoreTeams));
        memset(s_cachedCoreTeamPawns, 0, sizeof(s_cachedCoreTeamPawns));
        memset(s_lastCoreTeamReadUs, 0, sizeof(s_lastCoreTeamReadUs));
        memset(teams, 0, sizeof(teams));
    }
    auto coreTeamRefreshDueAt = [&](uint64_t nowAtUs, int idx) -> bool {
        if (idx < 0 || idx >= 64 || !pawns[idx])
            return false;
        if (s_cachedCoreTeamPawns[idx] != pawns[idx])
            return true;
        if (!teamLooksValid(teams[idx]))
            return true;
        return s_lastCoreTeamReadUs[idx] == 0 ||
               (nowAtUs - s_lastCoreTeamReadUs[idx]) >= kCoreTeamRefreshIntervalUs;
    };
    auto coreStateLooksSane = [&](int idx) -> bool {
        if (idx < 0 || idx >= 64 || !pawns[idx])
            return false;
        const bool teamValid = teamLooksValid(teams[idx]);
        const bool healthRangeValid = healths[idx] >= 0 && healths[idx] <= 100;
        const bool armorRangeValid = armors[idx] >= 0 && armors[idx] <= 100;
        const bool positionValid = isValidWorldPos(positions[idx]);
        const bool deadReadConsistent = (healths[idx] > 0) || (lifeStates[idx] != 0);
        return teamValid && healthRangeValid && armorRangeValid && positionValid && deadReadConsistent;
    };
    
    
    
    
    bool queuedCorePlayerReads = false;
    bool coreTeamReadsQueued[64] = {};
    auto markLiveTeamRead = [&](int idx) {
        if (idx < 0 || idx >= 64 || !pawns[idx])
            return;
        if (teamLooksValid(teams[idx]))
            liveTeamReads[idx] = true;
    };
    
    for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
        const int i = playerResolvedSlots[resolvedIdx];
        if (!pawns[i])
            continue;
        queuedCorePlayerReads = true;
        mem.AddScatterReadRequest(handle, pawns[i] + ofs.C_BaseEntity_m_iHealth, &healths[i], sizeof(int));
        mem.AddScatterReadRequest(handle, pawns[i] + ofs.C_CSPlayerPawn_m_ArmorValue, &armors[i], sizeof(int));
        if (coreTeamRefreshDueAt(coreNowUs, i)) {
            mem.AddScatterReadRequest(handle, pawns[i] + ofs.C_BaseEntity_m_iTeamNum, &teams[i], sizeof(int));
            coreTeamReadsQueued[i] = true;
        }
        mem.AddScatterReadRequest(handle, pawns[i] + ofs.C_BaseEntity_m_lifeState, &lifeStates[i], sizeof(uint8_t));
        mem.AddScatterReadRequest(handle, pawns[i] + ofs.C_BasePlayerPawn_m_vOldOrigin, &positions[i], sizeof(Vector3));
    }

    
    
    
    
    
    static uint8_t s_happyPathInsanityStreak[64] = {};
    static uint64_t s_happyPathResetSerial = 0;
    {
        const uint64_t hpResetSerial = s_sceneResetSerial.load(std::memory_order_relaxed);
        if (s_happyPathResetSerial != hpResetSerial) {
            s_happyPathResetSerial = hpResetSerial;
            memset(s_happyPathInsanityStreak, 0, sizeof(s_happyPathInsanityStreak));
        }
    }
    const bool coreScatterOk =
        !queuedCorePlayerReads || mem.ExecuteReadScatter(handle);

    if (coreScatterOk && queuedCorePlayerReads) {
        const uint64_t recentResetUs = s_lastSceneResetUs.load(std::memory_order_relaxed);
        const auto happyPathWarmupState =
            static_cast<esp::SceneWarmupState>(s_sceneWarmupState.load(std::memory_order_relaxed));
        const bool recentStructuralReset =
            recentResetUs > 0 &&
            coreNowUs > recentResetUs &&
            (coreNowUs - recentResetUs) <= 4000000u;
        const bool allowHappyPathEviction =
            !sceneSettling &&
            happyPathWarmupState == esp::SceneWarmupState::Stable &&
            !recentStructuralReset;
        const uint8_t happyPathEvictionThreshold = recentStructuralReset ? 24u : 8u;
        
        
        for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
            const int i = playerResolvedSlots[resolvedIdx];
            if (!pawns[i])
                continue;
            if (coreTeamReadsQueued[i])
                markLiveTeamRead(i);
            if (coreStateLooksSane(i)) {
                s_happyPathInsanityStreak[i] = 0;
            } else {
                if (s_happyPathInsanityStreak[i] < 255u)
                    ++s_happyPathInsanityStreak[i];
                if (allowHappyPathEviction &&
                    s_happyPathInsanityStreak[i] >= happyPathEvictionThreshold) {
                    s_stalePawnEvictionQueue[i] = true;
                    pawns[i] = 0;
                    healths[i] = 0;
                    positions[i] = {};
                    s_happyPathInsanityStreak[i] = 0;
                }
            }
        }
    }

    if (!coreScatterOk) {
        logUpdateDataIssue("scatter_10", "advanced_player_batch_failed_retry_core");
        memset(velocities, 0, sizeof(velocities));

        for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
            const int i = playerResolvedSlots[resolvedIdx];
            if (!pawns[i])
                continue;
            mem.AddScatterReadRequest(handle, pawns[i] + ofs.C_BaseEntity_m_iHealth, &healths[i], sizeof(int));
            mem.AddScatterReadRequest(handle, pawns[i] + ofs.C_CSPlayerPawn_m_ArmorValue, &armors[i], sizeof(int));
            if (coreTeamRefreshDueAt(coreNowUs, i)) {
                mem.AddScatterReadRequest(handle, pawns[i] + ofs.C_BaseEntity_m_iTeamNum, &teams[i], sizeof(int));
                coreTeamReadsQueued[i] = true;
            }
            mem.AddScatterReadRequest(handle, pawns[i] + ofs.C_BaseEntity_m_lifeState, &lifeStates[i], sizeof(uint8_t));
            mem.AddScatterReadRequest(handle, pawns[i] + ofs.C_BasePlayerPawn_m_vOldOrigin, &positions[i], sizeof(Vector3));
        }

         const bool retryCoreScatterOk = mem.ExecuteReadScatter(handle);
         if (!retryCoreScatterOk) {
            memset(healths, 0, sizeof(healths));
            memset(armors, 0, sizeof(armors));
            memset(lifeStates, 0, sizeof(lifeStates));
            memset(positions, 0, sizeof(positions));
            memcpy(teams, s_cachedCoreTeams, sizeof(teams));

            
            
            
            
            
            static uint8_t s_coreReadFailStreak[64] = {};
            static uint64_t s_coreReadFailResetSerial = 0;
            const auto sceneWarmupState =
                static_cast<esp::SceneWarmupState>(s_sceneWarmupState.load(std::memory_order_relaxed));
            const bool allowStalePawnEviction =
                !sceneSettling &&
                sceneWarmupState == esp::SceneWarmupState::Stable &&
                s_engineInGame.load(std::memory_order_relaxed);
            {
                const uint64_t failResetSerial = s_sceneResetSerial.load(std::memory_order_relaxed);
                if (s_coreReadFailResetSerial != failResetSerial) {
                    s_coreReadFailResetSerial = failResetSerial;
                    memset(s_coreReadFailStreak, 0, sizeof(s_coreReadFailStreak));
                }
            }

            bool recoveredAnyCorePlayer = false;
            for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
                const int i = playerResolvedSlots[resolvedIdx];
                if (!pawns[i])
                    continue;
                if (!isLikelyGamePointer(pawns[i])) {
                    if (allowStalePawnEviction) {
                        s_stalePawnEvictionQueue[i] = true;
                        pawns[i] = 0;
                    }
                    continue;
                }

                int healthValue = 0;
                int armorValue = 0;
                int teamValue = teams[i];
                uint8_t lifeStateValue = 0;
                Vector3 positionValue = {};
                const bool needsTeamRead = coreTeamRefreshDueAt(coreNowUs, i);

                mem.AddScatterReadRequest(handle, pawns[i] + ofs.C_BaseEntity_m_iHealth, &healthValue, sizeof(healthValue));
                mem.AddScatterReadRequest(handle, pawns[i] + ofs.C_CSPlayerPawn_m_ArmorValue, &armorValue, sizeof(armorValue));
                mem.AddScatterReadRequest(handle, pawns[i] + ofs.C_BaseEntity_m_lifeState, &lifeStateValue, sizeof(lifeStateValue));
                mem.AddScatterReadRequest(handle, pawns[i] + ofs.C_BasePlayerPawn_m_vOldOrigin, &positionValue, sizeof(positionValue));
                if (needsTeamRead)
                    mem.AddScatterReadRequest(handle, pawns[i] + ofs.C_BaseEntity_m_iTeamNum, &teamValue, sizeof(teamValue));

                if (!mem.ExecuteReadScatter(handle)) {
                    if (!allowStalePawnEviction) {
                        s_coreReadFailStreak[i] = 0;
                        continue;
                    }
                    if (s_coreReadFailStreak[i] < 255u)
                        ++s_coreReadFailStreak[i];
                    if (s_coreReadFailStreak[i] >= 12u) {
                        s_stalePawnEvictionQueue[i] = true;
                        pawns[i] = 0;
                        s_coreReadFailStreak[i] = 0;
                    }
                    continue;
                }

                const bool teamValueValid = !needsTeamRead || teamLooksValid(teamValue);
                const bool slotCoreLooksSane =
                    teamValueValid &&
                    healthValue >= 0 && healthValue <= 100 &&
                    armorValue >= 0 && armorValue <= 100 &&
                    isValidWorldPos(positionValue) &&
                    ((healthValue > 0) || (lifeStateValue != 0));
                if (!slotCoreLooksSane) {
                    if (!allowStalePawnEviction) {
                        s_coreReadFailStreak[i] = 0;
                        continue;
                    }
                    if (s_coreReadFailStreak[i] < 255u)
                        ++s_coreReadFailStreak[i];
                    if (s_coreReadFailStreak[i] >= 16u) {
                        s_stalePawnEvictionQueue[i] = true;
                        pawns[i] = 0;
                        s_coreReadFailStreak[i] = 0;
                    }
                    continue;
                }

                s_coreReadFailStreak[i] = 0;
                healths[i] = healthValue;
                armors[i] = armorValue;
                lifeStates[i] = lifeStateValue;
                positions[i] = positionValue;
                if (needsTeamRead && teamLooksValid(teamValue)) {
                    teams[i] = teamValue;
                    coreTeamReadsQueued[i] = true;
                    liveTeamReads[i] = true;
                }
                recoveredAnyCorePlayer = true;
            }

            const bool localCoreEvidence =
                (localTeam == 2 || localTeam == 3) ||
                (localControllerPawnHandle != 0u && localControllerPawnHandle != 0xFFFFFFFFu);
            if (!recoveredAnyCorePlayer && !localCoreEvidence)
                logUpdateDataIssue("scatter_10_core", "player_core_state_unavailable_using_cached_snapshot");
        } else {
            for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
                const int i = playerResolvedSlots[resolvedIdx];
                if (!pawns[i] || !coreTeamReadsQueued[i])
                    continue;
                markLiveTeamRead(i);
            }
        }
    }
    for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
        const int i = playerResolvedSlots[resolvedIdx];
        if (!pawns[i]) {
            s_cachedCoreTeams[i] = 0;
            s_cachedCoreTeamPawns[i] = 0;
            s_lastCoreTeamReadUs[i] = 0;
            continue;
        }
        s_cachedCoreTeamPawns[i] = pawns[i];
        if (teamLooksValid(teams[i])) {
            s_cachedCoreTeams[i] = teams[i];
            if (coreTeamReadsQueued[i] || s_lastCoreTeamReadUs[i] == 0)
                s_lastCoreTeamReadUs[i] = coreNowUs;
        } else if (!teamLooksValid(s_cachedCoreTeams[i])) {
            s_lastCoreTeamReadUs[i] = 0;
        }
    }
    
