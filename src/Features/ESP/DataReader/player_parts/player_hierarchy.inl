        auto isValidPawnHandle = [&](uint32_t handleValue) -> bool {
            const uint32_t slot = handleValue & kEntityHandleMask;
            return handleValue != 0u &&
                   handleValue != 0xFFFFFFFFu &&
                   slot != 0u &&
                   slot != kEntityHandleMask;
        };
        
        
        static uintptr_t s_cachedControllers[64] = {};
        static uint32_t s_cachedPawnHandles[64] = {};
        static uintptr_t s_cachedPawnEntries[64] = {};
        static uintptr_t s_cachedPawns[64] = {};

        
        
        static uint64_t s_hierarchyCacheResetSerial = 0;
        static bool s_controllerCacheWarmed = false;
        static uint64_t s_lastHierarchyRefreshUs = 0;
        static int s_playerDiscoveryCursor = 0;
        static uint32_t s_zeroControllerStreak = 0;
        static uint32_t s_partialHierarchyStreak = 0;
        static uint64_t s_lastPartialHierarchyRecoveryUs = 0;
        const uint64_t controllerResetSerial = s_sceneResetSerial.load(std::memory_order_relaxed);
        if (s_hierarchyCacheResetSerial != controllerResetSerial) {
            s_hierarchyCacheResetSerial = controllerResetSerial;
            s_controllerCacheWarmed = false;
            s_lastHierarchyRefreshUs = 0;
            s_playerDiscoveryCursor = 0;
            s_zeroControllerStreak = 0;
            s_partialHierarchyStreak = 0;
            s_lastPartialHierarchyRecoveryUs = 0;
            memset(s_cachedControllers, 0, sizeof(s_cachedControllers));
            memset(s_cachedPawnHandles, 0, sizeof(s_cachedPawnHandles));
            memset(s_cachedPawnEntries, 0, sizeof(s_cachedPawnEntries));
            memset(s_cachedPawns, 0, sizeof(s_cachedPawns));
        }

        for (int i = 0; i < 64; ++i) {
            if (s_stalePawnEvictionQueue[i]) {
                s_stalePawnEvictionQueue[i] = false;
                s_cachedPawns[i] = 0;
                s_cachedPawnHandles[i] = 0;
                s_cachedPawnEntries[i] = 0;
                s_cachedControllers[i] = 0;
            }
        }

        auto sceneWarmupState =
            static_cast<esp::SceneWarmupState>(s_sceneWarmupState.load(std::memory_order_relaxed));
        const bool hierarchyWarmupActive =
            !s_controllerCacheWarmed ||
            sceneSettling ||
            sceneWarmupState != esp::SceneWarmupState::Stable;

        const uint64_t hierarchyNowUs = TickNowUs();
        
        
        
        
        
        constexpr uint64_t kHierarchyWarmupIntervalUs  = 6000;
        constexpr uint64_t kHierarchySteadyIntervalUs  = 22000;  
        const uint64_t hierarchyRefreshIntervalUs =
            hierarchyWarmupActive
            ? kHierarchyWarmupIntervalUs
            : kHierarchySteadyIntervalUs;
        const bool hierarchyRefreshDue =
            !s_controllerCacheWarmed ||
            s_lastHierarchyRefreshUs == 0 ||
            (hierarchyNowUs - s_lastHierarchyRefreshUs) >= hierarchyRefreshIntervalUs;

        uintptr_t prevCachedControllers[64] = {};
        uintptr_t prevCachedPawnEntries[64] = {};
        memcpy(prevCachedControllers, s_cachedControllers, sizeof(prevCachedControllers));
        memcpy(prevCachedPawnEntries, s_cachedPawnEntries, sizeof(prevCachedPawnEntries));
        memcpy(controllers, s_cachedControllers, sizeof(controllers));
        memcpy(pawnHandles, s_cachedPawnHandles, sizeof(pawnHandles));
        memcpy(pawnEntries, s_cachedPawnEntries, sizeof(pawnEntries));
        memcpy(pawns, s_cachedPawns, sizeof(pawns));

        int playerRefreshSlots[64] = {};
        int playerRefreshSlotCount = 0;
        bool playerRefreshSlotMask[64] = {};
        auto pushRefreshSlot = [&](int idx) {
            if (idx < 0 || idx >= 64 || playerRefreshSlotMask[idx])
                return;
            playerRefreshSlotMask[idx] = true;
            playerRefreshSlots[playerRefreshSlotCount++] = idx;
        };
        for (int i = 0; i < 64; ++i) {
            const bool trackedSlot =
                s_players[i].valid ||
                s_players[i].pawn != 0 ||
                s_webRadarPlayers[i].valid ||
                s_webRadarPlayers[i].pawn != 0 ||
                s_cachedControllers[i] != 0 ||
                s_cachedPawns[i] != 0;
            if (trackedSlot)
                pushRefreshSlot(i);
        }
        if (localPlayerSlotHint > 0)
            pushRefreshSlot(localPlayerSlotHint - 1);

        constexpr int kPlayerDiscoverySlotLimit = 64;
        if (kPlayerDiscoverySlotLimit > 0) {
            
            
            
            
            
            
            
            const int discoveryBudget =
                !s_controllerCacheWarmed
                ? 64
                : forceFullPlayerDiscoverySweep
                ? std::clamp(std::max(12, activePlayerHint + 8), 12, 24)
                : std::clamp(std::max(4, activePlayerHint / 2 + 4), 4, 12);
            int scanned = 0;
            while (scanned < kPlayerDiscoverySlotLimit &&
                   playerRefreshSlotCount < 64 &&
                   scanned < discoveryBudget) {
                const int idx = (s_playerDiscoveryCursor + scanned) % kPlayerDiscoverySlotLimit;
                if (!playerRefreshSlotMask[idx])
                    pushRefreshSlot(idx);
                ++scanned;
            }
            s_playerDiscoveryCursor = (s_playerDiscoveryCursor + std::max(1, scanned)) % kPlayerDiscoverySlotLimit;
        } else {
            s_playerDiscoveryCursor = 0;
        }

        if (hierarchyRefreshDue && playerRefreshSlotCount > 0) {
            bool hadCachedControllerReads = false;
            for (int slotIdx = 0; slotIdx < playerRefreshSlotCount; ++slotIdx) {
                const int i = playerRefreshSlots[slotIdx];
                const int controllerSlot = i + 1;
                mem.AddScatterReadRequest(handle, listEntry + kEntitySlotSize * (controllerSlot & kEntitySlotMask), &controllers[i], sizeof(uintptr_t));
                if (!prevCachedControllers[i])
                    continue;
                mem.AddScatterReadRequest(handle, prevCachedControllers[i] + ofs.CCSPlayerController_m_hPlayerPawn, &pawnHandles[i], sizeof(uint32_t));
                hadCachedControllerReads = true;
            }
            if (!mem.ExecuteReadScatter(handle)) {
                logUpdateDataIssue("scatter_5_6", "controller_array_unavailable_using_cached_controllers");
            }

            for (int slotIdx = 0; slotIdx < playerRefreshSlotCount; ++slotIdx) {
                const int i = playerRefreshSlots[slotIdx];
                if (!isLikelyGamePointer(controllers[i]))
                    controllers[i] = 0;
            }

            if (!s_controllerCacheWarmed || forceFullPlayerDiscoverySweep) {
                bool anyDiscoveryRetry = false;
                for (int slotIdx = 0; slotIdx < playerRefreshSlotCount; ++slotIdx) {
                    const int i = playerRefreshSlots[slotIdx];
                    if (controllers[i])
                        continue;
                    const int controllerSlot = i + 1;
                    mem.AddScatterReadRequest(handle,
                        listEntry + kEntitySlotSize * (controllerSlot & kEntitySlotMask),
                        &controllers[i], sizeof(uintptr_t));
                    anyDiscoveryRetry = true;
                }
                if (anyDiscoveryRetry)
                    executeOptionalScatterRead();
                for (int slotIdx = 0; slotIdx < playerRefreshSlotCount; ++slotIdx) {
                    const int i = playerRefreshSlots[slotIdx];
                    if (!isLikelyGamePointer(controllers[i]))
                        controllers[i] = 0;
                }
            } else {
                bool anyControllerRepair = false;
                for (int slotIdx = 0; slotIdx < playerRefreshSlotCount; ++slotIdx) {
                    const int i = playerRefreshSlots[slotIdx];
                    if (controllers[i] || !prevCachedControllers[i])
                        continue;
                    const int controllerSlot = i + 1;
                    mem.AddScatterReadRequest(handle,
                        listEntry + kEntitySlotSize * (controllerSlot & kEntitySlotMask),
                        &controllers[i], sizeof(uintptr_t));
                    anyControllerRepair = true;
                }
                if (anyControllerRepair) {
                    executeOptionalScatterRead();
                    for (int slotIdx = 0; slotIdx < playerRefreshSlotCount; ++slotIdx) {
                        const int i = playerRefreshSlots[slotIdx];
                        if (!isLikelyGamePointer(controllers[i]))
                            controllers[i] = 0;
                    }
                }
            }

            {
                bool anyNew = false;
                int cachedCount = 0;
                for (int i = 0; i < 64; ++i) {
                    s_cachedControllers[i] = controllers[i];
                    if (controllers[i]) {
                        anyNew = true;
                    }
                    if (s_cachedControllers[i])
                        ++cachedCount;
                }

                if (!s_controllerCacheWarmed) {
                    const int warmupTargetControllers = std::clamp(
                        (maxClientsHint > 0)
                            ? std::min(maxClientsHint, 8)
                            : std::max(4, activePlayerHint + 2),
                        4, 8);
                    const bool localControllerReady =
                        localPlayerSlotHint <= 0 ||
                        (localPlayerSlotHint <= 64 && s_cachedControllers[localPlayerSlotHint - 1] != 0);
                    if (cachedCount >= warmupTargetControllers && localControllerReady) {
                        s_controllerCacheWarmed = true;
                        if (sceneWarmupState != esp::SceneWarmupState::Stable) {
                            setSceneWarmupState(esp::SceneWarmupState::Stable);
                            sceneWarmupState = esp::SceneWarmupState::Stable;
                        }
                    } else if (sceneWarmupState != esp::SceneWarmupState::SceneTransition) {
                        setSceneWarmupState(esp::SceneWarmupState::HierarchyWarming);
                        sceneWarmupState = esp::SceneWarmupState::HierarchyWarming;
                    }
                }

                if (!anyNew) {
                    ++s_zeroControllerStreak;
                    if (!sceneSettling && s_zeroControllerStreak >= 4 && (s_zeroControllerStreak % 4) == 0) {
                        setSceneWarmupState(esp::SceneWarmupState::Recovery);
                        sceneWarmupState = esp::SceneWarmupState::Recovery;
                        refreshDmaCaches("stale_tlb_no_controllers", DmaRefreshTier::Probe);
                    }
                } else {
                    s_zeroControllerStreak = 0;
                }
            }

            {
                bool controllersChanged = !hadCachedControllerReads;
                if (!controllersChanged) {
                    for (int slotIdx = 0; slotIdx < playerRefreshSlotCount; ++slotIdx) {
                        const int i = playerRefreshSlots[slotIdx];
                        if (controllers[i] != prevCachedControllers[i] ||
                            (controllers[i] && pawnHandles[i] != s_cachedPawnHandles[i]) ||
                            (controllers[i] && !isValidPawnHandle(pawnHandles[i]))) {
                            controllersChanged = true;
                            break;
                        }
                    }
                }
                if (controllersChanged) {
                    bool queuedFreshControllerReads = false;
                    for (int slotIdx = 0; slotIdx < playerRefreshSlotCount; ++slotIdx) {
                        const int i = playerRefreshSlots[slotIdx];
                        if (!controllers[i])
                            continue;
                        mem.AddScatterReadRequest(handle, controllers[i] + ofs.CCSPlayerController_m_hPlayerPawn, &pawnHandles[i], sizeof(uint32_t));
                        queuedFreshControllerReads = true;
                    }
                    if (queuedFreshControllerReads && !mem.ExecuteReadScatter(handle))
                        logUpdateDataIssue("scatter_6_refresh", "controller_pawn_data_refresh_failed");
                }
            }

            
            
            
            
            for (int slotIdx = 0; slotIdx < playerRefreshSlotCount; ++slotIdx) {
                const int i = playerRefreshSlots[slotIdx];
                if (controllers[i])
                    continue;
                pawnHandles[i] = 0;
                pawnEntries[i] = 0;
                pawns[i] = 0;
            }

            {
                bool anyPawnHandleRepair = false;
                for (int slotIdx = 0; slotIdx < playerRefreshSlotCount; ++slotIdx) {
                    const int i = playerRefreshSlots[slotIdx];
                    if (!controllers[i] || isValidPawnHandle(pawnHandles[i]))
                        continue;
                    mem.AddScatterReadRequest(handle,
                        controllers[i] + ofs.CCSPlayerController_m_hPlayerPawn,
                        &pawnHandles[i], sizeof(uint32_t));
                    anyPawnHandleRepair = true;
                }
                if (anyPawnHandleRepair)
                    executeOptionalScatterRead();
            }

            for (int slotIdx = 0; slotIdx < playerRefreshSlotCount; ++slotIdx) {
                const int i = playerRefreshSlots[slotIdx];
                if (isValidPawnHandle(pawnHandles[i]))
                    continue;
                pawnEntries[i] = 0;
                pawns[i] = 0;
            }

            bool queuedMergedPawnReads = false;
            for (int slotIdx = 0; slotIdx < playerRefreshSlotCount; ++slotIdx) {
                const int i = playerRefreshSlots[slotIdx];
                if (!isValidPawnHandle(pawnHandles[i]))
                    continue;
                const uint32_t block = (pawnHandles[i] & kEntityHandleMask) >> 9;
                mem.AddScatterReadRequest(handle, entityList + 0x10 + 8 * block, &pawnEntries[i], sizeof(uintptr_t));
                queuedMergedPawnReads = true;
            }
            for (int slotIdx = 0; slotIdx < playerRefreshSlotCount; ++slotIdx) {
                const int i = playerRefreshSlots[slotIdx];
                if (!prevCachedPawnEntries[i] || !isValidPawnHandle(pawnHandles[i]))
                    continue;
                const uint32_t slot = pawnHandles[i] & kEntitySlotMask;
                mem.AddScatterReadRequest(handle, prevCachedPawnEntries[i] + kEntitySlotSize * slot, &pawns[i], sizeof(uintptr_t));
                queuedMergedPawnReads = true;
            }
            if (queuedMergedPawnReads && !mem.ExecuteReadScatter(handle))
                logUpdateDataIssue("scatter_8_9", "entries_pawns_unavailable");

            for (int slotIdx = 0; slotIdx < playerRefreshSlotCount; ++slotIdx) {
                const int i = playerRefreshSlots[slotIdx];
                if (!isLikelyGamePointer(pawnEntries[i]))
                    pawnEntries[i] = 0;
            }

            {
                bool anyPawnEntryRepair = false;
                for (int slotIdx = 0; slotIdx < playerRefreshSlotCount; ++slotIdx) {
                    const int i = playerRefreshSlots[slotIdx];
                    if (pawnEntries[i] || !isValidPawnHandle(pawnHandles[i]))
                        continue;
                    const uint32_t block = (pawnHandles[i] & kEntityHandleMask) >> 9;
                    mem.AddScatterReadRequest(handle,
                        entityList + 0x10 + 8 * block,
                        &pawnEntries[i], sizeof(uintptr_t));
                    anyPawnEntryRepair = true;
                }
                if (anyPawnEntryRepair) {
                    executeOptionalScatterRead();
                    for (int slotIdx = 0; slotIdx < playerRefreshSlotCount; ++slotIdx) {
                        const int i = playerRefreshSlots[slotIdx];
                        if (!isLikelyGamePointer(pawnEntries[i]))
                            pawnEntries[i] = 0;
                    }
                }
            }

            for (int slotIdx = 0; slotIdx < playerRefreshSlotCount; ++slotIdx) {
                const int i = playerRefreshSlots[slotIdx];
                if (pawnEntries[i])
                    continue;
                pawns[i] = 0;
            }

            {
                bool entriesChanged = false;
                for (int slotIdx = 0; slotIdx < playerRefreshSlotCount; ++slotIdx) {
                    const int i = playerRefreshSlots[slotIdx];
                    if (pawnEntries[i] != prevCachedPawnEntries[i]) {
                        entriesChanged = true;
                        break;
                    }
                }
                if (entriesChanged) {
                    bool queuedFreshPawnReads = false;
                    for (int slotIdx = 0; slotIdx < playerRefreshSlotCount; ++slotIdx) {
                        const int i = playerRefreshSlots[slotIdx];
                        if (!pawnEntries[i] || !isValidPawnHandle(pawnHandles[i]))
                            continue;
                        const uint32_t slot = pawnHandles[i] & kEntitySlotMask;
                        mem.AddScatterReadRequest(handle, pawnEntries[i] + kEntitySlotSize * slot, &pawns[i], sizeof(uintptr_t));
                        queuedFreshPawnReads = true;
                    }
                    if (queuedFreshPawnReads && !mem.ExecuteReadScatter(handle))
                        logUpdateDataIssue("scatter_9_refresh", "pawn_pointers_refresh_failed");
                }
            }

            for (int slotIdx = 0; slotIdx < playerRefreshSlotCount; ++slotIdx) {
                const int i = playerRefreshSlots[slotIdx];
                if (!isLikelyGamePointer(pawns[i]))
                    pawns[i] = 0;
            }
            {
                bool anyPawnRepair = false;
                for (int slotIdx = 0; slotIdx < playerRefreshSlotCount; ++slotIdx) {
                    const int i = playerRefreshSlots[slotIdx];
                    if (pawns[i] || !pawnEntries[i] || !isValidPawnHandle(pawnHandles[i]))
                        continue;
                    const uint32_t slot = pawnHandles[i] & kEntitySlotMask;
                    mem.AddScatterReadRequest(handle,
                        pawnEntries[i] + kEntitySlotSize * slot,
                        &pawns[i], sizeof(uintptr_t));
                    anyPawnRepair = true;
                }
                if (anyPawnRepair) {
                    executeOptionalScatterRead();
                    for (int slotIdx = 0; slotIdx < playerRefreshSlotCount; ++slotIdx) {
                        const int i = playerRefreshSlots[slotIdx];
                        if (!isLikelyGamePointer(pawns[i]))
                            pawns[i] = 0;
                    }
                }
            }

            for (int slotIdx = 0; slotIdx < playerRefreshSlotCount; ++slotIdx) {
                const int i = playerRefreshSlots[slotIdx];

                s_cachedPawns[i] = pawns[i];
                s_cachedPawnHandles[i] = pawnHandles[i];
                s_cachedPawnEntries[i] = pawnEntries[i];
            }
            s_lastHierarchyRefreshUs = hierarchyNowUs;

            {
                int highestHierarchySlot = 0;
                for (int i = 63; i >= 0; --i) {
                    if (controllers[i] ||
                        isValidPawnHandle(pawnHandles[i]) ||
                        pawns[i] ||
                        s_cachedControllers[i] ||
                        s_cachedPawns[i]) {
                        highestHierarchySlot = i + 1;
                        break;
                    }
                }
                s_playerHierarchyHighWaterSlot.store(highestHierarchySlot, std::memory_order_relaxed);
                const int resolvedPlayerSlotLimit = std::max(playerSlotScanLimit, highestHierarchySlot);
                int controllerCount = 0;
                int pawnHandleCount = 0;
                int pawnCount = 0;
                for (int i = 0; i < resolvedPlayerSlotLimit; ++i) {
                    if (controllers[i])
                        ++controllerCount;
                    if (isValidPawnHandle(pawnHandles[i]))
                        ++pawnHandleCount;
                    if (pawns[i])
                        ++pawnCount;
                }

                const bool localHierarchyEvidence =
                    localPawn != 0 ||
                    (localControllerPawnHandle != 0u && localControllerPawnHandle != 0xFFFFFFFFu);
                const uint64_t recentResetUs = s_lastSceneResetUs.load(std::memory_order_relaxed);
                const bool recentStructuralReset =
                    recentResetUs > 0 &&
                    hierarchyNowUs > recentResetUs &&
                    (hierarchyNowUs - recentResetUs) <= 4000000u;
                const int hierarchyMissingTolerance = recentStructuralReset ? 0 : 2;
                const uint32_t partialHierarchyThreshold = recentStructuralReset ? 24u : 60u;
                const uint32_t partialHierarchyCadence = recentStructuralReset ? 12u : 30u;
                const bool hierarchyLooksPartial =
                    controllerCount >= 2 &&
                    pawnHandleCount >= 2 &&
                    pawnCount + hierarchyMissingTolerance < pawnHandleCount;
                if (hierarchyLooksPartial && localHierarchyEvidence) {
                    ++s_partialHierarchyStreak;
                    if (!sceneSettling &&
                        s_partialHierarchyStreak >= partialHierarchyThreshold &&
                        (s_partialHierarchyStreak % partialHierarchyCadence) == 0u) {
                        refreshDmaCaches(
                            "partial_player_hierarchy",
                            recentStructuralReset ? DmaRefreshTier::Repair : DmaRefreshTier::Probe);
                    }
                } else {
                    s_partialHierarchyStreak = 0;
                }
            }
        }

        int highestHierarchySlot = 0;
        for (int i = 63; i >= 0; --i) {
            if (controllers[i] ||
                isValidPawnHandle(pawnHandles[i]) ||
                pawns[i] ||
                s_cachedControllers[i] ||
                s_cachedPawns[i]) {
                highestHierarchySlot = i + 1;
                break;
            }
        }
        s_playerHierarchyHighWaterSlot.store(highestHierarchySlot, std::memory_order_relaxed);
        const int resolvedPlayerSlotLimit = std::max(playerSlotScanLimit, highestHierarchySlot);

        for (int i = 0; i < resolvedPlayerSlotLimit; ++i) {
            if (pawns[i] == localPawn) {
                localMaskBit = i;
                localMaskSlotBit = i + 1;
                const int handleSlot = static_cast<int>(pawnHandles[i] & kEntitySlotMask);
                localHandleSlotBit = handleSlot;
                break;
            }
        }
        if (localMaskBit < 0) {
            for (int i = resolvedPlayerSlotLimit; i < 64; ++i) {
                if (pawns[i] == localPawn) {
                    localMaskBit = i;
                    localMaskSlotBit = i + 1;
                    const int handleSlot = static_cast<int>(pawnHandles[i] & kEntitySlotMask);
                    localHandleSlotBit = handleSlot;
                    break;
                }
            }
        }
        if (localController) {
            for (int i = 0; i < resolvedPlayerSlotLimit; ++i) {
                if (controllers[i] == localController) {
                    localControllerMaskBit = i + 1;
                    break;
                }
            }
            if (localControllerMaskBit < 0) {
                for (int i = resolvedPlayerSlotLimit; i < 64; ++i) {
                    if (controllers[i] == localController) {
                        localControllerMaskBit = i + 1;
                        break;
                    }
                }
            }
        }
        localMaskResolved =
            (localMaskBit >= 0) ||
            (localMaskSlotBit >= 0) ||
            (localHandleSlotBit >= 0) ||
            (localControllerMaskBit >= 0);

        for (int i = 0; i < resolvedPlayerSlotLimit; ++i) {
            if (!pawns[i])
                continue;
            playerResolvedSlots[playerResolvedSlotCount++] = i;
        }
