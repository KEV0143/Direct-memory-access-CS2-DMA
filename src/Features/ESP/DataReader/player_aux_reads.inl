    static uint64_t s_lastPlayerIdentityAuxUs = 0;
    static uint64_t s_lastPlayerMoneyAuxUs = 0;
    static uint64_t s_lastPlayerStatusAuxUs = 0;
    static uint64_t s_lastPlayerDefuserAuxUs = 0;
    static uint64_t s_lastPlayerEyeAuxUs = 0;
    static uint64_t s_lastPlayerVisibilityAuxUs = 0;
    static uint64_t s_lastPlayerSpectatorAuxUs = 0;
    static uint64_t s_playerAuxTickResetSerial = 0;
    {
        const uint64_t auxResetSerial = s_sceneResetSerial.load(std::memory_order_relaxed);
        if (s_playerAuxTickResetSerial != auxResetSerial) {
            s_playerAuxTickResetSerial = auxResetSerial;
            s_lastPlayerIdentityAuxUs = 0;
            s_lastPlayerMoneyAuxUs = 0;
            s_lastPlayerStatusAuxUs = 0;
            s_lastPlayerDefuserAuxUs = 0;
            s_lastPlayerEyeAuxUs = 0;
            s_lastPlayerVisibilityAuxUs = 0;
            s_lastPlayerSpectatorAuxUs = 0;
        }
    }

    static uintptr_t s_cachedObserverPawns[64] = {};
    static uintptr_t s_cachedObserverServices[64] = {};
    static uint32_t s_cachedObserverTargets[64] = {};
    static uint8_t s_cachedObserverModes[64] = {};
    static uint64_t s_playerObserverCacheResetSerial = 0;
    {
        const uint64_t observerResetSerial = s_sceneResetSerial.load(std::memory_order_relaxed);
        if (s_playerObserverCacheResetSerial != observerResetSerial) {
            s_playerObserverCacheResetSerial = observerResetSerial;
            memset(s_cachedObserverPawns, 0, sizeof(s_cachedObserverPawns));
            memset(s_cachedObserverServices, 0, sizeof(s_cachedObserverServices));
            memset(s_cachedObserverTargets, 0, sizeof(s_cachedObserverTargets));
            memset(s_cachedObserverModes, 0, sizeof(s_cachedObserverModes));
        }
    }

    uintptr_t observerServices[64] = {};
    uint32_t observerTargets[64] = {};
    uint8_t observerModes[64] = {};
    memcpy(observerServices, s_cachedObserverServices, sizeof(observerServices));
    memcpy(observerTargets, s_cachedObserverTargets, sizeof(observerTargets));
    memcpy(observerModes, s_cachedObserverModes, sizeof(observerModes));

    const uint64_t playerAuxNowUs = TickNowUs();
    const bool wantsPlayerIdentityAux =
        webRadarDemandActive ||
        g::espName ||
        g::radarSpectatorList;
    const bool wantsPlayerMoneyAux =
        webRadarDemandActive ||
        (g::espFlags && g::espFlagMoney);
    const bool wantsPlayerStatusAux =
        webRadarDemandActive ||
        (g::espFlags && (g::espFlagScoped || g::espFlagDefusing || g::espFlagBlind));
    const bool wantsPlayerDefuserAux =
        webRadarDemandActive ||
        (g::espFlags && g::espFlagKit);
    const bool wantsPlayerEyeAux =
        webRadarDemandActive ||
        (g::radarEnabled && g::radarShowAngles);
    const bool wantsPlayerVisibilityAux =
        g::espVisibilityColoring;
    const bool hasObserverOffsets =
        ofs.C_BasePlayerPawn_m_pObserverServices > 0 &&
        ofs.CPlayer_ObserverServices_m_iObserverMode > 0 &&
        ofs.CPlayer_ObserverServices_m_hObserverTarget > 0;
    const bool wantsPlayerSpectatorAux =
        g::radarSpectatorList &&
        hasObserverOffsets;

    const bool playerIdentityAuxDue =
        wantsPlayerIdentityAux &&
        (s_lastPlayerIdentityAuxUs == 0 ||
         (playerAuxNowUs - s_lastPlayerIdentityAuxUs) >= esp::intervals::kPlayerIdentityAuxUs);
    const bool playerMoneyAuxDue =
        wantsPlayerMoneyAux &&
        (s_lastPlayerMoneyAuxUs == 0 ||
         (playerAuxNowUs - s_lastPlayerMoneyAuxUs) >= esp::intervals::kPlayerMoneyAuxUs);
    const bool playerStatusAuxDue =
        wantsPlayerStatusAux &&
        (s_lastPlayerStatusAuxUs == 0 ||
         (playerAuxNowUs - s_lastPlayerStatusAuxUs) >= esp::intervals::kPlayerStatusAuxUs);
    const bool playerDefuserAuxDue =
        wantsPlayerDefuserAux &&
        (s_lastPlayerDefuserAuxUs == 0 ||
         (playerAuxNowUs - s_lastPlayerDefuserAuxUs) >= esp::intervals::kPlayerDefuserAuxUs);
    const bool playerEyeAuxDue =
        wantsPlayerEyeAux &&
        (s_lastPlayerEyeAuxUs == 0 ||
         (playerAuxNowUs - s_lastPlayerEyeAuxUs) >= esp::intervals::kPlayerEyeAuxUs);
    const bool playerVisibilityAuxDue =
        wantsPlayerVisibilityAux &&
        hasSpottedStateOffsets &&
        (s_lastPlayerVisibilityAuxUs == 0 ||
         (playerAuxNowUs - s_lastPlayerVisibilityAuxUs) >= esp::intervals::kPlayerVisibilityAuxUs);
    const bool playerSpectatorAuxDue =
        wantsPlayerSpectatorAux &&
        (s_lastPlayerSpectatorAuxUs == 0 ||
         (playerAuxNowUs - s_lastPlayerSpectatorAuxUs) >= esp::intervals::kPlayerSpectatorAuxUs);

    _playerAuxActiveTick =
        playerIdentityAuxDue ||
        playerMoneyAuxDue ||
        playerStatusAuxDue ||
        playerDefuserAuxDue ||
        playerEyeAuxDue ||
        playerVisibilityAuxDue ||
        playerSpectatorAuxDue;

    auto markDynamicPawnsCached = [&]() {
        memcpy(s_cachedDynamicPawns, pawns, sizeof(s_cachedDynamicPawns));
    };

    
    
    
    
    
    
    if (_playerAuxActiveTick) {
        bool queuedPrimary = false;
        bool moneyServiceRefreshMask[64] = {};
        bool itemServiceRefreshMask[64] = {};
        bool observerServiceRefreshMask[64] = {};

        for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
            const int i = playerResolvedSlots[resolvedIdx];

            
            if (playerIdentityAuxDue && controllers[i]) {
                if (ofs.CBasePlayerController_m_iszPlayerName > 0) {
                    mem.AddScatterReadRequest(
                        handle,
                        controllers[i] + ofs.CBasePlayerController_m_iszPlayerName,
                        &names[i],
                        sizeof(names[i]));
                    queuedPrimary = true;
                }
                if (webRadarDemandActive && ofs.CBasePlayerController_m_iPing > 0) {
                    mem.AddScatterReadRequest(
                        handle,
                        controllers[i] + ofs.CBasePlayerController_m_iPing,
                        &pings[i],
                        sizeof(uint32_t));
                    queuedPrimary = true;
                }
            }

            
            if (playerMoneyAuxDue && controllers[i]) {
                if (controllers[i] != s_cachedMoneyControllers[i] || !s_cachedMoneyServices[i]) {
                    if (ofs.CCSPlayerController_m_pInGameMoneyServices > 0) {
                        mem.AddScatterReadRequest(
                            handle,
                            controllers[i] + ofs.CCSPlayerController_m_pInGameMoneyServices,
                            &moneyServices[i],
                            sizeof(uintptr_t));
                        moneyServiceRefreshMask[i] = true;
                        queuedPrimary = true;
                    }
                } else if (ofs.CCSPlayerController_InGameMoneyServices_m_iAccount > 0) {
                    mem.AddScatterReadRequest(
                        handle,
                        s_cachedMoneyServices[i] + ofs.CCSPlayerController_InGameMoneyServices_m_iAccount,
                        &moneys[i],
                        sizeof(int));
                    queuedPrimary = true;
                }
            }

            
            if (playerStatusAuxDue && pawns[i]) {
                if (ofs.C_CSPlayerPawn_m_bIsScoped > 0) {
                    mem.AddScatterReadRequest(
                        handle,
                        pawns[i] + ofs.C_CSPlayerPawn_m_bIsScoped,
                        &scopedFlags[i],
                        sizeof(uint8_t));
                    queuedPrimary = true;
                }
                if (ofs.C_CSPlayerPawn_m_bIsDefusing > 0) {
                    mem.AddScatterReadRequest(
                        handle,
                        pawns[i] + ofs.C_CSPlayerPawn_m_bIsDefusing,
                        &defusingFlags[i],
                        sizeof(uint8_t));
                    queuedPrimary = true;
                }
                if (ofs.C_CSPlayerPawnBase_m_flFlashDuration > 0) {
                    mem.AddScatterReadRequest(
                        handle,
                        pawns[i] + ofs.C_CSPlayerPawnBase_m_flFlashDuration,
                        &flashDurations[i],
                        sizeof(float));
                    queuedPrimary = true;
                }
            }

            
            if (playerDefuserAuxDue && pawns[i]) {
                if (pawns[i] != s_cachedDynamicPawns[i] || !s_cachedItemServices[i]) {
                    if (ofs.C_CSPlayerPawnBase_m_pItemServices > 0) {
                        mem.AddScatterReadRequest(
                            handle,
                            pawns[i] + ofs.C_CSPlayerPawnBase_m_pItemServices,
                            &itemServices[i],
                            sizeof(uintptr_t));
                        itemServiceRefreshMask[i] = true;
                        queuedPrimary = true;
                    }
                } else if (ofs.CCSPlayer_ItemServices_m_bHasDefuser > 0) {
                    mem.AddScatterReadRequest(
                        handle,
                        s_cachedItemServices[i] + ofs.CCSPlayer_ItemServices_m_bHasDefuser,
                        &hasDefuserFlags[i],
                        sizeof(uint8_t));
                    queuedPrimary = true;
                }
            }

            
            if (playerEyeAuxDue && pawns[i] && ofs.C_CSPlayerPawn_m_angEyeAngles > 0) {
                mem.AddScatterReadRequest(
                    handle,
                    pawns[i] + ofs.C_CSPlayerPawn_m_angEyeAngles,
                    &eyeAnglesPerPlayer[i],
                    sizeof(Vector3));
                queuedPrimary = true;
            }

            
            if (playerVisibilityAuxDue && pawns[i]) {
                mem.AddScatterReadRequest(
                    handle,
                    pawns[i] + ofs.C_CSPlayerPawn_m_entitySpottedState + ofs.EntitySpottedState_t_m_bSpottedByMask,
                    reinterpret_cast<void*>(&spottedMasks[i]),
                    sizeof(uint32_t) * 2);
                queuedPrimary = true;
                if (spottedFlagOffset >= 0) {
                    mem.AddScatterReadRequest(
                        handle,
                        pawns[i] + ofs.C_CSPlayerPawn_m_entitySpottedState + spottedFlagOffset,
                        &spottedFlags[i],
                        sizeof(uint8_t));
                }
            }

            if (playerSpectatorAuxDue && pawns[i]) {
                const bool observerServiceNeedsRefresh =
                    pawns[i] != s_cachedObserverPawns[i] ||
                    !s_cachedObserverServices[i];
                if (observerServiceNeedsRefresh) {
                    mem.AddScatterReadRequest(
                        handle,
                        pawns[i] + ofs.C_BasePlayerPawn_m_pObserverServices,
                        &observerServices[i],
                        sizeof(uintptr_t));
                    observerServiceRefreshMask[i] = true;
                    queuedPrimary = true;
                } else {
                    mem.AddScatterReadRequest(
                        handle,
                        s_cachedObserverServices[i] + ofs.CPlayer_ObserverServices_m_iObserverMode,
                        &observerModes[i],
                        sizeof(uint8_t));
                    mem.AddScatterReadRequest(
                        handle,
                        s_cachedObserverServices[i] + ofs.CPlayer_ObserverServices_m_hObserverTarget,
                        &observerTargets[i],
                        sizeof(uint32_t));
                    queuedPrimary = true;
                }
            }
        }

        
        bool primaryScatterOk = true;
        if (queuedPrimary)
            primaryScatterOk = mem.ExecuteReadScatter(handle);

        if (!primaryScatterOk) {
            
            if (playerIdentityAuxDue) {
                memcpy(names, s_cachedPlayerNames, sizeof(names));
                memcpy(pings, s_cachedPlayerPings, sizeof(pings));
            }
            if (playerMoneyAuxDue) {
                memcpy(moneyServices, s_cachedMoneyServices, sizeof(moneyServices));
                memcpy(moneys, s_cachedPlayerMoneys, sizeof(moneys));
            }
            if (playerStatusAuxDue) {
                memset(scopedFlags, 0, sizeof(scopedFlags));
                memset(defusingFlags, 0, sizeof(defusingFlags));
                memset(flashDurations, 0, sizeof(flashDurations));
            }
            if (playerDefuserAuxDue) {
                memset(itemServices, 0, sizeof(itemServices));
                memset(hasDefuserFlags, 0, sizeof(hasDefuserFlags));
            }
            if (playerEyeAuxDue) {
                memcpy(eyeAnglesPerPlayer, s_cachedEyeAnglesPerPlayer, sizeof(eyeAnglesPerPlayer));
            }
            if (playerVisibilityAuxDue) {
                memset(spottedFlags, 0, sizeof(spottedFlags));
                memset(spottedMasks, 0, sizeof(spottedMasks));
            }
            if (playerSpectatorAuxDue) {
                memcpy(observerServices, s_cachedObserverServices, sizeof(observerServices));
                memcpy(observerTargets, s_cachedObserverTargets, sizeof(observerTargets));
                memcpy(observerModes, s_cachedObserverModes, sizeof(observerModes));
            }
            logUpdateDataIssue("scatter_aux_primary", "player_aux_primary_scatter_failed");
        } else {
            
            bool queuedChainedRefresh = false;

            if (playerMoneyAuxDue) {
                for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
                    const int i = playerResolvedSlots[resolvedIdx];
                    if (!moneyServiceRefreshMask[i])
                        continue;
                    if (!isLikelyGamePointer(moneyServices[i])) {
                        moneyServices[i] = 0;
                        continue;
                    }
                    if (ofs.CCSPlayerController_InGameMoneyServices_m_iAccount > 0) {
                        mem.AddScatterReadRequest(
                            handle,
                            moneyServices[i] + ofs.CCSPlayerController_InGameMoneyServices_m_iAccount,
                            &moneys[i],
                            sizeof(int));
                        queuedChainedRefresh = true;
                    }
                }
            }

            if (playerDefuserAuxDue) {
                for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
                    const int i = playerResolvedSlots[resolvedIdx];
                    if (!itemServiceRefreshMask[i])
                        continue;
                    if (!isLikelyGamePointer(itemServices[i])) {
                        itemServices[i] = 0;
                        continue;
                    }
                    if (ofs.CCSPlayer_ItemServices_m_bHasDefuser > 0) {
                        mem.AddScatterReadRequest(
                            handle,
                            itemServices[i] + ofs.CCSPlayer_ItemServices_m_bHasDefuser,
                            &hasDefuserFlags[i],
                            sizeof(uint8_t));
                        queuedChainedRefresh = true;
                    }
                }
            }

            if (playerSpectatorAuxDue) {
                for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
                    const int i = playerResolvedSlots[resolvedIdx];
                    if (!observerServiceRefreshMask[i])
                        continue;
                    if (!isLikelyGamePointer(observerServices[i])) {
                        observerServices[i] = 0;
                        observerTargets[i] = 0;
                        observerModes[i] = 0;
                        continue;
                    }
                    mem.AddScatterReadRequest(
                        handle,
                        observerServices[i] + ofs.CPlayer_ObserverServices_m_iObserverMode,
                        &observerModes[i],
                        sizeof(uint8_t));
                    mem.AddScatterReadRequest(
                        handle,
                        observerServices[i] + ofs.CPlayer_ObserverServices_m_hObserverTarget,
                        &observerTargets[i],
                        sizeof(uint32_t));
                    queuedChainedRefresh = true;
                }
            }

            
            if (queuedChainedRefresh) {
                if (!mem.ExecuteReadScatter(handle)) {
                    
                    for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
                        const int i = playerResolvedSlots[resolvedIdx];
                        if (moneyServiceRefreshMask[i] && moneyServices[i])
                            moneys[i] = s_cachedPlayerMoneys[i];
                        if (itemServiceRefreshMask[i] && itemServices[i])
                            hasDefuserFlags[i] = s_cachedHasDefuserFlags[i];
                        if (observerServiceRefreshMask[i] && observerServices[i]) {
                            observerTargets[i] = s_cachedObserverTargets[i];
                            observerModes[i] = s_cachedObserverModes[i];
                        }
                    }
                    logUpdateDataIssue("scatter_aux_chain", "player_aux_chained_refresh_failed");
                }
            }

            
            if (playerIdentityAuxDue) {
                memcpy(s_cachedPlayerNames, names, sizeof(s_cachedPlayerNames));
                memcpy(s_cachedPlayerPings, pings, sizeof(s_cachedPlayerPings));
                memcpy(s_cachedIdentityControllers, controllers, sizeof(s_cachedIdentityControllers));
            }
            if (playerMoneyAuxDue) {
                memcpy(s_cachedMoneyControllers, controllers, sizeof(s_cachedMoneyControllers));
                memcpy(s_cachedMoneyServices, moneyServices, sizeof(s_cachedMoneyServices));
                memcpy(s_cachedPlayerMoneys, moneys, sizeof(s_cachedPlayerMoneys));
            }
            if (playerStatusAuxDue) {
                memcpy(s_cachedScopedFlags, scopedFlags, sizeof(s_cachedScopedFlags));
                memcpy(s_cachedDefusingFlags, defusingFlags, sizeof(s_cachedDefusingFlags));
                memcpy(s_cachedFlashDurations, flashDurations, sizeof(s_cachedFlashDurations));
            }
            if (playerDefuserAuxDue) {
                memcpy(s_cachedItemServices, itemServices, sizeof(s_cachedItemServices));
                memcpy(s_cachedHasDefuserFlags, hasDefuserFlags, sizeof(s_cachedHasDefuserFlags));
            }
            if (playerEyeAuxDue) {
                memcpy(s_cachedEyeAnglesPerPlayer, eyeAnglesPerPlayer, sizeof(s_cachedEyeAnglesPerPlayer));
            }
            if (playerVisibilityAuxDue) {
                memcpy(s_cachedSpottedFlags, spottedFlags, sizeof(s_cachedSpottedFlags));
                memcpy(s_cachedSpottedMasks, spottedMasks, sizeof(s_cachedSpottedMasks));
            }
            if (playerSpectatorAuxDue) {
                memcpy(s_cachedObserverPawns, pawns, sizeof(s_cachedObserverPawns));
                memcpy(s_cachedObserverServices, observerServices, sizeof(s_cachedObserverServices));
                memcpy(s_cachedObserverTargets, observerTargets, sizeof(s_cachedObserverTargets));
                memcpy(s_cachedObserverModes, observerModes, sizeof(s_cachedObserverModes));
            }
            
            if (playerStatusAuxDue || playerDefuserAuxDue || playerEyeAuxDue || playerVisibilityAuxDue)
                markDynamicPawnsCached();
        }

        
        if (playerIdentityAuxDue)   s_lastPlayerIdentityAuxUs   = playerAuxNowUs;
        if (playerMoneyAuxDue)      s_lastPlayerMoneyAuxUs      = playerAuxNowUs;
        if (playerStatusAuxDue)     s_lastPlayerStatusAuxUs     = playerAuxNowUs;
        if (playerDefuserAuxDue)    s_lastPlayerDefuserAuxUs    = playerAuxNowUs;
        if (playerEyeAuxDue)        s_lastPlayerEyeAuxUs        = playerAuxNowUs;
        if (playerVisibilityAuxDue) s_lastPlayerVisibilityAuxUs = playerAuxNowUs;
        if (playerSpectatorAuxDue)  s_lastPlayerSpectatorAuxUs  = playerAuxNowUs;
    }

    if (!wantsPlayerSpectatorAux) {
        if (s_spectatorCount != 0) {
            s_spectatorCount = 0;
            memset(s_spectators, 0, sizeof(s_spectators));
        }
    } else if (playerSpectatorAuxDue) {
        SpectatorEntry resolvedSpectators[64] = {};
        int resolvedSpectatorCount = 0;
        auto resolveEntityFromHandlePreferredStride = [&](uint32_t handleValue, uint32_t preferredStride) -> uintptr_t {
            if (!isValidEntityHandle(handleValue) || !entityList)
                return 0;

            const uint32_t entityIndex = handleValue & kEntityHandleMask;
            const uint32_t block = entityIndex >> 9;
            const uint32_t slot = entityIndex & kEntitySlotMask;
            uintptr_t entry = 0;
            if (!readPointer(entityList + 0x10 + 8ull * static_cast<uintptr_t>(block), &entry))
                return 0;

            auto readEntityAtStride = [&](uint32_t stride) -> uintptr_t {
                if (stride == 0)
                    return 0;
                uintptr_t entity = 0;
                readPointer(entry + static_cast<uintptr_t>(stride) * static_cast<uintptr_t>(slot), &entity);
                return isLikelyGamePointer(entity) ? entity : 0;
            };

            uintptr_t entity = readEntityAtStride(preferredStride);
            if (entity)
                return entity;
            entity = readEntityAtStride(kEntitySlotSize);
            if (entity)
                return entity;
            if (kEntitySlotSizeFallback != kEntitySlotSize)
                return readEntityAtStride(kEntitySlotSizeFallback);
            return 0;
        };

        auto observerTargetIsLocal = [&](uint32_t targetHandle) -> bool {
            if (!isValidEntityHandle(targetHandle))
                return false;
            if (localControllerPawnHandle != 0u &&
                localControllerPawnHandle != 0xFFFFFFFFu &&
                ((targetHandle & kEntityHandleMask) == (localControllerPawnHandle & kEntityHandleMask)))
                return true;

            const uintptr_t targetEntity =
                resolveEntityFromHandlePreferredStride(targetHandle, kEntitySlotSizeFallback);
            if (!targetEntity)
                return false;
            if (targetEntity == localPawn || (s_localPawn != 0 && targetEntity == s_localPawn))
                return true;
            return false;
        };

        auto readControllerObserverState = [&](int i,
                                               uintptr_t& observerPawn,
                                               uint8_t& observerMode,
                                               uint32_t& observerTarget) -> bool {
            observerPawn = 0;
            observerMode = 0;
            observerTarget = 0;
            if (i < 0 || i >= 64 || !controllers[i])
                return false;

            bool pawnIsAlive = false;
            if (ofs.CCSPlayerController_m_bPawnIsAlive > 0 &&
                readValue(controllers[i] + ofs.CCSPlayerController_m_bPawnIsAlive, &pawnIsAlive, sizeof(pawnIsAlive)) &&
                pawnIsAlive) {
                return false;
            }

            uint32_t observerPawnHandles[2] = {};
            int observerPawnHandleCount = 0;
            if (ofs.CCSPlayerController_m_hObserverPawn > 0) {
                readValue(
                    controllers[i] + ofs.CCSPlayerController_m_hObserverPawn,
                    &observerPawnHandles[observerPawnHandleCount],
                    sizeof(uint32_t));
                ++observerPawnHandleCount;
            }
            if (ofs.CBasePlayerController_m_hPawn > 0) {
                readValue(
                    controllers[i] + ofs.CBasePlayerController_m_hPawn,
                    &observerPawnHandles[observerPawnHandleCount],
                    sizeof(uint32_t));
                ++observerPawnHandleCount;
            }

            for (int handleIdx = 0; handleIdx < observerPawnHandleCount; ++handleIdx) {
                const uint32_t pawnHandle = observerPawnHandles[handleIdx];
                if (!isValidEntityHandle(pawnHandle))
                    continue;

                const uintptr_t candidatePawn =
                    resolveEntityFromHandlePreferredStride(pawnHandle, kEntitySlotSizeFallback);
                if (!candidatePawn ||
                    candidatePawn == localPawn ||
                    (s_localPawn != 0 && candidatePawn == s_localPawn)) {
                    continue;
                }

                uintptr_t candidateObserverServices = 0;
                if (!readPointer(
                        candidatePawn + ofs.C_BasePlayerPawn_m_pObserverServices,
                        &candidateObserverServices)) {
                    continue;
                }

                uint8_t candidateMode = 0;
                uint32_t candidateTarget = 0;
                if (!readValue(
                        candidateObserverServices + ofs.CPlayer_ObserverServices_m_iObserverMode,
                        &candidateMode,
                        sizeof(candidateMode)) ||
                    !readValue(
                        candidateObserverServices + ofs.CPlayer_ObserverServices_m_hObserverTarget,
                        &candidateTarget,
                        sizeof(candidateTarget))) {
                    continue;
                }
                if (candidateMode == 0 || !isValidEntityHandle(candidateTarget))
                    continue;

                observerPawn = candidatePawn;
                observerMode = candidateMode;
                observerTarget = candidateTarget;
                return true;
            }

            return false;
        };

        for (int i = 0; i < 64; ++i) {
            if (!controllers[i] && !pawns[i])
                continue;
            const bool isLocalByIndex =
                localControllerMaskBit > 0 &&
                localControllerMaskBit <= 64 &&
                i == (localControllerMaskBit - 1);
            const bool isLocalByPawn =
                (localPawn != 0 && pawns[i] == localPawn) ||
                (s_localPawn != 0 && pawns[i] == s_localPawn);
            const bool isLocalByController =
                localController != 0 && controllers[i] == localController;
            if (isLocalByIndex || isLocalByPawn || isLocalByController)
                continue;

            uintptr_t liveObserverPawn = 0;
            uint8_t liveObserverMode = 0;
            uint32_t liveObserverTarget = 0;
            const bool readLiveObserver =
                readControllerObserverState(i, liveObserverPawn, liveObserverMode, liveObserverTarget);
            if (!readLiveObserver) {
                liveObserverPawn = pawns[i];
                liveObserverMode = observerModes[i];
                liveObserverTarget = observerTargets[i];
            }
            if (liveObserverMode == 0 || !observerTargetIsLocal(liveObserverTarget))
                continue;

            SpectatorEntry& out = resolvedSpectators[resolvedSpectatorCount++];
            out.valid = true;
            out.observerMode = liveObserverMode;
            char liveName[128] = {};
            if ((!names[i][0] && !s_cachedPlayerNames[i][0]) &&
                controllers[i] &&
                ofs.CBasePlayerController_m_iszPlayerName > 0) {
                readValue(
                    controllers[i] + ofs.CBasePlayerController_m_iszPlayerName,
                    liveName,
                    sizeof(liveName) - 1);
                liveName[sizeof(liveName) - 1] = '\0';
            }
            const char* name =
                names[i][0] ? names[i] :
                s_cachedPlayerNames[i][0] ? s_cachedPlayerNames[i] :
                liveName;
            if (name && name[0]) {
                bool duplicate = false;
                for (int existingIdx = 0; existingIdx < resolvedSpectatorCount - 1; ++existingIdx) {
                    if (std::strncmp(resolvedSpectators[existingIdx].name, name, sizeof(out.name)) == 0) {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate) {
                    --resolvedSpectatorCount;
                    continue;
                }
                strncpy_s(out.name, sizeof(out.name), name, _TRUNCATE);
            } else {
                std::snprintf(out.name, sizeof(out.name), "Player %d", i + 1);
            }
            if (resolvedSpectatorCount >= 64)
                break;
        }

        s_spectatorCount = resolvedSpectatorCount;
        memcpy(s_spectators, resolvedSpectators, sizeof(s_spectators));
    }
