        if (worldScanOk && worldPositionsOk) {
        worldScanCommitted = true;
        int unknownUtilityProbeCount = 0;
        static thread_local int16_t s_worldPawnOwnerBySlot[kMaxTrackedWorldEntities + 1];
        static thread_local int16_t s_worldControllerOwnerBySlot[65];
        const bool needsOwnerPlayerMaps =
            worldDomainDroppedItemsDue ||
            worldDomainBombRescueDue;
        if (needsOwnerPlayerMaps) {
            std::memset(s_worldPawnOwnerBySlot, 0xFF, sizeof(s_worldPawnOwnerBySlot));
            std::memset(s_worldControllerOwnerBySlot, 0xFF, sizeof(s_worldControllerOwnerBySlot));
            for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
                const int playerIdx = playerResolvedSlots[resolvedIdx];
                const uint32_t pawnSlot = pawnHandles[playerIdx] & kEntityHandleMask;
                if (pawnSlot != 0u && pawnSlot <= kMaxTrackedWorldEntities)
                    s_worldPawnOwnerBySlot[pawnSlot] = static_cast<int16_t>(playerIdx);
                if (controllers[playerIdx])
                    s_worldControllerOwnerBySlot[playerIdx + 1] = static_cast<int16_t>(playerIdx);
            }
        }
        for (int processIdx = 0; processIdx < worldProcessCount; ++processIdx) {
            const int idx = s_worldProcessIndices[processIdx];
            const uintptr_t ent = worldEntities[idx];
            if (!ent) {
                if (isTrackedWorldEntitySlot(idx)) {
                    s_worldEntityRefs[idx] = 0;
                    s_worldEntitySubclassIds[idx] = 0;
                    s_worldEntityItemIds[idx] = 0;
                    resetWorldUtilityState(idx);
                    refreshTrackedWorldIndex(idx);
                }
                continue;
            }

            const Vector3 pos = worldPositions[idx];
            if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z))
                continue;

            const uint16_t rawItemId = worldItemDefs[idx];
            const uint16_t itemId = normalizeWorldItemId(rawItemId);
            const uint32_t subclassId = worldSubclassIds[idx];
            if (s_worldEntityRefs[idx] != ent || s_worldEntityItemIds[idx] != itemId || s_worldEntitySubclassIds[idx] != subclassId) {
                s_worldEntityRefs[idx] = ent;
                s_worldEntityItemIds[idx] = itemId;
                s_worldEntitySubclassIds[idx] = subclassId;
                resetWorldUtilityState(idx);
                refreshTrackedWorldIndex(idx);
            }

            const uint32_t owner = worldOwnerHandles[idx];
            const bool noOwner = (owner == 0u || owner == 0xFFFFFFFFu);
            const bool isHeGrenade = (itemId == 44);
            const bool isFlashGrenade = (itemId == 43);
            const bool isSmokeGrenade = (itemId == 45);
            const bool isMolotovGrenade = (itemId == 46);
            const bool isDecoyGrenade = (itemId == 47);
            const bool isIncendiaryGrenade = (itemId == 48);
            const bool isInfernoGrenade = isMolotovGrenade || isIncendiaryGrenade;
            const bool knownSmokeSubclass = hasTrackedSubclass(s_worldSmokeSubclassIds, subclassId);
            const bool knownDecoySubclass = hasTrackedSubclass(s_worldDecoySubclassIds, subclassId);
            const bool knownHeSubclass = hasTrackedSubclass(s_worldHeSubclassIds, subclassId);
            const bool knownInfernoSubclass = hasTrackedSubclass(s_worldInfernoSubclassIds, subclassId);
            const bool isUtilityGrenade = isFlashGrenade || isHeGrenade || isSmokeGrenade || isMolotovGrenade || isDecoyGrenade || isIncendiaryGrenade;
            const char* knownItemName = WeaponNameFromItemId(itemId);
            const bool knownDroppedItem = (knownItemName != nullptr) && !isUtilityGrenade && itemId != 49 && !IsKnifeItemId(itemId);
            const bool posNonOrigin = (std::fabs(pos.x) > 1.0f || std::fabs(pos.y) > 1.0f);
            const bool discoveryShardActive =
                (worldDiscoveryShardCount <= 1u) ||
                (static_cast<uint32_t>(idx - 65) % worldDiscoveryShardCount) == activeWorldDiscoveryShard;
            const bool droppedItemDiscoveryAllowed = !heavyWorldCadence || discoveryShardActive;
            const bool droppedItemCandidate =
                wantsDroppedItemMarkers &&
                !warmupWorldScan &&
                knownDroppedItem &&
                itemId < 1200 &&
                g::espItemEnabledMask.test(itemId) &&
                worldSceneNodes[idx] != 0 &&
                posNonOrigin &&
                droppedItemDiscoveryAllowed;
            const bool needsOwnerResolution = needsOwnerPlayerMaps && ((rawItemId == kWeaponC4Id) || droppedItemCandidate);
            int ownerPlayerIndex = -1;
            if (needsOwnerResolution && !noOwner) {
                if (rawItemId == kWeaponC4Id) {
                    ownerPlayerIndex = findPlayerIndexByEntityHandle(owner);
                } else {
                    const uint32_t ownerSlot = owner & kEntityHandleMask;
                    if (ownerSlot != 0u) {
                        if (ownerSlot <= kMaxTrackedWorldEntities && s_worldPawnOwnerBySlot[ownerSlot] >= 0)
                            ownerPlayerIndex = static_cast<int>(s_worldPawnOwnerBySlot[ownerSlot]);
                        else if (ownerSlot <= 64u && s_worldControllerOwnerBySlot[ownerSlot] >= 0)
                            ownerPlayerIndex = static_cast<int>(s_worldControllerOwnerBySlot[ownerSlot]);
                    }
                }
            }
            bool ownerHoldingNearby = false;
            if (ownerPlayerIndex >= 0 && ownerPlayerIndex < 64 &&
                healths[ownerPlayerIndex] > 0 &&
                lifeStates[ownerPlayerIndex] == 0 &&
                isFiniteVec(positions[ownerPlayerIndex])) {
                const float ownerDx = pos.x - positions[ownerPlayerIndex].x;
                const float ownerDy = pos.y - positions[ownerPlayerIndex].y;
                const float ownerDz = std::fabs(pos.z - positions[ownerPlayerIndex].z);
                ownerHoldingNearby = ((ownerDx * ownerDx + ownerDy * ownerDy) <= (120.0f * 120.0f)) && ownerDz <= 96.0f;
            }
            
            
            
            const bool ownerActiveWeaponMatches =
                (ownerPlayerIndex >= 0 && ownerPlayerIndex < 64 && itemId != 0 &&
                 weaponIds[ownerPlayerIndex] == itemId);
            const bool droppedOwnerReleased = noOwner || ownerPlayerIndex < 0 || !ownerHoldingNearby || !ownerActiveWeaponMatches;

            #include "world_process_dropped_c4.inl"
            #include "world_process_utility.inl"
        }
        }
        if (worldScanCommitted) {
            if (worldDomainBombRescueDue) {
                s_lastWorldBombRescueScanUs = nowUs;
                if (!worldScanFoundC4)
                    clearWorldBombCandidateSlots();
            }
            if (worldDomainDroppedItemsDue)
                s_lastWorldDroppedItemsScanUs = nowUs;
            if (worldDomainActiveUtilityDue)
                s_lastWorldActiveUtilityScanUs = nowUs;
            if (worldDomainSlowDiscoveryDue)
                s_lastWorldSlowDiscoveryScanUs = nowUs;
            if (scannedMarkerCount == 0 && s_worldTrackedIndexCount == 0) {
                if (s_worldIdleScanStreak < 255u)
                    ++s_worldIdleScanStreak;
            } else {
                s_worldIdleScanStreak = 0;
            }
            if (worldDiscoveryShardCount > 1u)
                s_worldDiscoveryShard = (activeWorldDiscoveryShard + 1u) % worldDiscoveryShardCount;
            else
                s_worldDiscoveryShard = 0;
            s_lastWorldScanUs = nowUs;
            s_lastWorldScanCommittedUs.store(nowUs, std::memory_order_relaxed);
        }

        if (kDebugWorldUtility) {
            static uint32_t s_dbgScan = 0;
            ++s_dbgScan;
            const bool anomaly = (dbgEvidenceEntities > 0 && (dbgSignalSmoke + dbgSignalInferno + dbgSignalDecoy + dbgSignalExplosive) == 0);
            if ((s_dbgScan % 12u) == 0u || anomaly) {
                DmaLogPrintf(
                    "[DEBUG] WorldESP: scan=%u time=%.2f interval=%.5f limit=%d markers=%d evidence=%d raw(S/I/D/E)=%d/%d/%d/%d signal=%d/%d/%d/%d anomaly=%d",
                    s_dbgScan,
                    currentGameTime,
                    safeIntervalPerTick,
                    worldLimit,
                    scannedMarkerCount,
                    dbgEvidenceEntities,
                    dbgRawSmoke,
                    dbgRawInferno,
                    dbgRawDecoy,
                    dbgRawExplosive,
                    dbgSignalSmoke,
                    dbgSignalInferno,
                    dbgSignalDecoy,
                    dbgSignalExplosive,
                    anomaly ? 1 : 0);
                for (int i = 0; i < dbgNoSignalSamples; ++i) {
                    const DebugSample& s = dbgSamples[i];
                    DmaLogPrintf(
                        "[DEBUG] WorldESP sample: idx=%d item=%u owner=%u ticks(s/i/d/d2/e)=%d/%d/%d/%d/%d fire=%d active=%d rem(s/i/d/e)=%.2f/%.2f/%.2f/%.2f",
                        s.idx,
                        static_cast<unsigned>(s.itemId),
                        static_cast<unsigned>(s.owner),
                        s.smokeTick,
                        s.infernoTick,
                        s.decoyTick,
                        s.decoyClientTick,
                        s.explodeTick,
                        s.infernoFireCount,
                        s.smokeActive,
                        s.smokeRemaining,
                        s.infernoRemaining,
                        s.decoyRemaining,
                        s.explodeRemaining);
                }
            }
        }
