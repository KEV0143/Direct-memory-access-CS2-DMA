    if (activeWeaponLaneDue) {
        auto resolveActiveWeaponEntity = [&](int idx) -> uintptr_t {
            if (idx < 0 || idx >= 64)
                return 0;
            return activeWeapons[idx];
        };
        const bool weaponServicesRefreshDue =
            s_lastWeaponServicesRefreshUs == 0 ||
            (inventoryNowUs - s_lastWeaponServicesRefreshUs) >= esp::intervals::kInventoryWeaponServicesRefreshUs;
        
        
        
        
        
        bool queuedActiveReads = false;
        bool activeMetaRefreshSlots[64] = {};
        uint16_t activeMetaWeaponIds[64] = {};
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
            if (!s_cachedWeaponServicesResolved[i])
                continue;
            mem.AddScatterReadRequest(handle,
                s_cachedWeaponServicesResolved[i] + ofs.CPlayer_WeaponServices_m_hActiveWeapon,
                &activeWeaponHandles[i], sizeof(uint32_t));
            queuedActiveReads = true;
        }
        
        for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
            const int i = inventoryPlayerSlots[inventorySlotIdx];
            const uintptr_t cachedEntity = s_cachedActiveWeaponsResolved[i];
            if (!cachedEntity)
                continue;
            const bool cachedWeaponIdMissing =
                s_cachedWeaponIdsResolved[i] == 0 ||
                s_cachedWeaponIdsResolved[i] >= 20000u;
            if (cachedWeaponIdMissing &&
                ofs.C_EconEntity_m_AttributeManager > 0 &&
                ofs.C_AttributeContainer_m_Item > 0 &&
                ofs.C_EconItemView_m_iItemDefinitionIndex > 0) {
                const uintptr_t itemDefAddr =
                    cachedEntity +
                    ofs.C_EconEntity_m_AttributeManager +
                    ofs.C_AttributeContainer_m_Item +
                    ofs.C_EconItemView_m_iItemDefinitionIndex;
                mem.AddScatterReadRequest(handle, itemDefAddr, &activeMetaWeaponIds[i], sizeof(uint16_t));
                activeMetaRefreshSlots[i] = true;
            }
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
                memcpy(weaponIds, s_cachedWeaponIdsResolved, sizeof(weaponIds));
                memcpy(ammoClips, s_cachedAmmoClipsResolved, sizeof(ammoClips));
                logUpdateDataIssue("scatter_12_active", "active_weapon_merged_failed_using_cached");
            } else {
                if (!weaponServicesRefreshDue)
                    memcpy(weaponServices, s_cachedWeaponServicesResolved, sizeof(weaponServices));
                memcpy(activeWeaponEntries, s_cachedActiveWeaponEntries, sizeof(activeWeaponEntries));
                memcpy(activeWeapons, s_cachedActiveWeaponsResolved, sizeof(activeWeapons));
                memcpy(weaponIds, s_cachedWeaponIdsResolved, sizeof(weaponIds));
                for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                    const int i = inventoryPlayerSlots[inventorySlotIdx];
                    if (activeMetaRefreshSlots[i] &&
                        activeMetaWeaponIds[i] > 0 &&
                        activeMetaWeaponIds[i] < 20000u) {
                        weaponIds[i] = activeMetaWeaponIds[i];
                    }
                }
                for (uintptr_t& weaponService : weaponServices) {
                    if (!isLikelyGamePointer(weaponService))
                        weaponService = 0;
                }
                
                bool chainDirty[64] = {};
                bool chainChanged = false;
                for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                    const int i = inventoryPlayerSlots[inventorySlotIdx];
                    if ((weaponServicesRefreshDue && weaponServices[i] != s_cachedWeaponServicesResolved[i]) ||
                        activeWeaponHandles[i] != s_cachedActiveWeaponHandles[i] ||
                        (!activeWeaponHandles[i] && s_cachedActiveWeaponHandles[i] != 0)) {
                        chainDirty[i] = true;
                        chainChanged = true;
                    }
                }
                if (chainChanged) {
                    for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                        const int i = inventoryPlayerSlots[inventorySlotIdx];
                        if (chainDirty[i])
                            activeWeaponHandles[i] = 0;
                    }
                    bool queuedHandleRefresh = false;
                    for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                        const int i = inventoryPlayerSlots[inventorySlotIdx];
                        if (!chainDirty[i] || !weaponServices[i])
                            continue;
                        mem.AddScatterReadRequest(handle,
                            weaponServices[i] + ofs.CPlayer_WeaponServices_m_hActiveWeapon,
                            &activeWeaponHandles[i], sizeof(uint32_t));
                        queuedHandleRefresh = true;
                    }
                    if (queuedHandleRefresh && !mem.ExecuteReadScatter(handle)) {
                        weaponHandleChainFailures = std::max(weaponHandleChainFailures, 1);
                        for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                            const int i = inventoryPlayerSlots[inventorySlotIdx];
                            if (chainDirty[i]) {
                                activeWeaponHandles[i] = s_cachedActiveWeaponHandles[i];
                                activeWeaponEntries[i] = s_cachedActiveWeaponEntries[i];
                                activeWeapons[i] = s_cachedActiveWeaponsResolved[i];
                                weaponIds[i] = s_cachedWeaponIdsResolved[i];
                                ammoClips[i] = s_cachedAmmoClipsResolved[i];
                            }
                        }
                    } else {
                        bool handlesDirty[64] = {};
                        bool entriesDirty[64] = {};
                        bool entitiesDirty[64] = {};
                        bool handlesChanged = false;
                        bool entriesChanged = false;
                        bool entitiesChanged = false;

                        for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                            const int i = inventoryPlayerSlots[inventorySlotIdx];
                            if (!chainDirty[i])
                                continue;
                            if (activeWeaponHandles[i] != s_cachedActiveWeaponHandles[i]) {
                                handlesDirty[i] = true;
                                handlesChanged = true;
                            }
                        }
                        for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                            const int i = inventoryPlayerSlots[inventorySlotIdx];
                            if (activeWeaponEntries[i] && !isLikelyGamePointer(activeWeaponEntries[i]))
                                activeWeaponEntries[i] = 0;
                            if (chainDirty[i] && activeWeaponEntries[i] != s_cachedActiveWeaponEntries[i]) {
                                entriesDirty[i] = true;
                                entriesChanged = true;
                            }
                        }
                        for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                            const int i = inventoryPlayerSlots[inventorySlotIdx];
                            if (activeWeapons[i] && !isLikelyGamePointer(activeWeapons[i]))
                                activeWeapons[i] = 0;
                            if (chainDirty[i] && (entriesDirty[i] || activeWeapons[i] != s_cachedActiveWeaponsResolved[i])) {
                                entitiesDirty[i] = true;
                                entitiesChanged = true;
                            }
                        }

                        if (handlesChanged) {
                            for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                                const int i = inventoryPlayerSlots[inventorySlotIdx];
                                if (handlesDirty[i])
                                    activeWeaponEntries[i] = 0;
                            }
                            bool queuedEntryRefresh = false;
                            for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                                const int i = inventoryPlayerSlots[inventorySlotIdx];
                                if (!handlesDirty[i])
                                    continue;
                                const uint32_t weaponHandle = activeWeaponHandles[i];
                                if (!isValidEntityHandle(weaponHandle))
                                    continue;
                                const uint32_t block = (weaponHandle & kEntityHandleMask) >> 9;
                                mem.AddScatterReadRequest(handle,
                                    entityList + 0x10 + 8 * block,
                                    &activeWeaponEntries[i], sizeof(uintptr_t));
                                queuedEntryRefresh = true;
                            }
                            if (queuedEntryRefresh && !mem.ExecuteReadScatter(handle)) {
                                weaponEntryFailures = 1;
                                for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                                    const int i = inventoryPlayerSlots[inventorySlotIdx];
                                    if (handlesDirty[i])
                                        activeWeaponEntries[i] = s_cachedActiveWeaponEntries[i];
                                }
                            }
                            for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                                const int i = inventoryPlayerSlots[inventorySlotIdx];
                                if (handlesDirty[i]) {
                                    entriesDirty[i] = true;
                                    entitiesDirty[i] = true;
                                    entriesChanged = true;
                                    entitiesChanged = true;
                                }
                            }
                        }

                        if (entriesChanged) {
                            for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                                const int i = inventoryPlayerSlots[inventorySlotIdx];
                                if (entriesDirty[i])
                                    activeWeapons[i] = 0;
                            }
                            bool queuedEntityRefresh = false;
                            for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                                const int i = inventoryPlayerSlots[inventorySlotIdx];
                                if (!entriesDirty[i])
                                    continue;
                                const uint32_t weaponHandle = activeWeaponHandles[i];
                                if (!isValidEntityHandle(weaponHandle) || !activeWeaponEntries[i])
                                    continue;
                                const uint32_t slot = weaponHandle & kEntitySlotMask;
                                mem.AddScatterReadRequest(handle,
                                    activeWeaponEntries[i] + kEntitySlotSize * slot,
                                    &activeWeapons[i], sizeof(uintptr_t));
                                queuedEntityRefresh = true;
                            }
                            if (queuedEntityRefresh && !mem.ExecuteReadScatter(handle)) {
                                weaponEntityFailures = 1;
                                for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                                    const int i = inventoryPlayerSlots[inventorySlotIdx];
                                    if (entriesDirty[i])
                                        activeWeapons[i] = s_cachedActiveWeaponsResolved[i];
                                }
                            }
                            for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                                const int i = inventoryPlayerSlots[inventorySlotIdx];
                                if (activeWeapons[i] && !isLikelyGamePointer(activeWeapons[i])) {
                                    activeWeapons[i] = 0;
                                    activeWeaponEntries[i] = 0;
                                }
                                if (entriesDirty[i]) {
                                    entitiesDirty[i] = true;
                                    entitiesChanged = true;
                                }
                            }
                        }

                        if (entitiesChanged) {
                            for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                                const int i = inventoryPlayerSlots[inventorySlotIdx];
                                if (entitiesDirty[i]) {
                                    weaponIds[i] = 0;
                                    ammoClips[i] = 0;
                                }
                            }
                            bool queuedMetaRefresh = false;
                            for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                                const int i = inventoryPlayerSlots[inventorySlotIdx];
                                if (!entitiesDirty[i])
                                    continue;
                                const uintptr_t weaponEntity = resolveActiveWeaponEntity(i);
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
                                for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                                    const int i = inventoryPlayerSlots[inventorySlotIdx];
                                    if (entitiesDirty[i]) {
                                        weaponIds[i] = s_cachedWeaponIdsResolved[i];
                                        ammoClips[i] = s_cachedAmmoClipsResolved[i];
                                    }
                                }
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
