    
    
    
    
    static uintptr_t s_cachedSceneNodes[64] = {};
    static uintptr_t s_cachedBoneArrays[64] = {};
    static uintptr_t s_cachedBonePawns[64] = {};
    static uint64_t s_boneCacheResetSerial = 0;
    static uint64_t s_lastBoneReadsUs = 0;
    if (g::espSkeleton) {
        auto clearResolvedBoneSlot = [&](int i) {
            hasBoneData[i] = false;
            sceneNodes[i] = 0;
            for (int b = 0; b < esp::kPlayerStoredBoneCount; ++b) {
                const int boneIdx = esp::kPlayerStoredBoneIds[b];
                allBones[i][boneIdx] = {};
            }
        };

        {
            const uint64_t boneResetSerial = s_sceneResetSerial.load(std::memory_order_relaxed);
            if (s_boneCacheResetSerial != boneResetSerial) {
                s_boneCacheResetSerial = boneResetSerial;
                s_lastBoneReadsUs = 0;
                memset(s_cachedSceneNodes, 0, sizeof(s_cachedSceneNodes));
                memset(s_cachedBoneArrays, 0, sizeof(s_cachedBoneArrays));
                memset(s_cachedBonePawns, 0, sizeof(s_cachedBonePawns));
            }
        }

        for (int i = 0; i < 64; ++i) {
            if (pawns[i] == s_cachedBonePawns[i])
                continue;
            s_cachedBonePawns[i] = pawns[i];
            s_cachedSceneNodes[i] = 0;
            s_cachedBoneArrays[i] = 0;
        }

        
        
        
        
        
        
        int boneScanSlots[64] = {};
        int boneScanSlotCount = 0;
        const int boneFilterLocalTeam = s_localTeam;
        const bool boneFilterTeamConfirmed =
            localTeamLiveResolved ||
            localControllerTeam == boneFilterLocalTeam;
        const bool filterTeammatesForBones =
            (boneFilterLocalTeam == 2 || boneFilterLocalTeam == 3) &&
            boneFilterTeamConfirmed &&
            !localTeamLikelySwitched;
        for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
            const int i = playerResolvedSlots[resolvedIdx];
            const bool staleTeamDuringSwitch =
                localTeamLikelySwitched &&
                !liveTeamReads[i];
            if (filterTeammatesForBones &&
                teams[i] == boneFilterLocalTeam &&
                !staleTeamDuringSwitch)
                continue;
            boneScanSlots[boneScanSlotCount++] = i;
        }
        if (boneScanSlotCount == 0 &&
            filterTeammatesForBones &&
            playerResolvedSlotCount > 0) {
            
            
            
            
            for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx)
                boneScanSlots[boneScanSlotCount++] = playerResolvedSlots[resolvedIdx];
        }

        const uint64_t boneNowUs = TickNowUs();
        bool boneReadsUrgent = false;
        for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
            const int i = boneScanSlots[scanIdx];
            if (!pawns[i])
                continue;
            if (!s_cachedSceneNodes[i] ||
                !s_cachedBoneArrays[i] ||
                !s_players[i].hasBones) {
                boneReadsUrgent = true;
                break;
            }
        }
        const bool boneReadsDue =
            boneReadsUrgent ||
            s_lastBoneReadsUs == 0 ||
            (boneNowUs - s_lastBoneReadsUs) >= esp::intervals::kBoneReadsUs;
        _boneReadsActiveTick = boneReadsDue;

        if (!boneReadsDue) {
            for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
                const int i = playerResolvedSlots[resolvedIdx];
                clearResolvedBoneSlot(i);
                sceneNodes[i] = s_cachedSceneNodes[i];
            }
        } else if (boneScanSlotCount == 0) {
            
            for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx)
                clearResolvedBoneSlot(playerResolvedSlots[resolvedIdx]);
        } else {
            uintptr_t boneArrays[64] = {};
            int sceneNodeFailures = 0;
            int boneArrayFailures = 0;
            int boneReadFailures = 0;
            bool queuedMergedBoneReads = false;

            memset(sceneNodes, 0, sizeof(sceneNodes));
            for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
                const int i = boneScanSlots[scanIdx];
                if (!pawns[i])
                    continue;
                mem.AddScatterReadRequest(
                    handle,
                    pawns[i] + ofs.C_BaseEntity_m_pGameSceneNode,
                    &sceneNodes[i],
                    sizeof(uintptr_t));
                queuedMergedBoneReads = true;
            }
            for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
                const int i = boneScanSlots[scanIdx];
                if (!s_cachedSceneNodes[i])
                    continue;
                mem.AddScatterReadRequest(
                    handle,
                    s_cachedSceneNodes[i] + ofs.CSkeletonInstance_m_modelState + 0x80,
                    &boneArrays[i],
                    sizeof(uintptr_t));
                queuedMergedBoneReads = true;
            }
            for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
                const int i = boneScanSlots[scanIdx];
                if (!s_cachedBoneArrays[i])
                    continue;
                hasBoneData[i] = true;
                for (int b = 0; b < esp::kPlayerStoredBoneCount; ++b) {
                    const int boneIdx = esp::kPlayerStoredBoneIds[b];
                    mem.AddScatterReadRequest(
                        handle,
                        s_cachedBoneArrays[i] + boneIdx * 32,
                        &allBones[i][boneIdx],
                        sizeof(Vector3));
                    queuedMergedBoneReads = true;
                }
            }

            if (queuedMergedBoneReads && !mem.ExecuteReadScatter(handle)) {
                sceneNodeFailures = 1;
                boneArrayFailures = 1;
                for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
                    const int i = boneScanSlots[scanIdx];
                    sceneNodes[i] = 0;
                    boneArrays[i] = 0;
                    clearResolvedBoneSlot(i);
                }
                logUpdateDataIssue("scatter_16_17", "optional_failed_bone_merged_disabled");
            }

            for (uintptr_t& sceneNode : sceneNodes) {
                if (!isLikelyGamePointer(sceneNode))
                    sceneNode = 0;
            }

            bool sceneNodesChanged = false;
            for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
                const int i = boneScanSlots[scanIdx];
                if (sceneNodes[i] != s_cachedSceneNodes[i]) {
                    sceneNodesChanged = true;
                    break;
                }
            }
            if (sceneNodesChanged) {
                for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx)
                    boneArrays[boneScanSlots[scanIdx]] = 0;
                bool queuedSceneNodeRefresh = false;
                for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
                    const int i = boneScanSlots[scanIdx];
                    if (!sceneNodes[i])
                        continue;
                    mem.AddScatterReadRequest(
                        handle,
                        sceneNodes[i] + ofs.CSkeletonInstance_m_modelState + 0x80,
                        &boneArrays[i],
                        sizeof(uintptr_t));
                    queuedSceneNodeRefresh = true;
                }
                if (queuedSceneNodeRefresh && !mem.ExecuteReadScatter(handle)) {
                    boneArrayFailures = 1;
                    memset(boneArrays, 0, sizeof(boneArrays));
                    logUpdateDataIssue("scatter_16_refresh", "optional_failed_scene_node_bone_array_refresh");
                }
            }

            bool bonesChanged = sceneNodesChanged;
            if (!bonesChanged) {
                for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
                    const int i = boneScanSlots[scanIdx];
                    if (boneArrays[i] != s_cachedBoneArrays[i]) {
                        bonesChanged = true;
                        break;
                    }
                }
            }

            if (bonesChanged) {
                for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx)
                    clearResolvedBoneSlot(boneScanSlots[scanIdx]);
                bool queuedFreshBoneReads = false;
                for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
                    const int i = boneScanSlots[scanIdx];
                    if (!boneArrays[i])
                        continue;
                    hasBoneData[i] = true;
                    for (int b = 0; b < esp::kPlayerStoredBoneCount; ++b) {
                        const int boneIdx = esp::kPlayerStoredBoneIds[b];
                        mem.AddScatterReadRequest(
                            handle,
                            boneArrays[i] + boneIdx * 32,
                            &allBones[i][boneIdx],
                            sizeof(Vector3));
                        queuedFreshBoneReads = true;
                    }
                }
                if (queuedFreshBoneReads && !mem.ExecuteReadScatter(handle)) {
                    boneReadFailures = 1;
                    for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx)
                        clearResolvedBoneSlot(boneScanSlots[scanIdx]);
                    logUpdateDataIssue("scatter_17_refresh", "optional_failed_bone_positions_refresh");
                }
            }

            for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
                const int i = boneScanSlots[scanIdx];
                s_cachedSceneNodes[i] = sceneNodes[i];
                s_cachedBoneArrays[i] = boneArrays[i];
            }
            s_lastBoneReadsUs = boneNowUs;

            if (narrowDebugEnabled(kNarrowDebugBones)) {
                static uint32_t s_bonesDebugCounter = 0;
                const bool emitBonesDebug =
                    sceneNodeFailures > 0 ||
                    boneArrayFailures > 0 ||
                    boneReadFailures > 0 ||
                    narrowDebugTick(kNarrowDebugBones, s_bonesDebugCounter, 40u);
                if (emitBonesDebug) {
                    int sceneNodeCount = 0;
                    int boneArrayCount = 0;
                    int readyCount = 0;
                    for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
                        const int i = boneScanSlots[scanIdx];
                        if (sceneNodes[i])
                            ++sceneNodeCount;
                        if (boneArrays[i])
                            ++boneArrayCount;
                        if (hasBoneData[i])
                            ++readyCount;
                    }
                    DmaLogPrintf(
                        "[DEBUG] Bones: fails(scene/array/read)=%d/%d/%d skeleton=%d sceneNodes=%d boneArrays=%d ready=%d needed=%d",
                        sceneNodeFailures,
                        boneArrayFailures,
                        boneReadFailures,
                        g::espSkeleton ? 1 : 0,
                        sceneNodeCount,
                        boneArrayCount,
                        readyCount,
                        esp::kPlayerStoredBoneCount);

                    int sampleCount = 0;
                    for (int scanIdx = 0; scanIdx < boneScanSlotCount && sampleCount < 3; ++scanIdx) {
                        const int i = boneScanSlots[scanIdx];
                        if (!pawns[i])
                            continue;
                        if (!sceneNodes[i] && !boneArrays[i] && !hasBoneData[i])
                            continue;
                        ++sampleCount;
                        DmaLogPrintf(
                            "[DEBUG] Bones sample: slot=%d pawn=0x%llX scene=0x%llX boneArray=0x%llX hasBones=%d health=%d life=%u",
                            i,
                            static_cast<unsigned long long>(pawns[i]),
                            static_cast<unsigned long long>(sceneNodes[i]),
                            static_cast<unsigned long long>(boneArrays[i]),
                            hasBoneData[i] ? 1 : 0,
                            healths[i],
                            static_cast<unsigned>(lifeStates[i]));
                    }
                }
            }
        }
    } else if (narrowDebugEnabled(kNarrowDebugBones)) {
        static uint32_t s_bonesDisabledDebugCounter = 0;
        if (narrowDebugTick(kNarrowDebugBones, s_bonesDisabledDebugCounter, 120u)) {
            DmaLogPrintf("[DEBUG] Bones: skipped skeleton=0");
        }
    }
