    
    bool worldScanFoundC4 = false;
    uintptr_t worldScanC4Entity = 0;
    Vector3 worldScanC4Pos = {};
    int worldScanC4OwnerIdx = -1;
    bool worldScanC4OwnerAlive = false;
    bool worldScanC4OwnerNearby = false;
    bool worldScanC4NoOwner = false;
    int worldScanC4Score = (std::numeric_limits<int>::min)();
    const int worldSignOnState = s_engineSignOnState.load(std::memory_order_relaxed);
    const bool liveWorldContext =
        s_engineInGame.load(std::memory_order_relaxed) &&
        !s_engineMenu.load(std::memory_order_relaxed) &&
        worldSignOnState == 6;

    const bool kDebugWorldUtility = narrowDebugEnabled(kNarrowDebugWorld);
    const bool wantsDroppedItemMarkers = g::espItem;
    
    
    
    
    
    constexpr bool wantsWorldProjectiles = false;
    const bool wantsWorldUtilityMarkers = g::espWorld || webRadarDemandActive;
    const bool wantsWorldUtilityData = wantsWorldUtilityMarkers || wantsWorldProjectiles;
    const bool wantsBombConsumers = g::espBombInfo || g::radarShowBomb || webRadarDemandActive;
    const bool wantsWorldOwnerHandles =
        wantsWorldProjectiles ||
        wantsDroppedItemMarkers ||
        wantsBombConsumers;
    bool worldScanAttempted = false;
    bool worldScanOk = false;
    static int s_worldBombCandidateSlots[8] = {};
    static uint8_t s_worldBombCandidateSlotCount = 0;

    #include "world_parts/world_domain_scheduler.inl"
    if (shouldScanWorld && entityList && ofs.CGameSceneNode_m_vecAbsOrigin > 0 && highestEntityIndex > 64) {
        static bool s_warnedSmokeTickOffset = false;
        static bool s_warnedSmokeActiveOffset = false;
        static bool s_warnedInfernoTickOffset = false;
        static bool s_warnedDecoyTickOffset = false;
        static bool s_warnedExplodeTickOffset = false;
        if (ofs.C_SmokeGrenadeProjectile_m_nSmokeEffectTickBegin <= 0 && !s_warnedSmokeTickOffset) {
            s_warnedSmokeTickOffset = true;
            logUpdateDataIssue("scatter_20", "missing_offset_smoke_tick_begin");
        }
        if (ofs.C_SmokeGrenadeProjectile_m_bDidSmokeEffect <= 0 && !s_warnedSmokeActiveOffset) {
            s_warnedSmokeActiveOffset = true;
            logUpdateDataIssue("scatter_20", "missing_offset_smoke_active_flag");
        }
        if (ofs.C_Inferno_m_nFireEffectTickBegin <= 0 && !s_warnedInfernoTickOffset) {
            s_warnedInfernoTickOffset = true;
            logUpdateDataIssue("scatter_20", "missing_offset_inferno_tick_begin");
        }
        if (ofs.C_DecoyProjectile_m_nDecoyShotTick <= 0 && !s_warnedDecoyTickOffset) {
            s_warnedDecoyTickOffset = true;
            logUpdateDataIssue("scatter_20", "missing_offset_decoy_tick_begin");
        }
        if (ofs.C_BaseCSGrenadeProjectile_m_nExplodeEffectTickBegin <= 0 && !s_warnedExplodeTickOffset) {
            s_warnedExplodeTickOffset = true;
            logUpdateDataIssue("scatter_20", "missing_offset_explode_tick_begin");
        }

        #include "world_parts/world_read_context.inl"

        
        const bool shouldReadWorldUtilityDetails =
            worldDomainActiveUtilityDue &&
            (!heavyWorldCadence || (nowUs - s_lastWorldUtilityDetailScanUs) >= effectiveWorldUtilityDetailIntervalUs);
        const bool shouldReadWorldUtilityProbeDetails =
            shouldReadWorldUtilityDetails &&
            (!heavyWorldCadence || (nowUs - s_lastWorldUtilityProbeScanUs) >= effectiveWorldUtilityProbeIntervalUs);

        
        
        
        
        
        static uintptr_t s_cachedWorldSceneNodes[kMaxTrackedWorldEntities + 1] = {};
        static uint32_t s_cachedWorldOwnerHandles[kMaxTrackedWorldEntities + 1] = {};
        static Vector3 s_cachedWorldPositions[kMaxTrackedWorldEntities + 1] = {};
        static uint64_t s_worldDetailCacheResetSerial = 0;
        {
            const uint64_t detailResetSerial = s_sceneResetSerial.load(std::memory_order_relaxed);
            if (s_worldDetailCacheResetSerial != detailResetSerial) {
                s_worldDetailCacheResetSerial = detailResetSerial;
                memset(s_cachedWorldSceneNodes, 0, sizeof(s_cachedWorldSceneNodes));
                memset(s_cachedWorldOwnerHandles, 0, sizeof(s_cachedWorldOwnerHandles));
                memset(s_cachedWorldPositions, 0, sizeof(s_cachedWorldPositions));
            }
        }

        
        auto queueUtilityReads = [&](int idx, uintptr_t ent, uint16_t itemId, uint32_t subclassId) {
            const bool isSmokeGrenade = (itemId == 45);
            const bool isMolotovGrenade = (itemId == 46);
            const bool isIncendiaryGrenade = (itemId == 48);
            const bool isInfernoGrenade = isMolotovGrenade || isIncendiaryGrenade;
            const bool isDecoyGrenade = (itemId == 47);
            const bool isHeGrenade = (itemId == 44);
            const bool knownSmokeSubclass = hasTrackedSubclass(s_worldSmokeSubclassIds, subclassId);
            const bool knownDecoySubclass = hasTrackedSubclass(s_worldDecoySubclassIds, subclassId);
            const bool knownHeSubclass = hasTrackedSubclass(s_worldHeSubclassIds, subclassId);
            const bool knownInfernoSubclass = hasTrackedSubclass(s_worldInfernoSubclassIds, subclassId);

            if (isSmokeGrenade || knownSmokeSubclass) {
                if (ofs.C_SmokeGrenadeProjectile_m_nSmokeEffectTickBegin > 0)
                    mem.AddScatterReadRequest(handle, ent + ofs.C_SmokeGrenadeProjectile_m_nSmokeEffectTickBegin, &worldSmokeTick[idx], sizeof(int));
                if (ofs.C_SmokeGrenadeProjectile_m_bDidSmokeEffect > 0)
                    mem.AddScatterReadRequest(handle, ent + ofs.C_SmokeGrenadeProjectile_m_bDidSmokeEffect, &worldSmokeActive[idx], sizeof(uint8_t));
                if (ofs.C_SmokeGrenadeProjectile_m_bSmokeVolumeDataReceived > 0)
                    mem.AddScatterReadRequest(handle, ent + ofs.C_SmokeGrenadeProjectile_m_bSmokeVolumeDataReceived, &worldSmokeVolumeDataReceived[idx], sizeof(uint8_t));
                if (ofs.C_SmokeGrenadeProjectile_m_bSmokeEffectSpawned > 0)
                    mem.AddScatterReadRequest(handle, ent + ofs.C_SmokeGrenadeProjectile_m_bSmokeEffectSpawned, &worldSmokeEffectSpawned[idx], sizeof(uint8_t));
                if (ofs.C_BaseEntity_m_vecVelocity > 0 && wantsWorldProjectiles && isSmokeGrenade)
                    mem.AddScatterReadRequest(handle, ent + ofs.C_BaseEntity_m_vecVelocity, &worldVelocities[idx], sizeof(Vector3));
            }
            if (isInfernoGrenade || knownInfernoSubclass) {
                if (ofs.C_Inferno_m_nFireEffectTickBegin > 0)
                    mem.AddScatterReadRequest(handle, ent + ofs.C_Inferno_m_nFireEffectTickBegin, &worldInfernoTick[idx], sizeof(int));
                if (ofs.C_Inferno_m_nFireLifetime > 0)
                    mem.AddScatterReadRequest(handle, ent + ofs.C_Inferno_m_nFireLifetime, &worldInfernoLife[idx], sizeof(float));
                if (ofs.C_Inferno_m_fireCount > 0)
                    mem.AddScatterReadRequest(handle, ent + ofs.C_Inferno_m_fireCount, &worldInfernoFireCount[idx], sizeof(int));
                if (ofs.C_Inferno_m_bInPostEffectTime > 0)
                    mem.AddScatterReadRequest(handle, ent + ofs.C_Inferno_m_bInPostEffectTime, &worldInfernoInPostEffect[idx], sizeof(uint8_t));
                if (ofs.C_BaseEntity_m_vecVelocity > 0 && wantsWorldProjectiles && isInfernoGrenade)
                    mem.AddScatterReadRequest(handle, ent + ofs.C_BaseEntity_m_vecVelocity, &worldVelocities[idx], sizeof(Vector3));
            }
            if (isDecoyGrenade || knownDecoySubclass) {
                if (ofs.C_DecoyProjectile_m_nDecoyShotTick > 0)
                    mem.AddScatterReadRequest(handle, ent + ofs.C_DecoyProjectile_m_nDecoyShotTick, &worldDecoyTick[idx], sizeof(int));
                if (ofs.C_DecoyProjectile_m_nClientLastKnownDecoyShotTick > 0)
                    mem.AddScatterReadRequest(handle, ent + ofs.C_DecoyProjectile_m_nClientLastKnownDecoyShotTick, &worldDecoyClientTick[idx], sizeof(int));
                if (ofs.C_BaseEntity_m_vecVelocity > 0 && wantsWorldProjectiles && isDecoyGrenade)
                    mem.AddScatterReadRequest(handle, ent + ofs.C_BaseEntity_m_vecVelocity, &worldVelocities[idx], sizeof(Vector3));
            }
            if ((isHeGrenade || knownHeSubclass) && ofs.C_BaseCSGrenadeProjectile_m_nExplodeEffectTickBegin > 0)
                mem.AddScatterReadRequest(handle, ent + ofs.C_BaseCSGrenadeProjectile_m_nExplodeEffectTickBegin, &worldExplodeTick[idx], sizeof(int));
        };

        
        auto queueProbeReads = [&](int idx, uintptr_t ent, uint32_t subclassId) {
            const bool knownSmoke = hasTrackedSubclass(s_worldSmokeSubclassIds, subclassId);
            const bool knownInferno = hasTrackedSubclass(s_worldInfernoSubclassIds, subclassId);
            const bool knownDecoy = hasTrackedSubclass(s_worldDecoySubclassIds, subclassId);
            const bool knownHe = hasTrackedSubclass(s_worldHeSubclassIds, subclassId);
            if (!knownSmoke) {
                if (ofs.C_SmokeGrenadeProjectile_m_nSmokeEffectTickBegin > 0)
                    mem.AddScatterReadRequest(handle, ent + ofs.C_SmokeGrenadeProjectile_m_nSmokeEffectTickBegin, &worldSmokeTick[idx], sizeof(int));
                if (ofs.C_SmokeGrenadeProjectile_m_bDidSmokeEffect > 0)
                    mem.AddScatterReadRequest(handle, ent + ofs.C_SmokeGrenadeProjectile_m_bDidSmokeEffect, &worldSmokeActive[idx], sizeof(uint8_t));
                if (ofs.C_SmokeGrenadeProjectile_m_bSmokeVolumeDataReceived > 0)
                    mem.AddScatterReadRequest(handle, ent + ofs.C_SmokeGrenadeProjectile_m_bSmokeVolumeDataReceived, &worldSmokeVolumeDataReceived[idx], sizeof(uint8_t));
                if (ofs.C_SmokeGrenadeProjectile_m_bSmokeEffectSpawned > 0)
                    mem.AddScatterReadRequest(handle, ent + ofs.C_SmokeGrenadeProjectile_m_bSmokeEffectSpawned, &worldSmokeEffectSpawned[idx], sizeof(uint8_t));
            }
            if (!knownInferno) {
                if (ofs.C_Inferno_m_nFireEffectTickBegin > 0)
                    mem.AddScatterReadRequest(handle, ent + ofs.C_Inferno_m_nFireEffectTickBegin, &worldInfernoTick[idx], sizeof(int));
                if (ofs.C_Inferno_m_nFireLifetime > 0)
                    mem.AddScatterReadRequest(handle, ent + ofs.C_Inferno_m_nFireLifetime, &worldInfernoLife[idx], sizeof(float));
                if (ofs.C_Inferno_m_fireCount > 0)
                    mem.AddScatterReadRequest(handle, ent + ofs.C_Inferno_m_fireCount, &worldInfernoFireCount[idx], sizeof(int));
                if (ofs.C_Inferno_m_bInPostEffectTime > 0)
                    mem.AddScatterReadRequest(handle, ent + ofs.C_Inferno_m_bInPostEffectTime, &worldInfernoInPostEffect[idx], sizeof(uint8_t));
            }
            if (!knownDecoy) {
                if (ofs.C_DecoyProjectile_m_nDecoyShotTick > 0)
                    mem.AddScatterReadRequest(handle, ent + ofs.C_DecoyProjectile_m_nDecoyShotTick, &worldDecoyTick[idx], sizeof(int));
                if (ofs.C_DecoyProjectile_m_nClientLastKnownDecoyShotTick > 0)
                    mem.AddScatterReadRequest(handle, ent + ofs.C_DecoyProjectile_m_nClientLastKnownDecoyShotTick, &worldDecoyClientTick[idx], sizeof(int));
            }
            if (!knownHe && ofs.C_BaseCSGrenadeProjectile_m_nExplodeEffectTickBegin > 0)
                mem.AddScatterReadRequest(handle, ent + ofs.C_BaseCSGrenadeProjectile_m_nExplodeEffectTickBegin, &worldExplodeTick[idx], sizeof(int));
        };
        static thread_local int s_probeQueuedIndices[kMaxTrackedWorldEntities + 1];
        int probeQueuedCount = 0;

        
        
        static thread_local uint8_t s_prefetchedPositionMask[kMaxTrackedWorldEntities + 1];
        std::memset(s_prefetchedPositionMask, 0, sizeof(s_prefetchedPositionMask));

        
        
        
        
        
        {
            
            for (int b = 0; b < blockCount; ++b)
                mem.AddScatterReadRequest(handle, entityList + 0x10 + static_cast<uintptr_t>(b) * 8u, &worldBlocks[b], sizeof(uintptr_t));

            
            for (int candidateIdx = 0; candidateIdx < worldCandidateCount; ++candidateIdx) {
                const int idx = s_worldCandidateIndices[candidateIdx];
                const int blockIdx = idx >> 9;
                if (blockIdx < 0 || blockIdx >= s_cachedWorldBlockCount)
                    continue;
                const uintptr_t cachedBlock = s_cachedWorldBlocks[blockIdx];
                if (!cachedBlock)
                    continue;
                mem.AddScatterReadRequest(handle, cachedBlock + kEntitySlotSize * (static_cast<uint32_t>(idx) & kEntitySlotMask), &worldEntities[idx], sizeof(uintptr_t));
            }

            
            
            
            for (int candidateIdx = 0; candidateIdx < worldCandidateCount; ++candidateIdx) {
                const int idx = s_worldCandidateIndices[candidateIdx];
                const uintptr_t cachedEnt = s_worldEntityRefs[idx];
                if (!cachedEnt)
                    continue;
                const uintptr_t cachedSceneNode = s_cachedWorldSceneNodes[idx];
                if (!cachedSceneNode)
                    continue;
                const uint16_t cachedItemId = s_worldEntityItemIds[idx];
                const uint32_t cachedSubclassId = s_worldEntitySubclassIds[idx];
                
                if (cachedItemId == 0 && cachedSubclassId == 0)
                    continue;

                const bool cachedKnownSmokeSubclass = hasTrackedSubclass(s_worldSmokeSubclassIds, cachedSubclassId);
                const bool cachedKnownDecoySubclass = hasTrackedSubclass(s_worldDecoySubclassIds, cachedSubclassId);
                const bool cachedKnownHeSubclass = hasTrackedSubclass(s_worldHeSubclassIds, cachedSubclassId);
                const bool cachedKnownInfernoSubclass = hasTrackedSubclass(s_worldInfernoSubclassIds, cachedSubclassId);
                const bool cachedBombCandidate =
                    cachedItemId == kWeaponC4Id &&
                    worldDomainBombRescueDue;
                const bool cachedDroppedItemCandidate =
                    worldDomainDroppedItemsDue &&
                    cachedItemId != 0 &&
                    cachedItemId != kWeaponC4Id &&
                    WeaponNameFromItemId(cachedItemId) != nullptr &&
                    !isUtilityWorldItemId(cachedItemId) &&
                    !IsKnifeItemId(cachedItemId);
                const bool cachedUtilityCandidate =
                    worldDomainActiveUtilityDue &&
                    (isUtilityWorldItemId(cachedItemId) ||
                     cachedKnownSmokeSubclass ||
                     cachedKnownDecoySubclass ||
                     cachedKnownHeSubclass ||
                     cachedKnownInfernoSubclass ||
                     s_worldUtilityHasHistory[idx]);
                const bool cachedProbeCandidate =
                    shouldReadWorldUtilityProbeDetails &&
                    worldDomainActiveUtilityDue &&
                    cachedItemId == 0 &&
                    cachedSubclassId != 0 &&
                    !cachedKnownSmokeSubclass &&
                    !cachedKnownDecoySubclass &&
                    !cachedKnownHeSubclass &&
                    !cachedKnownInfernoSubclass;
                if (!(cachedBombCandidate ||
                      cachedDroppedItemCandidate ||
                      cachedUtilityCandidate ||
                      cachedProbeCandidate))
                    continue;

                
                if (ofs.CGameSceneNode_m_vecAbsOrigin > 0) {
                    mem.AddScatterReadRequest(handle, cachedSceneNode + ofs.CGameSceneNode_m_vecAbsOrigin, &worldPositions[idx], sizeof(Vector3));
                    s_prefetchedPositionMask[idx] = 1u;
                }

                
                if (shouldReadWorldUtilityDetails) {
                    const bool isUtility =
                        isUtilityWorldItemId(cachedItemId) ||
                        cachedKnownSmokeSubclass ||
                        cachedKnownDecoySubclass ||
                        cachedKnownHeSubclass ||
                        cachedKnownInfernoSubclass;
                    if (isUtility)
                        queueUtilityReads(idx, cachedEnt, cachedItemId, cachedSubclassId);
                }

                
                if (cachedProbeCandidate) {
                    queueProbeReads(idx, cachedEnt, cachedSubclassId);
                    s_probeQueuedIndices[probeQueuedCount++] = idx;
                }
            }

            
            worldScanAttempted = true;
            worldScanOk = executeOptionalScatterRead();

            if (worldScanOk) {
                
                bool blocksChanged = false;
                for (int b = 0; b < blockCount; ++b) {
                    if (!isLikelyGamePointer(worldBlocks[b]))
                        worldBlocks[b] = s_cachedWorldBlocks[b];
                    else if (worldBlocks[b] != s_cachedWorldBlocks[b])
                        blocksChanged = true;
                }

                if (blocksChanged) {
                    
                    
                    for (int b = 0; b < blockCount; ++b)
                        s_cachedWorldBlocks[b] = worldBlocks[b];
                    s_cachedWorldBlockCount = blockCount;

                    
                    std::memset(s_prefetchedPositionMask, 0, sizeof(s_prefetchedPositionMask));
                    for (int candidateIdx = 0; candidateIdx < worldCandidateCount; ++candidateIdx) {
                        const int idx = s_worldCandidateIndices[candidateIdx];
                        worldEntities[idx] = 0;
                        worldPositions[idx] = {};
                        worldOwnerHandles[idx] = 0;
                    }

                    
                    for (int candidateIdx = 0; candidateIdx < worldCandidateCount; ++candidateIdx) {
                        const int idx = s_worldCandidateIndices[candidateIdx];
                        const int blockIdx = idx >> 9;
                        if (blockIdx < 0 || blockIdx >= blockCount)
                            continue;
                        if (!worldBlocks[blockIdx])
                            continue;
                        mem.AddScatterReadRequest(handle, worldBlocks[blockIdx] + kEntitySlotSize * (static_cast<uint32_t>(idx) & kEntitySlotMask), &worldEntities[idx], sizeof(uintptr_t));
                    }
                    worldScanOk = executeOptionalScatterRead();
                } else {
                    
                    for (int b = 0; b < blockCount; ++b)
                        s_cachedWorldBlocks[b] = worldBlocks[b];
                    s_cachedWorldBlockCount = blockCount;
                }

                
                if (shouldReadWorldUtilityDetails)
                    s_lastWorldUtilityDetailScanUs = nowUs;

            }
        }






        int newEntityCount = 0;
        int skippedStaticCount = 0;
        if (worldScanOk) {
            
            
            
            
            for (int candidateIdx = 0; candidateIdx < worldCandidateCount; ++candidateIdx) {
                const int idx = s_worldCandidateIndices[candidateIdx];
                uintptr_t ent = worldEntities[idx];
                if (ent && !isLikelyGamePointer(ent)) {
                    worldEntities[idx] = 0;
                    ent = 0;
                }
                if (!ent)
                    continue;

                const bool entityUnchanged = (ent == s_worldEntityRefs[idx]);
                if (entityUnchanged && s_cachedWorldSceneNodes[idx]) {
                    
                    worldSceneNodes[idx] = s_cachedWorldSceneNodes[idx];
                    worldOwnerHandles[idx] = s_cachedWorldOwnerHandles[idx];
                    worldSubclassIds[idx] = s_worldEntitySubclassIds[idx];
                    worldItemDefs[idx] = s_worldEntityItemIds[idx];
                    
                    if (!(worldItemDefs[idx] != 0 || worldSubclassIds[idx] != 0))
                        ++skippedStaticCount;
                } else {
                    
                    ++newEntityCount;
                    if (s_prefetchedPositionMask[idx]) {
                        worldPositions[idx] = {};
                        worldOwnerHandles[idx] = 0;
                    }



                    mem.AddScatterReadRequest(handle, ent + ofs.C_BaseEntity_m_pGameSceneNode, &worldSceneNodes[idx], sizeof(uintptr_t));
                    if (wantsWorldOwnerHandles && ofs.C_BaseEntity_m_hOwnerEntity > 0)
                        mem.AddScatterReadRequest(handle, ent + ofs.C_BaseEntity_m_hOwnerEntity, &worldOwnerHandles[idx], sizeof(uint32_t));
                    if (!bombOnlyWorldMode && ofs.C_BaseEntity_m_nSubclassID > 0)
                        mem.AddScatterReadRequest(handle, ent + ofs.C_BaseEntity_m_nSubclassID, &worldSubclassIds[idx], sizeof(uint32_t));
                    if (ofs.C_EconEntity_m_AttributeManager > 0 && ofs.C_AttributeContainer_m_Item > 0 && ofs.C_EconItemView_m_iItemDefinitionIndex > 0) {
                        const uintptr_t itemDefAddress =
                            ent + ofs.C_EconEntity_m_AttributeManager + ofs.C_AttributeContainer_m_Item + ofs.C_EconItemView_m_iItemDefinitionIndex;
                        mem.AddScatterReadRequest(handle, itemDefAddress, &worldItemDefs[idx], sizeof(uint16_t));
                    }
                }
            }
            
            if (newEntityCount > 0) {
                if (!executeOptionalScatterRead()) {
                    
                    for (int candidateIdx = 0; candidateIdx < worldCandidateCount; ++candidateIdx) {
                        const int idx = s_worldCandidateIndices[candidateIdx];
                        if (worldEntities[idx] && worldEntities[idx] != s_worldEntityRefs[idx]) {
                            worldSceneNodes[idx] = 0;
                            worldOwnerHandles[idx] = 0;
                            worldSubclassIds[idx] = 0;
                            worldItemDefs[idx] = 0;
                        }
                    }
                }
            }
        }


        if (worldScanOk && newEntityCount > 0) {
            for (int candidateIdx = 0; candidateIdx < worldCandidateCount; ++candidateIdx) {
                const int idx = s_worldCandidateIndices[candidateIdx];
                const uintptr_t ent = worldEntities[idx];
                if (!ent) continue;
                const bool entityUnchanged = (ent == s_worldEntityRefs[idx]) && s_cachedWorldSceneNodes[idx];
                if (entityUnchanged) continue;
                if (!worldSceneNodes[idx]) continue;
                if (ofs.CGameSceneNode_m_vecAbsOrigin > 0 &&
                    (!bombOnlyWorldMode || worldItemDefs[idx] == kWeaponC4Id || s_worldEntityItemIds[idx] == kWeaponC4Id))
                    mem.AddScatterReadRequest(handle, worldSceneNodes[idx] + ofs.CGameSceneNode_m_vecAbsOrigin, &worldPositions[idx], sizeof(Vector3));
                if (shouldReadWorldUtilityDetails)
                    queueUtilityReads(idx, ent, worldItemDefs[idx], worldSubclassIds[idx]);
            }
            executeOptionalScatterRead();
        }

        
        for (int candidateIdx = 0; candidateIdx < worldCandidateCount; ++candidateIdx) {
            const int idx = s_worldCandidateIndices[candidateIdx];
            s_worldEntityChangedFlags[idx] = static_cast<uint8_t>(worldEntities[idx] != s_worldEntityRefs[idx]);
            if (worldEntities[idx] && worldSceneNodes[idx]) {
                s_cachedWorldSceneNodes[idx] = worldSceneNodes[idx];
                s_cachedWorldOwnerHandles[idx] = worldOwnerHandles[idx];
                if (std::isfinite(worldPositions[idx].x))
                    s_cachedWorldPositions[idx] = worldPositions[idx];
                else if (s_cachedWorldPositions[idx].x != 0.0f)
                    worldPositions[idx] = s_cachedWorldPositions[idx]; 
            } else if (!worldEntities[idx]) {
                s_cachedWorldSceneNodes[idx] = 0;
                s_cachedWorldOwnerHandles[idx] = 0;
                s_cachedWorldPositions[idx] = {};
            }
        }
        int worldProcessCount = 0;
        for (int candidateIdx = 0; candidateIdx < worldCandidateCount; ++candidateIdx) {
            const int idx = s_worldCandidateIndices[candidateIdx];
            const bool hadTrackedState = shouldTrackWorldSlotForDueDomains(idx);
            if (!worldEntities[idx]) {
                if (hadTrackedState)
                    s_worldProcessIndices[worldProcessCount++] = idx;
                continue;
            }
            const uint16_t itemId = worldItemDefs[idx];
            const uint32_t subclassId = worldSubclassIds[idx];
            const bool featureRelevantEntity =
                (itemId == kWeaponC4Id && worldDomainBombRescueDue) ||
                (worldDomainDroppedItemsDue &&
                 itemId != 0 &&
                 itemId != kWeaponC4Id &&
                 WeaponNameFromItemId(itemId) != nullptr &&
                 !isUtilityWorldItemId(itemId) &&
                 !IsKnifeItemId(itemId)) ||
                (worldDomainActiveUtilityDue &&
                 (isUtilityWorldItemId(itemId) ||
                  subclassId != 0 ||
                  s_worldUtilityHasHistory[idx]));
            if (s_worldEntityChangedFlags[idx] != 0 ||
                hadTrackedState ||
                featureRelevantEntity) {
                s_worldProcessIndices[worldProcessCount++] = idx;
            }
        }
        static thread_local int s_worldUtilityProcessIndices[kMaxTrackedWorldEntities + 1];
        int worldUtilityProcessCount = 0;
        for (int processIdx = 0; processIdx < worldProcessCount; ++processIdx) {
            const int idx = s_worldProcessIndices[processIdx];
            if (!worldEntities[idx])
                continue;

            const uint16_t itemId = worldItemDefs[idx];
            const uint32_t subclassId = worldSubclassIds[idx];
            const bool utilityTrackedState =
                s_worldUtilityHasHistory[idx] ||
                s_worldSmokeLatched[idx] ||
                s_worldInfernoLatched[idx] ||
                s_worldDecoyLatched[idx] ||
                s_worldExplosiveLatched[idx] ||
                s_worldSmokeEvidenceCount[idx] != 0 ||
                s_worldInfernoEvidenceCount[idx] != 0 ||
                s_worldDecoyEvidenceCount[idx] != 0 ||
                s_worldExplosiveEvidenceCount[idx] != 0;
            const bool utilityCandidate =
                worldDomainActiveUtilityDue &&
                (isUtilityWorldItemId(itemId) ||
                 hasTrackedSubclass(s_worldSmokeSubclassIds, subclassId) ||
                 hasTrackedSubclass(s_worldDecoySubclassIds, subclassId) ||
                 hasTrackedSubclass(s_worldHeSubclassIds, subclassId) ||
                 hasTrackedSubclass(s_worldInfernoSubclassIds, subclassId) ||
                 utilityTrackedState);
            if (utilityCandidate)
                s_worldUtilityProcessIndices[worldUtilityProcessCount++] = idx;
        }

        #include "world_parts/world_marker_synthesis.inl"
        #include "world_parts/world_utility_probe.inl"
        #include "world_parts/world_process_entities.inl"
    } else if (kDebugWorldUtility) {
        static uint32_t s_dbgSkipScan = 0;
        ++s_dbgSkipScan;
        if ((s_dbgSkipScan % 24u) == 0u) {
            const bool modeEnabled = (g::espWorld || g::espBombInfo || g::radarShowBomb || webRadarDemandActive);
            const uint64_t sinceLast = nowUs - s_lastWorldScanUs;
            DmaLogPrintf(
                "[DEBUG] WorldESP skipped: mode=%d gate=%d since=%llu entityList=%d posOfs=%d highest=%d shouldScan=%d",
                modeEnabled ? 1 : 0,
                (sinceLast >= 120000) ? 1 : 0,
                static_cast<unsigned long long>(sinceLast),
                entityList ? 1 : 0,
                (ofs.CGameSceneNode_m_vecAbsOrigin > 0) ? 1 : 0,
                highestEntityIndex,
                shouldScanWorld ? 1 : 0);
        }
    }
    if (!liveWorldContext) {
        for (int i = 0; i < s_worldMarkerCount && i < 256; ++i)
            s_worldMarkers[i].valid = false;
        s_worldMarkerCount = 0;
        s_lastWorldScanUs = 0;
    }
    const bool worldSubsystemActive =
        liveWorldContext &&
        (wantsWorldUtilityMarkers ||
         wantsDroppedItemMarkers ||
         wantsBombConsumers);
    if (!worldSubsystemActive) {
        SetSubsystemUnknown(RuntimeSubsystem::World);
    } else if (!shouldScanWorld) {
        MarkSubsystemHealthy(RuntimeSubsystem::World, nowUs);
    } else if (worldScanAttempted && worldScanOk) {
        MarkSubsystemHealthy(RuntimeSubsystem::World, nowUs);
    } else if (worldScanAttempted) {
        MarkSubsystemDegraded(RuntimeSubsystem::World, nowUs);
    } else if (entityList && ofs.CGameSceneNode_m_vecAbsOrigin > 0) {
        MarkSubsystemDegraded(RuntimeSubsystem::World, nowUs);
    } else {
        SetSubsystemUnknown(RuntimeSubsystem::World);
    }
    uint16_t localWeaponIdResolved = 0;
