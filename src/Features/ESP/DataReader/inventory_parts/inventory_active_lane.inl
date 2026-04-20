    if (activeWeaponLaneDue) {
        const bool weaponServicesRefreshDue =
            s_lastWeaponServicesRefreshUs == 0 ||
            (inventoryNowUs - s_lastWeaponServicesRefreshUs) >= esp::intervals::kInventoryWeaponServicesRefreshUs;
        
        
        
        
        
        bool queuedActiveReads = false;
        if (weaponServicesRefreshDue) {
            for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                const int i = inventoryPlayerSlots[inventorySlotIdx];
                if (!pawns[i])
                    continue;
                mem.AddScatterReadRequest(handle,
                    pawns[i] + ofs.C_BasePlayerPawn_m_pWeaponServices,
                    &weaponServices[i], sizeof(uintptr_t));
                queuedActiveReads = true;
            }
        }
        for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
            const int i = inventoryPlayerSlots[inventorySlotIdx];
            const bool needsClippingFallback =
                pawns[i] &&
                (weaponServicesRefreshDue || !s_cachedActiveWeaponsResolved[i] || !s_cachedClippingWeaponsResolved[i]);
            if (!needsClippingFallback)
                continue;
            mem.AddScatterReadRequest(handle,
                pawns[i] + ofs.C_CSPlayerPawn_m_pClippingWeapon,
                &clippingWeapons[i], sizeof(uintptr_t));
            queuedActiveReads = true;
        }
        for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
            const int i = inventoryPlayerSlots[inventorySlotIdx];
            if (!s_cachedWeaponServicesResolved[i])
                continue;
            mem.AddScatterReadRequest(handle,
                s_cachedWeaponServicesResolved[i] + ofs.CPlayer_WeaponServices_m_hActiveWeapon,
                &activeWeaponHandles[i], sizeof(uint32_t));
            queuedActiveReads = true;
        }
        
        for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
            const int i = inventoryPlayerSlots[inventorySlotIdx];
            const uintptr_t cachedEntity = s_cachedActiveWeaponsResolved[i] ? s_cachedActiveWeaponsResolved[i] : clippingWeapons[i];
            if (!cachedEntity)
                continue;
            if (ofs.C_BasePlayerWeapon_m_iClip1 > 0)
                mem.AddScatterReadRequest(handle, cachedEntity + ofs.C_BasePlayerWeapon_m_iClip1, &ammoClips[i], sizeof(int));
            queuedActiveReads = true;
        }

        if (queuedActiveReads) {
            if (!mem.ExecuteReadScatter(handle)) {
                weaponHandleChainFailures = 1;
                memcpy(weaponServices, s_cachedWeaponServicesResolved, sizeof(weaponServices));
                memcpy(activeWeaponHandles, s_cachedActiveWeaponHandles, sizeof(activeWeaponHandles));
                memcpy(activeWeaponEntries, s_cachedActiveWeaponEntries, sizeof(activeWeaponEntries));
                memcpy(activeWeapons, s_cachedActiveWeaponsResolved, sizeof(activeWeapons));
                memcpy(clippingWeapons, s_cachedClippingWeaponsResolved, sizeof(clippingWeapons));
                memcpy(weaponIds, s_cachedWeaponIdsResolved, sizeof(weaponIds));
                memcpy(ammoClips, s_cachedAmmoClipsResolved, sizeof(ammoClips));
                logUpdateDataIssue("scatter_12_active", "active_weapon_merged_failed_using_cached");
            } else {
                if (!weaponServicesRefreshDue)
                    memcpy(weaponServices, s_cachedWeaponServicesResolved, sizeof(weaponServices));
                memcpy(activeWeaponEntries, s_cachedActiveWeaponEntries, sizeof(activeWeaponEntries));
                memcpy(activeWeapons, s_cachedActiveWeaponsResolved, sizeof(activeWeapons));
                for (uintptr_t& clippingWeapon : clippingWeapons) {
                    if (!isLikelyGamePointer(clippingWeapon))
                        clippingWeapon = 0;
                }
                memcpy(weaponIds, s_cachedWeaponIdsResolved, sizeof(weaponIds));
                for (uintptr_t& weaponService : weaponServices) {
                    if (!isLikelyGamePointer(weaponService))
                        weaponService = 0;
                }
                
                bool chainChanged = false;
                for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                    const int i = inventoryPlayerSlots[inventorySlotIdx];
                    if ((weaponServicesRefreshDue && weaponServices[i] != s_cachedWeaponServicesResolved[i]) ||
                        activeWeaponHandles[i] != s_cachedActiveWeaponHandles[i] ||
                        (!activeWeaponHandles[i] && s_cachedActiveWeaponHandles[i] != 0)) {
                        chainChanged = true;
                        break;
                    }
                }
                if (chainChanged) {
                    
                    
                    
                    
                    
                    memset(activeWeaponHandles, 0, sizeof(activeWeaponHandles));
                    memset(activeWeaponEntries, 0, sizeof(activeWeaponEntries));
                    
                    for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                        const int i = inventoryPlayerSlots[inventorySlotIdx];
                        if (!weaponServices[i])
                            continue;
                        mem.AddScatterReadRequest(handle,
                            weaponServices[i] + ofs.CPlayer_WeaponServices_m_hActiveWeapon,
                            &activeWeaponHandles[i], sizeof(uint32_t));
                    }
                    
                    for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                        const int i = inventoryPlayerSlots[inventorySlotIdx];
                        const uint32_t cachedHandle = s_cachedActiveWeaponHandles[i];
                        if (!cachedHandle || cachedHandle == 0xFFFFFFFFu)
                            continue;
                        const uint32_t block = (cachedHandle & kEntityHandleMask) >> 9;
                        mem.AddScatterReadRequest(handle,
                            entityList + 0x10 + 8 * block,
                            &activeWeaponEntries[i], sizeof(uintptr_t));
                    }
                    
                    memset(activeWeapons, 0, sizeof(activeWeapons));
                    for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                        const int i = inventoryPlayerSlots[inventorySlotIdx];
                        const uint32_t cachedHandle = s_cachedActiveWeaponHandles[i];
                        if (!cachedHandle || cachedHandle == 0xFFFFFFFFu || !s_cachedActiveWeaponEntries[i])
                            continue;
                        const uint32_t slot = cachedHandle & kEntitySlotMask;
                        mem.AddScatterReadRequest(handle,
                            s_cachedActiveWeaponEntries[i] + kEntitySlotSize * slot,
                            &activeWeapons[i], sizeof(uintptr_t));
                    }
                    
                    memset(weaponIds, 0, sizeof(weaponIds));
                    memset(ammoClips, 0, sizeof(ammoClips));
                    for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                        const int i = inventoryPlayerSlots[inventorySlotIdx];
                        const uintptr_t cachedWeapon = s_cachedActiveWeaponsResolved[i] ? s_cachedActiveWeaponsResolved[i] : clippingWeapons[i];
                        if (!cachedWeapon)
                            continue;
                        const uintptr_t itemDefAddr =
                            cachedWeapon +
                            ofs.C_EconEntity_m_AttributeManager +
                            ofs.C_AttributeContainer_m_Item +
                            ofs.C_EconItemView_m_iItemDefinitionIndex;
                        mem.AddScatterReadRequest(handle, itemDefAddr, &weaponIds[i], sizeof(uint16_t));
                        if (ofs.C_BasePlayerWeapon_m_iClip1 > 0)
                            mem.AddScatterReadRequest(handle, cachedWeapon + ofs.C_BasePlayerWeapon_m_iClip1, &ammoClips[i], sizeof(int));
                    }
                    if (!mem.ExecuteReadScatter(handle)) {
                        weaponHandleChainFailures = std::max(weaponHandleChainFailures, 1);
                        memcpy(activeWeaponHandles, s_cachedActiveWeaponHandles, sizeof(activeWeaponHandles));
                        memcpy(activeWeaponEntries, s_cachedActiveWeaponEntries, sizeof(activeWeaponEntries));
                        memcpy(activeWeapons, s_cachedActiveWeaponsResolved, sizeof(activeWeapons));
                        memcpy(weaponIds, s_cachedWeaponIdsResolved, sizeof(weaponIds));
                        memcpy(ammoClips, s_cachedAmmoClipsResolved, sizeof(ammoClips));
                    } else {
                        
                        bool handlesChanged = false;
                        for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                            const int i = inventoryPlayerSlots[inventorySlotIdx];
                            if (activeWeaponHandles[i] != s_cachedActiveWeaponHandles[i]) {
                                handlesChanged = true;
                                break;
                            }
                        }
                        bool entriesChanged = false;
                        for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                            const int i = inventoryPlayerSlots[inventorySlotIdx];
                            if (activeWeaponEntries[i] && !isLikelyGamePointer(activeWeaponEntries[i]))
                                activeWeaponEntries[i] = 0;
                            if (activeWeaponEntries[i] != s_cachedActiveWeaponEntries[i])
                                entriesChanged = true;
                        }
                        bool entitiesChanged = entriesChanged;
                        if (!entitiesChanged) {
                            for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                                const int i = inventoryPlayerSlots[inventorySlotIdx];
                                if (activeWeapons[i] && !isLikelyGamePointer(activeWeapons[i]))
                                    activeWeapons[i] = 0;
                                if (activeWeapons[i] != s_cachedActiveWeaponsResolved[i]) {
                                    entitiesChanged = true;
                                    break;
                                }
                            }
                        }

                        
                        if (handlesChanged) {
                            memset(activeWeaponEntries, 0, sizeof(activeWeaponEntries));
                            bool queuedEntryRefresh = false;
                            for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                                const int i = inventoryPlayerSlots[inventorySlotIdx];
                                const uint32_t weaponHandle = activeWeaponHandles[i];
                                if (!weaponHandle || weaponHandle == 0xFFFFFFFFu)
                                    continue;
                                const uint32_t block = (weaponHandle & kEntityHandleMask) >> 9;
                                mem.AddScatterReadRequest(handle,
                                    entityList + 0x10 + 8 * block,
                                    &activeWeaponEntries[i], sizeof(uintptr_t));
                                queuedEntryRefresh = true;
                            }
                            if (queuedEntryRefresh && !mem.ExecuteReadScatter(handle)) {
                                weaponEntryFailures = 1;
                                memcpy(activeWeaponEntries, s_cachedActiveWeaponEntries, sizeof(activeWeaponEntries));
                            }
                            entriesChanged = true;
                            entitiesChanged = true;
                        }

                        
                        if (entriesChanged) {
                            memset(activeWeapons, 0, sizeof(activeWeapons));
                            bool queuedEntityRefresh = false;
                            for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                                const int i = inventoryPlayerSlots[inventorySlotIdx];
                                const uint32_t weaponHandle = activeWeaponHandles[i];
                                if (!weaponHandle || weaponHandle == 0xFFFFFFFFu || !activeWeaponEntries[i])
                                    continue;
                                const uint32_t slot = weaponHandle & kEntitySlotMask;
                                mem.AddScatterReadRequest(handle,
                                    activeWeaponEntries[i] + kEntitySlotSize * slot,
                                    &activeWeapons[i], sizeof(uintptr_t));
                                queuedEntityRefresh = true;
                            }
                            if (queuedEntityRefresh && !mem.ExecuteReadScatter(handle)) {
                                weaponEntityFailures = 1;
                                memcpy(activeWeapons, s_cachedActiveWeaponsResolved, sizeof(activeWeapons));
                            }
                            entitiesChanged = true;
                        }

                        
                        if (entitiesChanged) {
                            memset(weaponIds, 0, sizeof(weaponIds));
                            memset(ammoClips, 0, sizeof(ammoClips));
                            bool queuedMetaRefresh = false;
                            for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                                const int i = inventoryPlayerSlots[inventorySlotIdx];
                                const uintptr_t weaponEntity = activeWeapons[i] ? activeWeapons[i] : clippingWeapons[i];
                                if (!weaponEntity)
                                    continue;
                                const uintptr_t itemDefAddr =
                                    weaponEntity +
                                    ofs.C_EconEntity_m_AttributeManager +
                                    ofs.C_AttributeContainer_m_Item +
                                    ofs.C_EconItemView_m_iItemDefinitionIndex;
                                mem.AddScatterReadRequest(handle, itemDefAddr, &weaponIds[i], sizeof(uint16_t));
                                queuedMetaRefresh = true;
                                if (ofs.C_BasePlayerWeapon_m_iClip1 > 0)
                                    mem.AddScatterReadRequest(handle, weaponEntity + ofs.C_BasePlayerWeapon_m_iClip1, &ammoClips[i], sizeof(int));
                            }
                            if (queuedMetaRefresh && !mem.ExecuteReadScatter(handle)) {
                                weaponMetaFailures = 1;
                                memcpy(weaponIds, s_cachedWeaponIdsResolved, sizeof(weaponIds));
                                memcpy(ammoClips, s_cachedAmmoClipsResolved, sizeof(ammoClips));
                            }
                        }
                    }
                }

                for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx)
                    storeInventoryActiveSlotToCache(inventoryPlayerSlots[inventorySlotIdx]);
                if (weaponServicesRefreshDue)
                    s_lastWeaponServicesRefreshUs = inventoryNowUs;
            }
        }
        s_lastActiveWeaponLaneUs = inventoryNowUs;
    }
