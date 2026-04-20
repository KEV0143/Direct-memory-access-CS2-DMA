    const uint64_t worldSceneResetAge = nowUs - s_lastSceneResetUs.load(std::memory_order_relaxed);
    const bool worldScanAllowed = worldSceneResetAge > 2000000;
    
    
    
    
    
    const bool bombRescueScanAllowed = worldSceneResetAge > 300000;
    bool worldScanCommitted = false;

    const uint64_t worldScanIntervalUs = [&]() -> uint64_t {
        if (highestEntityIndex <= 800)  return 50000;
        if (highestEntityIndex <= 1200) return 70000;
        if (highestEntityIndex <= 2000) return 90000;
        return 120000;
    }();
    const bool heavyWorldCadence = (highestEntityIndex > 1200);
    const uint64_t worldUtilityDetailIntervalUs = [&]() -> uint64_t {
        if (!heavyWorldCadence)
            return worldScanIntervalUs;
        if (highestEntityIndex <= 1600)
            return wantsWorldProjectiles ? 110000 : 140000;
        if (highestEntityIndex <= 2400)
            return wantsWorldProjectiles ? 130000 : 160000;
        return wantsWorldProjectiles ? 150000 : 200000;
    }();

    static uint64_t s_worldCacheResetSerial = 0;
    static float s_prevWorldGameTime = 0.0f;
    static uint32_t s_worldWarmupScans = 0;
    static uint64_t s_lastWorldUtilityDetailScanUs = 0;
    static uint64_t s_lastWorldUtilityProbeScanUs = 0;
    static uint32_t s_worldDiscoveryShard = 0;
    static uint32_t s_worldIdleScanStreak = 0;
    static uint64_t s_lastWorldBombRescueScanUs = 0;
    static uint64_t s_lastWorldDroppedItemsScanUs = 0;
    static uint64_t s_lastWorldActiveUtilityScanUs = 0;
    static uint64_t s_lastWorldSlowDiscoveryScanUs = 0;

    const bool bombOnlyWorldMode =
        wantsBombConsumers &&
        !wantsDroppedItemMarkers &&
        !wantsWorldUtilityData;
    const bool hasKnownBombCandidateSlots = (s_worldBombCandidateSlotCount != 0u);
    
    
    
    
    
    const bool bombStateHasPosition =
        s_bombState.planted || (s_bombState.dropped && isValidWorldPos(s_bombState.position));
    const bool bombEntityCandidatePosValid =
        weaponC4Entity != 0 &&
        weaponC4PosValid &&
        isValidWorldPos(weaponC4WorldPos);
    const bool bombFallbackRescueNeeded =
        wantsBombConsumers &&
        !bombPlantedByRules &&
        !bombStateHasPosition &&
        !bombEntityCandidatePosValid;
    const bool bombRescueDiscoveryNeeded =
        bombFallbackRescueNeeded &&
        !hasKnownBombCandidateSlots;
    const bool forceBombWorldDiscovery =
        bombRescueDiscoveryNeeded &&
        !bombPlantedByRules &&
        wantsBombConsumers;

    const uint64_t worldCacheSceneResetSerial = s_sceneResetSerial.load(std::memory_order_relaxed);
    if (s_worldCacheResetSerial != worldCacheSceneResetSerial) {
        s_worldCacheResetSerial = worldCacheSceneResetSerial;
        s_prevWorldGameTime = 0.0f;
        s_worldWarmupScans = 0;
        s_lastWorldUtilityDetailScanUs = 0;
        s_lastWorldUtilityProbeScanUs = 0;
        s_worldDiscoveryShard = 0;
        s_worldIdleScanStreak = 0;
        s_lastWorldBombRescueScanUs = 0;
        s_lastWorldDroppedItemsScanUs = 0;
        s_lastWorldActiveUtilityScanUs = 0;
        s_lastWorldSlowDiscoveryScanUs = 0;
        std::memset(s_worldBombCandidateSlots, 0, sizeof(s_worldBombCandidateSlots));
        s_worldBombCandidateSlotCount = 0;
        std::memset(s_worldTrackedIndices, 0, sizeof(s_worldTrackedIndices));
        std::memset(s_worldTrackedIndexPos, 0, sizeof(s_worldTrackedIndexPos));
        s_worldTrackedIndexCount = 0;
    }

    const bool wantsGeneralWorldScan = wantsDroppedItemMarkers || wantsWorldUtilityData;
    const bool worldIdleDiscovery =
        !forceBombWorldDiscovery &&
        wantsGeneralWorldScan &&
        s_worldIdleScanStreak >= 3u &&
        s_worldTrackedIndexCount == 0 &&
        s_worldMarkerCount == 0;
    const uint64_t effectiveWorldUtilityDetailIntervalUs = [&]() -> uint64_t {
        if (worldIdleDiscovery) {
            if (highestEntityIndex <= 1200)
                return wantsWorldProjectiles ? 90000u : 140000u;
            if (highestEntityIndex <= 2200)
                return wantsWorldProjectiles ? 120000u : 180000u;
            return wantsWorldProjectiles ? 150000u : 220000u;
        }
        return worldUtilityDetailIntervalUs;
    }();
    const uint64_t effectiveWorldUtilityProbeIntervalUs = std::max<uint64_t>(
        effectiveWorldUtilityDetailIntervalUs,
        worldIdleDiscovery
            ? (wantsWorldProjectiles ? 180000u : 260000u)
            : (wantsWorldProjectiles ? 150000u : 180000u));
    const uint32_t worldDiscoveryShardCount = [&]() -> uint32_t {
        if (forceBombWorldDiscovery)
            return 1u;
        if (s_worldWarmupScans < 2u)
            return 1u;
        if (worldIdleDiscovery) {
            if (highestEntityIndex <= 800)
                return wantsWorldProjectiles ? 3u : 4u;
            if (highestEntityIndex <= 1400)
                return wantsWorldProjectiles ? 4u : 6u;
            if (highestEntityIndex <= 2200)
                return wantsWorldProjectiles ? 6u : 8u;
            return wantsWorldProjectiles ? 8u : 10u;
        }
        if (highestEntityIndex <= 800)
            return 2u;
        if (highestEntityIndex <= 1400)
            return 3u;
        if (highestEntityIndex <= 2200)
            return 4u;
        return 6u;
    }();
    const uint32_t activeWorldDiscoveryShard =
        (worldDiscoveryShardCount > 1u) ? (s_worldDiscoveryShard % worldDiscoveryShardCount) : 0u;

    const uint64_t worldDroppedItemsIntervalUs =
        worldIdleDiscovery ? std::max<uint64_t>(worldScanIntervalUs, 90000u) : worldScanIntervalUs;
    const uint64_t worldActiveUtilityIntervalUs =
        worldIdleDiscovery ? std::max<uint64_t>(effectiveWorldUtilityDetailIntervalUs, 90000u)
                           : effectiveWorldUtilityDetailIntervalUs;
    const uint64_t worldSlowDiscoveryIntervalUs =
        worldIdleDiscovery ? std::max<uint64_t>(worldScanIntervalUs * 2u, 120000u)
                           : worldScanIntervalUs;
    const uint64_t worldBombRescueIntervalUs =
        forceBombWorldDiscovery ? std::min<uint64_t>(worldScanIntervalUs, 15000u)
                                : worldScanIntervalUs;

    const bool worldDomainBombRescueDue =
        bombRescueScanAllowed &&
        bombFallbackRescueNeeded &&
        (nowUs - s_lastWorldBombRescueScanUs) >= worldBombRescueIntervalUs;
    const bool worldDomainDroppedItemsDue =
        worldScanAllowed &&
        wantsDroppedItemMarkers &&
        (nowUs - s_lastWorldDroppedItemsScanUs) >= worldDroppedItemsIntervalUs;
    const bool worldDomainActiveUtilityDue =
        worldScanAllowed &&
        wantsWorldUtilityData &&
        (nowUs - s_lastWorldActiveUtilityScanUs) >= worldActiveUtilityIntervalUs;
    const bool worldDomainSlowDiscoveryDue =
        worldScanAllowed &&
        (wantsGeneralWorldScan || bombRescueDiscoveryNeeded) &&
        (nowUs - s_lastWorldSlowDiscoveryScanUs) >=
            (forceBombWorldDiscovery ? std::min<uint64_t>(worldSlowDiscoveryIntervalUs, 15000u)
                                     : worldSlowDiscoveryIntervalUs);

    const bool shouldScanWorld =
        worldDomainBombRescueDue ||
        worldDomainDroppedItemsDue ||
        worldDomainActiveUtilityDue ||
        worldDomainSlowDiscoveryDue;
