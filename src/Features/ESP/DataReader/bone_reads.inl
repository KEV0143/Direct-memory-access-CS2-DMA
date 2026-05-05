    
    
    
    
    static uintptr_t s_cachedSceneNodes[64] = {};
    static uintptr_t s_cachedBoneArrays[64] = {};
    static uintptr_t s_cachedBonePawns[64] = {};
    static uint64_t s_boneCacheResetSerial = 0;
    static uint64_t s_lastBoneReadsUs = 0;
    static uint8_t s_sceneNodeZeroStreak[64] = {};
    static uint8_t s_boneArrayZeroStreak[64] = {};
    static uint32_t s_perSlotBoneStaleStreak[64] = {};
    static uint8_t s_perSlotBoneStaleEscalation[64] = {};
    static uint64_t s_lastPerSlotBoneLocalResetUs[64] = {};
    static uint64_t s_lastPerSlotBoneRefreshUs = 0;
    if (g::espSkeleton) {
        const uint64_t boneSubsystemNowUs = TickNowUs();
        auto clearBoneCacheSlot = [&](int i) {
            if (i < 0 || i >= 64)
                return;
            s_cachedSceneNodes[i] = 0;
            s_cachedBoneArrays[i] = 0;
            s_sceneNodeZeroStreak[i] = 0;
            s_boneArrayZeroStreak[i] = 0;
        };
        auto clearResolvedBoneSlot = [&](int i) {
            hasBoneData[i] = false;
            sceneNodes[i] = 0;
            for (int b = 0; b < esp::kPlayerStoredBoneCount; ++b) {
                const int boneIdx = esp::kPlayerStoredBoneIds[b];
                allBones[i][boneIdx] = {};
            }
        };
        auto copyCommittedBoneSlot = [&](int i, const esp::PlayerData& committed) -> bool {
            if (i < 0 || i >= 64)
                return false;
            if (!committed.valid ||
                !committed.hasBones ||
                committed.pawn == 0 ||
                committed.pawn != pawns[i]) {
                return false;
            }
            hasBoneData[i] = true;
            sceneNodes[i] = s_cachedSceneNodes[i];
            for (int b = 0; b < esp::kPlayerStoredBoneCount; ++b) {
                const int boneIdx = esp::kPlayerStoredBoneIds[b];
                allBones[i][boneIdx] = committed.bones[b];
            }
            return true;
        };
        auto restoreCommittedBoneSlot = [&](int i) -> bool {
            if (copyCommittedBoneSlot(i, s_players[i]))
                return true;
            return copyCommittedBoneSlot(i, s_prevPlayers[i]);
        };
        auto hasReusableCommittedBoneSlot = [&](int i) -> bool {
            if (i < 0 || i >= 64 || !pawns[i])
                return false;
            const esp::PlayerData& current = s_players[i];
            if (current.valid && current.hasBones && current.pawn == pawns[i])
                return true;
            const esp::PlayerData& previous = s_prevPlayers[i];
            return previous.valid && previous.hasBones && previous.pawn == pawns[i];
        };

        {
            const uint64_t boneResetSerial = s_sceneResetSerial.load(std::memory_order_relaxed);
            if (s_boneCacheResetSerial != boneResetSerial) {
                s_boneCacheResetSerial = boneResetSerial;
                s_lastBoneReadsUs = 0;
                memset(s_cachedSceneNodes, 0, sizeof(s_cachedSceneNodes));
                memset(s_cachedBoneArrays, 0, sizeof(s_cachedBoneArrays));
                memset(s_cachedBonePawns, 0, sizeof(s_cachedBonePawns));
                memset(s_sceneNodeZeroStreak, 0, sizeof(s_sceneNodeZeroStreak));
                memset(s_boneArrayZeroStreak, 0, sizeof(s_boneArrayZeroStreak));
                memset(s_perSlotBoneStaleStreak, 0, sizeof(s_perSlotBoneStaleStreak));
                memset(s_perSlotBoneStaleEscalation, 0, sizeof(s_perSlotBoneStaleEscalation));
                memset(s_lastPerSlotBoneLocalResetUs, 0, sizeof(s_lastPerSlotBoneLocalResetUs));
                s_lastPerSlotBoneRefreshUs = 0;
            }
        }

        for (int i = 0; i < 64; ++i) {
            if (pawns[i] == s_cachedBonePawns[i])
                continue;
            s_cachedBonePawns[i] = pawns[i];
            clearBoneCacheSlot(i);
            s_perSlotBoneStaleStreak[i] = 0;
            s_perSlotBoneStaleEscalation[i] = 0;
            s_lastPerSlotBoneLocalResetUs[i] = 0;
        }

        for (int i = 0; i < 64; ++i) {
            if (!pawns[i] ||
                !s_players[i].valid ||
                s_players[i].pawn != pawns[i]) {
                continue;
            }

            const bool respawnedSamePawn =
                s_players[i].health <= 0 &&
                healths[i] > 0 &&
                lifeStates[i] == 0;
            const bool teleportedSamePawn =
                s_players[i].health > 0 &&
                healths[i] > 0 &&
                lifeStates[i] == 0 &&
                isValidWorldPos(s_players[i].position) &&
                isValidWorldPos(positions[i]) &&
                std::hypot(
                    positions[i].x - s_players[i].position.x,
                    positions[i].y - s_players[i].position.y) >= 512.0f;
            if (respawnedSamePawn || teleportedSamePawn) {
                clearBoneCacheSlot(i);
                s_perSlotBoneStaleStreak[i] = 0;
                s_perSlotBoneStaleEscalation[i] = 0;
                s_lastPerSlotBoneLocalResetUs[i] = 0;
                clearResolvedBoneSlot(i);
            }
        }

        int boneScanSlots[64] = {};
        int boneScanSlotCount = 0;
        const int boneFilterLocalTeam = s_localTeam;
        const bool boneFilterTeamConfirmed =
            localTeamLiveResolved ||
            localControllerTeam == boneFilterLocalTeam;
        const bool filterTeammatesForBones =
            !g::espShowTeammates &&
            (boneFilterLocalTeam == 2 || boneFilterLocalTeam == 3) &&
            boneFilterTeamConfirmed &&
            !localTeamLikelySwitched;
        for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
            const int i = playerResolvedSlots[resolvedIdx];
            const bool liveCoreForBones =
                pawns[i] != 0 &&
                healths[i] > 0 &&
                lifeStates[i] == 0;
            const bool cachedLiveForBones =
                s_players[i].valid &&
                s_players[i].pawn == pawns[i] &&
                s_players[i].health > 0;
            if (!liveCoreForBones && !cachedLiveForBones) {
                s_perSlotBoneStaleStreak[i] = 0;
                s_perSlotBoneStaleEscalation[i] = 0;
                continue;
            }
            const bool staleTeamDuringSwitch =
                localTeamLikelySwitched &&
                !liveTeamReads[i];
            const bool liveConfirmedTeammate =
                liveTeamReads[i] &&
                teams[i] == boneFilterLocalTeam;
            const bool committedWouldRenderAsEnemy =
                s_players[i].valid &&
                s_players[i].pawn == pawns[i] &&
                (boneFilterLocalTeam != 2 && boneFilterLocalTeam != 3 ||
                 s_players[i].team != boneFilterLocalTeam);
            if (filterTeammatesForBones &&
                liveConfirmedTeammate &&
                !staleTeamDuringSwitch &&
                !committedWouldRenderAsEnemy) {
                continue;
            }
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
                !hasReusableCommittedBoneSlot(i)) {
                boneReadsUrgent = true;
                break;
            }
        }
        constexpr uint64_t kBoneUrgentRetryUs = 12000;
        const bool boneReadsDue =
            s_lastBoneReadsUs == 0 ||
            (boneReadsUrgent && (boneNowUs - s_lastBoneReadsUs) >= kBoneUrgentRetryUs) ||
            (boneNowUs - s_lastBoneReadsUs) >= esp::intervals::kBoneReadsUs;
        _boneReadsActiveTick = boneReadsDue;

        if (boneScanSlotCount == 0) {
            for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx)
                clearResolvedBoneSlot(playerResolvedSlots[resolvedIdx]);
            SetSubsystemUnknown(RuntimeSubsystem::Bones);
        } else if (!boneReadsDue) {
            for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
                const int i = playerResolvedSlots[resolvedIdx];
                if (!restoreCommittedBoneSlot(i)) {
                    clearResolvedBoneSlot(i);
                    sceneNodes[i] = s_cachedSceneNodes[i];
                }
            }
            MarkSubsystemHealthy(RuntimeSubsystem::Bones, boneSubsystemNowUs);
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
                    if (!restoreCommittedBoneSlot(i)) {
                        sceneNodes[i] = 0;
                        boneArrays[i] = 0;
                        clearResolvedBoneSlot(i);
                    }
                }
                logUpdateDataIssue("scatter_16_17", "optional_failed_bone_merged_disabled");
            }

            for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
                const int i = boneScanSlots[scanIdx];
                if (!isLikelyGamePointer(sceneNodes[i]))
                    sceneNodes[i] = 0;
                if (!sceneNodes[i] && s_cachedSceneNodes[i] && hasReusableCommittedBoneSlot(i)) {
                    if (s_sceneNodeZeroStreak[i] < 0xFFu)
                        ++s_sceneNodeZeroStreak[i];
                    if (s_sceneNodeZeroStreak[i] <= 4u)
                        sceneNodes[i] = s_cachedSceneNodes[i];
                } else if (sceneNodes[i]) {
                    s_sceneNodeZeroStreak[i] = 0;
                }
            }

            bool sceneNodeDirty[64] = {};
            bool sceneNodesChanged = false;
            for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
                const int i = boneScanSlots[scanIdx];
                if (sceneNodes[i] != s_cachedSceneNodes[i]) {
                    sceneNodeDirty[i] = true;
                    sceneNodesChanged = true;
                }
            }
            if (sceneNodesChanged) {
                for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
                    const int i = boneScanSlots[scanIdx];
                    if (sceneNodeDirty[i])
                        boneArrays[i] = 0;
                }
                bool queuedSceneNodeRefresh = false;
                for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
                    const int i = boneScanSlots[scanIdx];
                    if (!sceneNodeDirty[i] || !sceneNodes[i])
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
                    for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
                        const int i = boneScanSlots[scanIdx];
                        if (sceneNodeDirty[i])
                            boneArrays[i] = 0;
                    }
                    logUpdateDataIssue("scatter_16_refresh", "optional_failed_scene_node_bone_array_refresh");
                }
            }

            for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
                const int i = boneScanSlots[scanIdx];
                if (!isLikelyGamePointer(boneArrays[i]))
                    boneArrays[i] = 0;
                if (!boneArrays[i] &&
                    s_cachedBoneArrays[i] &&
                    !sceneNodeDirty[i] &&
                    hasReusableCommittedBoneSlot(i)) {
                    if (s_boneArrayZeroStreak[i] < 0xFFu)
                        ++s_boneArrayZeroStreak[i];
                    if (s_boneArrayZeroStreak[i] <= 4u)
                        boneArrays[i] = s_cachedBoneArrays[i];
                } else if (boneArrays[i]) {
                    s_boneArrayZeroStreak[i] = 0;
                }
            }

            bool bonesDirty[64] = {};
            bool bonesChanged = false;
            for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
                const int i = boneScanSlots[scanIdx];
                if (sceneNodeDirty[i] || boneArrays[i] != s_cachedBoneArrays[i]) {
                    bonesDirty[i] = true;
                    bonesChanged = true;
                }
            }

            if (bonesChanged) {
                for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
                    const int i = boneScanSlots[scanIdx];
                    if (bonesDirty[i])
                        clearResolvedBoneSlot(i);
                }
                bool queuedFreshBoneReads = false;
                for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
                    const int i = boneScanSlots[scanIdx];
                    if (!bonesDirty[i] || !boneArrays[i])
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
                    for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
                        const int i = boneScanSlots[scanIdx];
                        if (!bonesDirty[i])
                            continue;
                        if (!restoreCommittedBoneSlot(i))
                            clearResolvedBoneSlot(i);
                    }
                    logUpdateDataIssue("scatter_17_refresh", "optional_failed_bone_positions_refresh");
                }
            }

            for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
                const int i = boneScanSlots[scanIdx];
                if (!hasBoneData[i])
                    restoreCommittedBoneSlot(i);
            }

            for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
                const int i = boneScanSlots[scanIdx];
                s_cachedSceneNodes[i] = sceneNodes[i];
                s_cachedBoneArrays[i] = boneArrays[i];
            }
            s_lastBoneReadsUs = boneNowUs;

            bool perSlotStaleDetected = false;
            bool perSlotNeedsProbe = false;
            constexpr uint32_t kPerSlotBoneLocalResetFrames = 30u;
            constexpr uint8_t kPerSlotBoneProbeEscalations = 8u;
            constexpr uint64_t kPerSlotBoneLocalResetCooldownUs = 90000u;
            constexpr uint64_t kPerSlotBoneProbeCooldownUs = 12000000u;
            for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
                const int i = boneScanSlots[scanIdx];
                const bool liveCoreForBones =
                    pawns[i] != 0 &&
                    healths[i] > 0 &&
                    lifeStates[i] == 0;
                const bool cachedLiveForBones =
                    s_players[i].valid &&
                    s_players[i].pawn == pawns[i] &&
                    s_players[i].health > 0;
                if (!liveCoreForBones && !cachedLiveForBones) {
                    s_perSlotBoneStaleStreak[i] = 0;
                    s_perSlotBoneStaleEscalation[i] = 0;
                    continue;
                }
                bool slotLooksStale = false;
                if (sceneNodes[i] == 0 || boneArrays[i] == 0) {
                    slotLooksStale = true;
                } else if (hasBoneData[i]) {
                    bool anyNonZero = false;
                    for (int b = 0; b < esp::kPlayerStoredBoneCount; ++b) {
                        const int boneIdx = esp::kPlayerStoredBoneIds[b];
                        const Vector3& v = allBones[i][boneIdx];
                        if (std::fabs(v.x) > 0.5f || std::fabs(v.y) > 0.5f || std::fabs(v.z) > 0.5f) {
                            anyNonZero = true;
                            break;
                        }
                    }
                    if (!anyNonZero)
                        slotLooksStale = true;
                } else {
                    slotLooksStale = true;
                }
                if (slotLooksStale) {
                    if (s_perSlotBoneStaleStreak[i] < 0xFFFFFFFFu)
                        ++s_perSlotBoneStaleStreak[i];
                    if (s_perSlotBoneStaleStreak[i] >= kPerSlotBoneLocalResetFrames)
                        perSlotStaleDetected = true;
                } else {
                    s_perSlotBoneStaleStreak[i] = 0;
                    s_perSlotBoneStaleEscalation[i] = 0;
                    s_lastPerSlotBoneLocalResetUs[i] = 0;
                }
            }
            if (perSlotStaleDetected) {
                for (int i = 0; i < 64; ++i) {
                    if (s_perSlotBoneStaleStreak[i] < kPerSlotBoneLocalResetFrames)
                        continue;

                    const bool localResetCooldownElapsed =
                        s_lastPerSlotBoneLocalResetUs[i] == 0 ||
                        boneNowUs <= s_lastPerSlotBoneLocalResetUs[i] ||
                        (boneNowUs - s_lastPerSlotBoneLocalResetUs[i]) >= kPerSlotBoneLocalResetCooldownUs;
                    if (!localResetCooldownElapsed)
                        continue;

                    s_lastPerSlotBoneLocalResetUs[i] = boneNowUs;
                    clearBoneCacheSlot(i);
                    clearResolvedBoneSlot(i);
                    s_perSlotBoneStaleStreak[i] = 0;
                    if (s_perSlotBoneStaleEscalation[i] < 0xFFu)
                        ++s_perSlotBoneStaleEscalation[i];
                    if (s_perSlotBoneStaleEscalation[i] >= kPerSlotBoneProbeEscalations)
                        perSlotNeedsProbe = true;
                }

                if (perSlotNeedsProbe) {
                    const bool perSlotProbeCooldownElapsed =
                        s_lastPerSlotBoneRefreshUs == 0 ||
                        boneNowUs <= s_lastPerSlotBoneRefreshUs ||
                        (boneNowUs - s_lastPerSlotBoneRefreshUs) >= kPerSlotBoneProbeCooldownUs;
                    if (perSlotProbeCooldownElapsed) {
                        s_lastPerSlotBoneRefreshUs = boneNowUs;
                        refreshDmaCaches("per_slot_bone_stale_probe", DmaRefreshTier::Probe, false);
                    }
                }
            }

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
                }
            }

            if (sceneNodeFailures > 0 || boneArrayFailures > 0 || boneReadFailures > 0) {
                const bool anyCachedBones = [&]() -> bool {
                    for (int scanIdx = 0; scanIdx < boneScanSlotCount; ++scanIdx) {
                        const int i = boneScanSlots[scanIdx];
                        if (s_cachedBoneArrays[i] != 0 || hasReusableCommittedBoneSlot(i))
                            return true;
                    }
                    return false;
                }();
                if (anyCachedBones)
                    MarkSubsystemDegraded(RuntimeSubsystem::Bones, boneSubsystemNowUs);
                else
                    MarkSubsystemFailed(RuntimeSubsystem::Bones, boneSubsystemNowUs);
            } else {
                MarkSubsystemHealthy(RuntimeSubsystem::Bones, boneSubsystemNowUs);
            }
        }
    } else if (narrowDebugEnabled(kNarrowDebugBones)) {
        static uint32_t s_bonesDisabledDebugCounter = 0;
        if (narrowDebugTick(kNarrowDebugBones, s_bonesDisabledDebugCounter, 120u)) {
            DmaLogPrintf("[DEBUG] Bones: skipped skeleton=0");
        }
        SetSubsystemUnknown(RuntimeSubsystem::Bones);
    } else {
        SetSubsystemUnknown(RuntimeSubsystem::Bones);
    }
