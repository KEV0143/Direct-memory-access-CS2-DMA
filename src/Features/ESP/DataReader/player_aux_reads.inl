    static uint64_t s_lastPlayerIdentityAuxUs = 0;
    static uint64_t s_lastPlayerMoneyAuxUs = 0;
    static uint64_t s_lastPlayerStatusAuxUs = 0;
    static uint64_t s_lastPlayerDefuserAuxUs = 0;
    static uint64_t s_lastPlayerEyeAuxUs = 0;
    static uint64_t s_lastPlayerVisibilityAuxUs = 0;
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
        }
    }

    const uint64_t playerAuxNowUs = TickNowUs();
    const bool wantsPlayerIdentityAux =
        webRadarDemandActive ||
        g::espName;
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

    _playerAuxActiveTick =
        playerIdentityAuxDue ||
        playerMoneyAuxDue ||
        playerStatusAuxDue ||
        playerDefuserAuxDue ||
        playerEyeAuxDue ||
        playerVisibilityAuxDue;

    auto markDynamicPawnsCached = [&]() {
        memcpy(s_cachedDynamicPawns, pawns, sizeof(s_cachedDynamicPawns));
    };

    
    
    
    
    
    
    if (_playerAuxActiveTick) {
        bool queuedPrimary = false;
        bool moneyServiceRefreshMask[64] = {};
        bool itemServiceRefreshMask[64] = {};

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
                memcpy(scopedFlags, s_cachedScopedFlags, sizeof(scopedFlags));
                memcpy(defusingFlags, s_cachedDefusingFlags, sizeof(defusingFlags));
                memcpy(flashDurations, s_cachedFlashDurations, sizeof(flashDurations));
            }
            if (playerDefuserAuxDue) {
                memcpy(itemServices, s_cachedItemServices, sizeof(itemServices));
                memcpy(hasDefuserFlags, s_cachedHasDefuserFlags, sizeof(hasDefuserFlags));
            }
            if (playerEyeAuxDue) {
                memcpy(eyeAnglesPerPlayer, s_cachedEyeAnglesPerPlayer, sizeof(eyeAnglesPerPlayer));
            }
            if (playerVisibilityAuxDue) {
                memcpy(spottedFlags, s_cachedSpottedFlags, sizeof(spottedFlags));
                memcpy(spottedMasks, s_cachedSpottedMasks, sizeof(spottedMasks));
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

            
            if (queuedChainedRefresh) {
                if (!mem.ExecuteReadScatter(handle)) {
                    
                    for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
                        const int i = playerResolvedSlots[resolvedIdx];
                        if (moneyServiceRefreshMask[i] && moneyServices[i])
                            moneys[i] = s_cachedPlayerMoneys[i];
                        if (itemServiceRefreshMask[i] && itemServices[i])
                            hasDefuserFlags[i] = s_cachedHasDefuserFlags[i];
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
            
            if (playerStatusAuxDue || playerDefuserAuxDue || playerEyeAuxDue || playerVisibilityAuxDue)
                markDynamicPawnsCached();
        }

        
        if (playerIdentityAuxDue)   s_lastPlayerIdentityAuxUs   = playerAuxNowUs;
        if (playerMoneyAuxDue)      s_lastPlayerMoneyAuxUs      = playerAuxNowUs;
        if (playerStatusAuxDue)     s_lastPlayerStatusAuxUs     = playerAuxNowUs;
        if (playerDefuserAuxDue)    s_lastPlayerDefuserAuxUs    = playerAuxNowUs;
        if (playerEyeAuxDue)        s_lastPlayerEyeAuxUs        = playerAuxNowUs;
        if (playerVisibilityAuxDue) s_lastPlayerVisibilityAuxUs = playerAuxNowUs;
    }
