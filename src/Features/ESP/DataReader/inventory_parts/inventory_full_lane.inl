    if (fullInventoryLaneDue) {
        bool queuedInventoryHandleReads = false;
        for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
            const int i = inventoryPlayerSlots[inventorySlotIdx];
            if (!weaponServices[i] || ofs.CPlayer_WeaponServices_m_hMyWeapons <= 0)
                continue;
            mem.AddScatterReadRequest(
                handle,
                weaponServices[i] + ofs.CPlayer_WeaponServices_m_hMyWeapons,
                &inventoryWeaponCounts[i],
                sizeof(int));
            mem.AddScatterReadRequest(
                handle,
                weaponServices[i] + ofs.CPlayer_WeaponServices_m_hMyWeapons + 0x8,
                &inventoryWeaponHandleArrays[i],
                sizeof(uintptr_t));
            queuedInventoryHandleReads = true;
        }
        for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
            const int i = inventoryPlayerSlots[inventorySlotIdx];
            if (!s_cachedInvHandleArrays[i] || s_cachedInvCounts[i] <= 0)
                continue;
            const int cachedSlotCount = std::clamp(s_cachedInvCounts[i], 0, kMaxInventoryWeapons);
            for (int slot = 0; slot < cachedSlotCount; ++slot) {
                mem.AddScatterReadRequest(
                    handle,
                    s_cachedInvHandleArrays[i] + static_cast<uintptr_t>(slot) * sizeof(uint32_t),
                    &inventoryWeaponHandles[i][slot],
                    sizeof(uint32_t));
            }
            queuedInventoryHandleReads = true;
        }
        if (queuedInventoryHandleReads) {
            if (!mem.ExecuteReadScatter(handle)) {
                inventoryHandleFailures = 1;
                memcpy(inventoryWeaponCounts, s_cachedInvCounts, sizeof(inventoryWeaponCounts));
                memcpy(inventoryWeaponHandleArrays, s_cachedInvHandleArrays, sizeof(inventoryWeaponHandleArrays));
                memcpy(inventoryWeaponHandles, s_cachedInventoryWeaponHandlesResolved, sizeof(inventoryWeaponHandles));
                memcpy(inventoryWeaponEntries, s_cachedInventoryWeaponEntries, sizeof(inventoryWeaponEntries));
                memcpy(inventoryWeapons, s_cachedInventoryWeaponsResolved, sizeof(inventoryWeapons));
                memcpy(inventoryWeaponIds, s_cachedInventoryWeaponIdsResolved, sizeof(inventoryWeaponIds));
                memcpy(inventoryHasBombBySlot, s_cachedInventoryHasBombResolved, sizeof(inventoryHasBombBySlot));
                logUpdateDataIssue("scatter_12_inv", "inventory_handles_failed_using_cached");
            } else {
                for (uintptr_t& handleArray : inventoryWeaponHandleArrays) {
                    if (!isLikelyGamePointer(handleArray))
                        handleArray = 0;
                }
                for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                    const int i = inventoryPlayerSlots[inventorySlotIdx];
                    if (!inventoryWeaponHandleArrays[i] ||
                        inventoryWeaponCounts[i] < 0 ||
                        inventoryWeaponCounts[i] > kMaxInventoryWeapons) {
                        inventoryWeaponCounts[i] = 0;
                        inventoryWeaponHandleArrays[i] = 0;
                        memset(inventoryWeaponHandles[i], 0, sizeof(inventoryWeaponHandles[i]));
                        continue;
                    }
                    for (int slot = 0; slot < kMaxInventoryWeapons; ++slot) {
                        if (!isValidEntityHandle(inventoryWeaponHandles[i][slot]))
                            inventoryWeaponHandles[i][slot] = 0;
                    }
                }

                bool arraysChanged = false;
                for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                    const int i = inventoryPlayerSlots[inventorySlotIdx];
                    if (inventoryWeaponHandleArrays[i] != s_cachedInvHandleArrays[i] ||
                        inventoryWeaponCounts[i] != s_cachedInvCounts[i]) {
                        arraysChanged = true;
                        break;
                    }
                }

                if (arraysChanged) {
                    memcpy(inventoryWeaponHandles, s_cachedInventoryWeaponHandlesResolved, sizeof(inventoryWeaponHandles));
                    bool queuedFreshHandleReads = false;
                    for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                        const int i = inventoryPlayerSlots[inventorySlotIdx];
                        const int inventorySlotCount = getInventorySlotCount(i);
                        if (!inventoryWeaponHandleArrays[i] || inventorySlotCount <= 0)
                            continue;
                        for (int slot = 0; slot < inventorySlotCount; ++slot) {
                            mem.AddScatterReadRequest(
                                handle,
                                inventoryWeaponHandleArrays[i] + static_cast<uintptr_t>(slot) * sizeof(uint32_t),
                                &inventoryWeaponHandles[i][slot],
                                sizeof(uint32_t));
                            queuedFreshHandleReads = true;
                        }
                    }
                    if (queuedFreshHandleReads && !mem.ExecuteReadScatter(handle)) {
                        inventoryHandleFailures = 1;
                        memcpy(inventoryWeaponHandles, s_cachedInventoryWeaponHandlesResolved, sizeof(inventoryWeaponHandles));
                        memcpy(inventoryWeaponEntries, s_cachedInventoryWeaponEntries, sizeof(inventoryWeaponEntries));
                        memcpy(inventoryWeapons, s_cachedInventoryWeaponsResolved, sizeof(inventoryWeapons));
                        memcpy(inventoryWeaponIds, s_cachedInventoryWeaponIdsResolved, sizeof(inventoryWeaponIds));
                        memcpy(inventoryHasBombBySlot, s_cachedInventoryHasBombResolved, sizeof(inventoryHasBombBySlot));
                        logUpdateDataIssue("scatter_12_inv_refresh", "inventory_handle_refresh_failed_using_cached");
                    }
                }

                bool inventoryHandlesChanged = false;
                for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount && !inventoryHandlesChanged; ++inventorySlotIdx) {
                    const int i = inventoryPlayerSlots[inventorySlotIdx];
                    const int slotCount = std::clamp(inventoryWeaponCounts[i], 0, kMaxInventoryWeapons);
                    for (int slot = 0; slot < slotCount; ++slot) {
                        if (inventoryWeaponHandles[i][slot] != s_cachedInventoryWeaponHandlesResolved[i][slot]) {
                            inventoryHandlesChanged = true;
                            break;
                        }
                    }
                }
                const bool inventoryMetaRefreshDue =
                    (g::espBombInfo || g::radarShowBomb || webRadarDemandActive) &&
                    (s_lastInventoryMetaRefreshUs == 0 ||
                     (inventoryNowUs - s_lastInventoryMetaRefreshUs) >= 32000u);
                if (inventoryMetaRefreshDue)
                    inventoryHandlesChanged = true;

                for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                    const int i = inventoryPlayerSlots[inventorySlotIdx];
                    s_cachedInvHandleArrays[i] = inventoryWeaponHandleArrays[i];
                    s_cachedInvCounts[i] = inventoryWeaponCounts[i];
                    memcpy(s_cachedInventoryWeaponHandlesResolved[i], inventoryWeaponHandles[i], sizeof(s_cachedInventoryWeaponHandlesResolved[i]));
                }

                if (inventoryHandlesChanged) {
                bool queuedInventoryEntryReads = false;
                
                for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                    const int i = inventoryPlayerSlots[inventorySlotIdx];
                    const int inventorySlotCount = getInventorySlotCount(i);
                    for (int slot = 0; slot < inventorySlotCount; ++slot) {
                        const uint32_t weaponHandle = inventoryWeaponHandles[i][slot];
                        if (!isValidEntityHandle(weaponHandle))
                            continue;
                        const uint32_t block = (weaponHandle & kEntityHandleMask) >> 9;
                        mem.AddScatterReadRequest(
                            handle,
                            entityList + 0x10 + 8 * block,
                            &inventoryWeaponEntries[i][slot],
                            sizeof(uintptr_t));
                        queuedInventoryEntryReads = true;
                    }
                }
                
                for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                    const int i = inventoryPlayerSlots[inventorySlotIdx];
                    const int inventorySlotCount = getInventorySlotCount(i);
                    for (int slot = 0; slot < inventorySlotCount; ++slot) {
                        const uint32_t weaponHandle = inventoryWeaponHandles[i][slot];
                        if (!isValidEntityHandle(weaponHandle) || !s_cachedInventoryWeaponEntries[i][slot])
                            continue;
                        const uint32_t handleSlot = weaponHandle & kEntitySlotMask;
                        mem.AddScatterReadRequest(
                            handle,
                            s_cachedInventoryWeaponEntries[i][slot] + kEntitySlotSize * handleSlot,
                            &inventoryWeapons[i][slot],
                            sizeof(uintptr_t));
                        queuedInventoryEntryReads = true;
                    }
                }
                
                for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                    const int i = inventoryPlayerSlots[inventorySlotIdx];
                    const int inventorySlotCount = getInventorySlotCount(i);
                    for (int slot = 0; slot < inventorySlotCount; ++slot) {
                        const uintptr_t cachedWeapon = s_cachedInventoryWeaponsResolved[i][slot];
                        if (!cachedWeapon)
                            continue;
                        const uintptr_t itemDefAddress =
                            cachedWeapon +
                            ofs.C_EconEntity_m_AttributeManager +
                            ofs.C_AttributeContainer_m_Item +
                            ofs.C_EconItemView_m_iItemDefinitionIndex;
                        mem.AddScatterReadRequest(
                            handle,
                            itemDefAddress,
                            &inventoryWeaponIds[i][slot],
                            sizeof(uint16_t));
                        queuedInventoryEntryReads = true;
                    }
                }
                if (queuedInventoryEntryReads) {
                    if (!mem.ExecuteReadScatter(handle)) {
                        weaponEntryFailures = std::max(weaponEntryFailures, 1);
                        memcpy(inventoryWeaponEntries, s_cachedInventoryWeaponEntries, sizeof(inventoryWeaponEntries));
                        memcpy(inventoryWeapons, s_cachedInventoryWeaponsResolved, sizeof(inventoryWeapons));
                        memcpy(inventoryWeaponIds, s_cachedInventoryWeaponIdsResolved, sizeof(inventoryWeaponIds));
                        memcpy(inventoryHasBombBySlot, s_cachedInventoryHasBombResolved, sizeof(inventoryHasBombBySlot));
                        logUpdateDataIssue("scatter_13_inv", "inventory_entries_failed_using_cached");
                    } else {
                        
                        bool inventoryEntriesChanged = false;
                        for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount && !inventoryEntriesChanged; ++inventorySlotIdx) {
                            const int i = inventoryPlayerSlots[inventorySlotIdx];
                            const int inventorySlotCount = getInventorySlotCount(i);
                            for (int slot = 0; slot < inventorySlotCount; ++slot) {
                                if (inventoryWeaponEntries[i][slot] != s_cachedInventoryWeaponEntries[i][slot]) {
                                    inventoryEntriesChanged = true;
                                    break;
                                }
                            }
                        }

                        if (inventoryEntriesChanged) {
                            
                            memcpy(inventoryWeapons, s_cachedInventoryWeaponsResolved, sizeof(inventoryWeapons));
                            bool queuedInventoryEntityRefresh = false;
                            for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                                const int i = inventoryPlayerSlots[inventorySlotIdx];
                                const int inventorySlotCount = getInventorySlotCount(i);
                                for (int slot = 0; slot < inventorySlotCount; ++slot) {
                                    const uint32_t weaponHandle = inventoryWeaponHandles[i][slot];
                                    if (!isValidEntityHandle(weaponHandle) || !inventoryWeaponEntries[i][slot])
                                        continue;
                                    const uint32_t handleSlot = weaponHandle & kEntitySlotMask;
                                    mem.AddScatterReadRequest(
                                        handle,
                                        inventoryWeaponEntries[i][slot] + kEntitySlotSize * handleSlot,
                                        &inventoryWeapons[i][slot],
                                        sizeof(uintptr_t));
                                    queuedInventoryEntityRefresh = true;
                                }
                            }
                            if (queuedInventoryEntityRefresh && !mem.ExecuteReadScatter(handle)) {
                                weaponEntityFailures = std::max(weaponEntityFailures, 1);
                                memcpy(inventoryWeapons, s_cachedInventoryWeaponsResolved, sizeof(inventoryWeapons));
                                memcpy(inventoryWeaponIds, s_cachedInventoryWeaponIdsResolved, sizeof(inventoryWeaponIds));
                                memcpy(inventoryHasBombBySlot, s_cachedInventoryHasBombResolved, sizeof(inventoryHasBombBySlot));
                                logUpdateDataIssue("scatter_14_inv_refresh", "inventory_entity_refresh_failed_using_cached");
                            }
                        }
                        for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                            const int i = inventoryPlayerSlots[inventorySlotIdx];
                            const int inventorySlotCount = getInventorySlotCount(i);
                            for (int slot = 0; slot < inventorySlotCount; ++slot) {
                                if (inventoryWeaponEntries[i][slot] && !isLikelyGamePointer(inventoryWeaponEntries[i][slot]))
                                    inventoryWeaponEntries[i][slot] = 0;
                                if (inventoryWeapons[i][slot] && !isLikelyGamePointer(inventoryWeapons[i][slot]))
                                    inventoryWeapons[i][slot] = 0;
                            }
                        }

                        
                        bool inventoryEntitiesChanged = inventoryEntriesChanged;
                        if (!inventoryEntitiesChanged) {
                            for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount && !inventoryEntitiesChanged; ++inventorySlotIdx) {
                                const int i = inventoryPlayerSlots[inventorySlotIdx];
                                const int inventorySlotCount = getInventorySlotCount(i);
                                for (int slot = 0; slot < inventorySlotCount; ++slot) {
                                    if (inventoryWeapons[i][slot] != s_cachedInventoryWeaponsResolved[i][slot]) {
                                        inventoryEntitiesChanged = true;
                                        break;
                                    }
                                }
                            }
                        }

                        for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                            const int i = inventoryPlayerSlots[inventorySlotIdx];
                            memcpy(s_cachedInventoryWeaponEntries[i], inventoryWeaponEntries[i], sizeof(s_cachedInventoryWeaponEntries[i]));
                            memcpy(s_cachedInventoryWeaponsResolved[i], inventoryWeapons[i], sizeof(s_cachedInventoryWeaponsResolved[i]));
                        }

                        
                        if (inventoryEntitiesChanged) {
                            bool queuedInventoryMetaReads = false;
                            for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                                const int i = inventoryPlayerSlots[inventorySlotIdx];
                                const int inventorySlotCount = getInventorySlotCount(i);
                                for (int slot = 0; slot < inventorySlotCount; ++slot) {
                                    const uintptr_t weaponEntity = inventoryWeapons[i][slot];
                                    if (!weaponEntity)
                                        continue;
                                    const uintptr_t itemDefAddress =
                                        weaponEntity +
                                        ofs.C_EconEntity_m_AttributeManager +
                                        ofs.C_AttributeContainer_m_Item +
                                        ofs.C_EconItemView_m_iItemDefinitionIndex;
                                    mem.AddScatterReadRequest(
                                        handle,
                                        itemDefAddress,
                                        &inventoryWeaponIds[i][slot],
                                        sizeof(uint16_t));
                                    queuedInventoryMetaReads = true;
                                }
                            }
                            if (queuedInventoryMetaReads && !mem.ExecuteReadScatter(handle)) {
                                weaponMetaFailures = std::max(weaponMetaFailures, 1);
                                memcpy(inventoryWeaponIds, s_cachedInventoryWeaponIdsResolved, sizeof(inventoryWeaponIds));
                                memcpy(inventoryHasBombBySlot, s_cachedInventoryHasBombResolved, sizeof(inventoryHasBombBySlot));
                                logUpdateDataIssue("scatter_15_inv", "inventory_meta_failed_using_cached");
                            }
                        }

                        memset(inventoryHasBombBySlot, 0, sizeof(inventoryHasBombBySlot));
                        for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
                            const int i = inventoryPlayerSlots[inventorySlotIdx];
                            const int inventorySlotCount = getInventorySlotCount(i);
                            for (int slot = 0; slot < inventorySlotCount; ++slot) {
                if (inventoryWeaponIds[i][slot] == kWeaponC4Id || inventoryWeapons[i][slot] == weaponC4Entity) {
                                    inventoryHasBombBySlot[i] = true;
                                    break;
                                }
                            }
                        }

                        for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx)
                            storeInventoryFullSlotToCache(inventoryPlayerSlots[inventorySlotIdx]);
                        if (inventoryMetaRefreshDue)
                            s_lastInventoryMetaRefreshUs = inventoryNowUs;
                    }
                }
                }
            }
        }
        s_lastFullInventoryLaneUs = inventoryNowUs;
    }
