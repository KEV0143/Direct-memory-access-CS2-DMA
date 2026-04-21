    static uintptr_t s_cachedActiveWeaponEntries[64] = {};
    static uintptr_t s_cachedInventoryWeaponEntries[64][kMaxInventoryWeapons] = {};
    static uintptr_t s_cachedWeaponServicesResolved[64] = {};
    static uint32_t s_cachedActiveWeaponHandles[64] = {};
    static uintptr_t s_cachedActiveWeaponsResolved[64] = {};
    static uintptr_t s_cachedClippingWeaponsResolved[64] = {};
    static uint16_t s_cachedWeaponIdsResolved[64] = {};
    static int s_cachedAmmoClipsResolved[64] = {};
    static uintptr_t s_cachedInvHandleArrays[64] = {};
    static int s_cachedInvCounts[64] = {};
    static uint32_t s_cachedInventoryWeaponHandlesResolved[64][kMaxInventoryWeapons] = {};
    static uintptr_t s_cachedInventoryWeaponsResolved[64][kMaxInventoryWeapons] = {};
    static uint16_t s_cachedInventoryWeaponIdsResolved[64][kMaxInventoryWeapons] = {};
    static bool s_cachedInventoryHasBombResolved[64] = {};
    static uintptr_t s_cachedInventoryPawns[64] = {};
    static uint64_t s_inventoryCacheResetSerial = 0;
    static uint64_t s_lastActiveWeaponLaneUs = 0;
    static uint64_t s_lastFullInventoryLaneUs = 0;
    static uint64_t s_lastWeaponServicesRefreshUs = 0;
    {
        const uint64_t inventoryResetSerial = s_sceneResetSerial.load(std::memory_order_relaxed);
        if (s_inventoryCacheResetSerial != inventoryResetSerial) {
            s_inventoryCacheResetSerial = inventoryResetSerial;
            s_lastActiveWeaponLaneUs = 0;
            s_lastFullInventoryLaneUs = 0;
            s_lastWeaponServicesRefreshUs = 0;
            memset(s_cachedActiveWeaponEntries, 0, sizeof(s_cachedActiveWeaponEntries));
            memset(s_cachedInventoryWeaponEntries, 0, sizeof(s_cachedInventoryWeaponEntries));
            memset(s_cachedWeaponServicesResolved, 0, sizeof(s_cachedWeaponServicesResolved));
            memset(s_cachedActiveWeaponHandles, 0, sizeof(s_cachedActiveWeaponHandles));
            memset(s_cachedActiveWeaponsResolved, 0, sizeof(s_cachedActiveWeaponsResolved));
            memset(s_cachedClippingWeaponsResolved, 0, sizeof(s_cachedClippingWeaponsResolved));
            memset(s_cachedWeaponIdsResolved, 0, sizeof(s_cachedWeaponIdsResolved));
            memset(s_cachedAmmoClipsResolved, 0, sizeof(s_cachedAmmoClipsResolved));
            memset(s_cachedInvHandleArrays, 0, sizeof(s_cachedInvHandleArrays));
            memset(s_cachedInvCounts, 0, sizeof(s_cachedInvCounts));
            memset(s_cachedInventoryWeaponHandlesResolved, 0, sizeof(s_cachedInventoryWeaponHandlesResolved));
            memset(s_cachedInventoryWeaponsResolved, 0, sizeof(s_cachedInventoryWeaponsResolved));
            memset(s_cachedInventoryWeaponIdsResolved, 0, sizeof(s_cachedInventoryWeaponIdsResolved));
            memset(s_cachedInventoryHasBombResolved, 0, sizeof(s_cachedInventoryHasBombResolved));
            memset(s_cachedInventoryPawns, 0, sizeof(s_cachedInventoryPawns));
        }
    }

    auto clearInventorySlotState = [&](int i) {
        weaponServices[i] = 0;
        activeWeaponHandles[i] = 0;
        activeWeaponEntries[i] = 0;
        activeWeapons[i] = 0;
        clippingWeapons[i] = 0;
        weaponIds[i] = 0;
        ammoClips[i] = 0;
        inventoryWeaponCounts[i] = 0;
        inventoryWeaponHandleArrays[i] = 0;
        memset(inventoryWeaponHandles[i], 0, sizeof(inventoryWeaponHandles[i]));
        memset(inventoryWeaponEntries[i], 0, sizeof(inventoryWeaponEntries[i]));
        memset(inventoryWeapons[i], 0, sizeof(inventoryWeapons[i]));
        memset(inventoryWeaponIds[i], 0, sizeof(inventoryWeaponIds[i]));
        inventoryHasBombBySlot[i] = false;

        s_cachedWeaponServicesResolved[i] = 0;
        s_cachedActiveWeaponHandles[i] = 0;
        s_cachedActiveWeaponEntries[i] = 0;
        s_cachedActiveWeaponsResolved[i] = 0;
        s_cachedClippingWeaponsResolved[i] = 0;
        s_cachedWeaponIdsResolved[i] = 0;
        s_cachedAmmoClipsResolved[i] = 0;
        s_cachedInvCounts[i] = 0;
        s_cachedInvHandleArrays[i] = 0;
        memset(s_cachedInventoryWeaponHandlesResolved[i], 0, sizeof(s_cachedInventoryWeaponHandlesResolved[i]));
        memset(s_cachedInventoryWeaponEntries[i], 0, sizeof(s_cachedInventoryWeaponEntries[i]));
        memset(s_cachedInventoryWeaponsResolved[i], 0, sizeof(s_cachedInventoryWeaponsResolved[i]));
        memset(s_cachedInventoryWeaponIdsResolved[i], 0, sizeof(s_cachedInventoryWeaponIdsResolved[i]));
        s_cachedInventoryHasBombResolved[i] = false;
    };

    const int inventoryCacheSlotLimit = std::max(
        playerSlotScanLimit,
        std::clamp(s_playerHierarchyHighWaterSlot.load(std::memory_order_relaxed), 0, 64));
    for (int i = 0; i < inventoryCacheSlotLimit; ++i) {
        if (pawns[i] != s_cachedInventoryPawns[i]) {
            clearInventorySlotState(i);
        }
        if (pawns[i])
            continue;
        s_cachedInventoryPawns[i] = 0;
        clearInventorySlotState(i);
    }
    for (int i = 0; i < inventoryCacheSlotLimit; ++i) {
        if (pawns[i])
            s_cachedInventoryPawns[i] = pawns[i];
    }

    int inventoryPlayerSlots[64] = {};
    int inventoryPlayerSlotCount = 0;
    bool inventoryPlayerSlotAdded[64] = {};
    auto addInventoryPlayerSlot = [&](int idx) {
        if (idx < 0 || idx >= 64 || inventoryPlayerSlotAdded[idx] || !pawns[idx])
            return;
        inventoryPlayerSlotAdded[idx] = true;
        inventoryPlayerSlots[inventoryPlayerSlotCount++] = idx;
    };

    const int localInventorySlot = ResolveLocalPlayerIndex(localControllerMaskBit, localMaskBit);
    addInventoryPlayerSlot(localInventorySlot);
    for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx)
        addInventoryPlayerSlot(playerResolvedSlots[resolvedIdx]);
    if (webRadarDemandActive) {
        for (int i = 0; i < inventoryCacheSlotLimit; ++i)
            addInventoryPlayerSlot(i);
    }

    auto copyInventorySlotFromCache = [&](int i) {
        weaponServices[i] = s_cachedWeaponServicesResolved[i];
        activeWeaponHandles[i] = s_cachedActiveWeaponHandles[i];
        activeWeaponEntries[i] = s_cachedActiveWeaponEntries[i];
        activeWeapons[i] = s_cachedActiveWeaponsResolved[i];
        clippingWeapons[i] = s_cachedClippingWeaponsResolved[i];
        weaponIds[i] = s_cachedWeaponIdsResolved[i];
        ammoClips[i] = s_cachedAmmoClipsResolved[i];
        inventoryWeaponCounts[i] = s_cachedInvCounts[i];
        inventoryWeaponHandleArrays[i] = s_cachedInvHandleArrays[i];
        memcpy(inventoryWeaponHandles[i], s_cachedInventoryWeaponHandlesResolved[i], sizeof(inventoryWeaponHandles[i]));
        memcpy(inventoryWeaponEntries[i], s_cachedInventoryWeaponEntries[i], sizeof(inventoryWeaponEntries[i]));
        memcpy(inventoryWeapons[i], s_cachedInventoryWeaponsResolved[i], sizeof(inventoryWeapons[i]));
        memcpy(inventoryWeaponIds[i], s_cachedInventoryWeaponIdsResolved[i], sizeof(inventoryWeaponIds[i]));
        inventoryHasBombBySlot[i] = s_cachedInventoryHasBombResolved[i];
    };

    auto storeInventoryActiveSlotToCache = [&](int i) {
        s_cachedWeaponServicesResolved[i] = weaponServices[i];
        s_cachedActiveWeaponHandles[i] = activeWeaponHandles[i];
        s_cachedActiveWeaponEntries[i] = activeWeaponEntries[i];
        s_cachedActiveWeaponsResolved[i] = activeWeapons[i];
        s_cachedClippingWeaponsResolved[i] = clippingWeapons[i];
        s_cachedWeaponIdsResolved[i] = weaponIds[i];
        s_cachedAmmoClipsResolved[i] = ammoClips[i];
    };

    auto storeInventoryFullSlotToCache = [&](int i) {
        s_cachedInvCounts[i] = inventoryWeaponCounts[i];
        s_cachedInvHandleArrays[i] = inventoryWeaponHandleArrays[i];
        memcpy(s_cachedInventoryWeaponHandlesResolved[i], inventoryWeaponHandles[i], sizeof(s_cachedInventoryWeaponHandlesResolved[i]));
        memcpy(s_cachedInventoryWeaponEntries[i], inventoryWeaponEntries[i], sizeof(s_cachedInventoryWeaponEntries[i]));
        memcpy(s_cachedInventoryWeaponsResolved[i], inventoryWeapons[i], sizeof(s_cachedInventoryWeaponsResolved[i]));
        memcpy(s_cachedInventoryWeaponIdsResolved[i], inventoryWeaponIds[i], sizeof(s_cachedInventoryWeaponIdsResolved[i]));
        s_cachedInventoryHasBombResolved[i] = inventoryHasBombBySlot[i];
    };

    for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx)
        copyInventorySlotFromCache(inventoryPlayerSlots[inventorySlotIdx]);

    if (ofs.C_CSPlayerPawn_m_pClippingWeapon <= 0) {
        for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
            const int i = inventoryPlayerSlots[inventorySlotIdx];
            clippingWeapons[i] = 0;
            s_cachedClippingWeaponsResolved[i] = 0;
        }
    }

    const uint64_t inventoryNowUs = TickNowUs();
    const bool wantsInventoryData =
        webRadarDemandActive ||
        g::espWeapon ||
        g::espWeaponAmmo ||
        g::espBombInfo ||
        g::radarShowBomb;
    const uint64_t fullInventoryLaneIntervalUs =
        webRadarDemandActive
        ? std::min<uint64_t>(esp::intervals::kInventoryFullInventoryLaneUs, 20000u)
        : esp::intervals::kInventoryFullInventoryLaneUs;
    bool noKnifePrimaryInventoryRefreshNeeded = false;
    if (wantsInventoryData && g::espWeapon && g::espWeaponIcon && g::espWeaponIconNoKnife) {
        for (int inventorySlotIdx = 0; inventorySlotIdx < inventoryPlayerSlotCount; ++inventorySlotIdx) {
            const int i = inventoryPlayerSlots[inventorySlotIdx];
            if (!IsKnifeItemId(weaponIds[i]))
                continue;

            const int inventorySlotCount = getInventorySlotCount(i);
            bool hasKnownInventoryWeapon = false;
            bool hasPrimaryInventoryWeapon = false;
            for (int slot = 0; slot < inventorySlotCount; ++slot) {
                const uint16_t candidate = inventoryWeaponIds[i][slot];
                if (candidate == 0 || candidate >= 20000u)
                    continue;
                hasKnownInventoryWeapon = true;
                if (IsPrimaryWeaponItemId(candidate)) {
                    hasPrimaryInventoryWeapon = true;
                    break;
                }
            }

            if (inventorySlotCount <= 0 || (!hasPrimaryInventoryWeapon && !hasKnownInventoryWeapon)) {
                noKnifePrimaryInventoryRefreshNeeded = true;
                break;
            }
        }
    }
    const bool activeWeaponLaneDue =
        wantsInventoryData &&
        (s_lastActiveWeaponLaneUs == 0 ||
         (inventoryNowUs - s_lastActiveWeaponLaneUs) >= esp::intervals::kInventoryActiveWeaponLaneUs);
    const bool fullInventoryLaneDue =
        wantsInventoryData &&
        (noKnifePrimaryInventoryRefreshNeeded ||
         s_lastFullInventoryLaneUs == 0 ||
         (inventoryNowUs - s_lastFullInventoryLaneUs) >= fullInventoryLaneIntervalUs);
    _inventoryActiveTick = activeWeaponLaneDue;
    _inventoryFullTick = fullInventoryLaneDue;

    int weaponHandleChainFailures = 0;
    int inventoryHandleFailures = 0;
    int weaponEntryFailures = 0;
    int weaponEntityFailures = 0;
    int weaponMetaFailures = 0;

    #include "inventory_parts/inventory_active_lane.inl"
    #include "inventory_parts/inventory_full_lane.inl"

    

    if (narrowDebugEnabled(kNarrowDebugWeaponChain)) {
        static uint32_t s_weaponChainDebugCounter = 0;
        const bool emitWeaponChainDebug =
            weaponHandleChainFailures > 0 ||
            inventoryHandleFailures > 0 ||
            weaponEntryFailures > 0 ||
            weaponEntityFailures > 0 ||
            weaponMetaFailures > 0 ||
            narrowDebugTick(kNarrowDebugWeaponChain, s_weaponChainDebugCounter, 40u);
        if (emitWeaponChainDebug) {
            int weaponServiceCount = 0;
            int activeHandleCount = 0;
            int activeEntityCount = 0;
            int inventoryWeaponCount = 0;
            int inventoryBombSlotCount = 0;
            for (int i = 0; i < playerSlotScanLimit; ++i) {
                if (weaponServices[i])
                    ++weaponServiceCount;
                if (activeWeaponHandles[i] && activeWeaponHandles[i] != 0xFFFFFFFFu)
                    ++activeHandleCount;
                if (activeWeapons[i] || clippingWeapons[i])
                    ++activeEntityCount;
                const int inventorySlotCount = getInventorySlotCount(i);
                for (int slot = 0; slot < inventorySlotCount; ++slot) {
                    if (inventoryWeaponHandles[i][slot] && inventoryWeaponHandles[i][slot] != 0xFFFFFFFFu)
                        ++inventoryWeaponCount;
                }
                if (inventoryHasBombBySlot[i])
                    ++inventoryBombSlotCount;
            }
            DmaLogPrintf(
                "[DEBUG] WeaponChain: fails(active/inv_handle/entry/entity/meta)=%d/%d/%d/%d/%d services=%d activeHandles=%d activeEntities=%d inventoryWeapons=%d inventoryBombSlots=%d weaponC4=0x%llX owner=0x%X",
                weaponHandleChainFailures,
                inventoryHandleFailures,
                weaponEntryFailures,
                weaponEntityFailures,
                weaponMetaFailures,
                weaponServiceCount,
                activeHandleCount,
                activeEntityCount,
                inventoryWeaponCount,
                inventoryBombSlotCount,
                static_cast<unsigned long long>(weaponC4Entity),
                weaponC4OwnerHandle);

            int sampleCount = 0;
            for (int i = 0; i < playerSlotScanLimit && sampleCount < 3; ++i) {
                int invValid = 0;
                const int inventorySlotCount = getInventorySlotCount(i);
                for (int slot = 0; slot < inventorySlotCount; ++slot) {
                    if (inventoryWeaponHandles[i][slot] && inventoryWeaponHandles[i][slot] != 0xFFFFFFFFu)
                        ++invValid;
                }
                if (!weaponServices[i] && invValid <= 0 && !bombCarrierBySlot[i] && !inventoryHasBombBySlot[i])
                    continue;
                ++sampleCount;
                DmaLogPrintf(
                    "[DEBUG] WeaponChain sample: slot=%d pawn=0x%llX activeHandle=0x%X activeEnt=0x%llX clip=0x%llX weaponId=%u ammo=%d invCount=%d bomb(carrier/inv)=%d/%d",
                    i,
                    static_cast<unsigned long long>(pawns[i]),
                    activeWeaponHandles[i],
                    static_cast<unsigned long long>(activeWeapons[i]),
                    static_cast<unsigned long long>(clippingWeapons[i]),
                    static_cast<unsigned>(weaponIds[i]),
                    ammoClips[i],
                    invValid,
                    bombCarrierBySlot[i] ? 1 : 0,
                    inventoryHasBombBySlot[i] ? 1 : 0);
            }
        }
    }

    

    WorldMarker scannedMarkers[256] = {};
    int scannedMarkerCount = 0;
