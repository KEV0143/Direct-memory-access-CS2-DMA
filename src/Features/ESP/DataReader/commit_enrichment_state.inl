    static int s_lastResolvedBombCarrierSlot = -1;
    static uint64_t s_lastResolvedBombCarrierUs = 0;
    static uint64_t s_bombCarrierCacheResetEpoch = 0;
    const uint64_t bombCarrierEpoch = s_bombEpoch.load(std::memory_order_relaxed);
    if (s_bombCarrierCacheResetEpoch != bombCarrierEpoch) {
        s_bombCarrierCacheResetEpoch = bombCarrierEpoch;
        s_lastResolvedBombCarrierSlot = -1;
        s_lastResolvedBombCarrierUs = 0;
    }

    constexpr int kBombCarrierTeamT = 2;
    auto isResolvedBombCandidate = [&](int idx) -> bool {
        if (idx < 0 || idx >= 64)
            return false;
        if (!pawns[idx] || healths[idx] <= 0 || lifeStates[idx] != 0)
            return false;
        if (teams[idx] != kBombCarrierTeamT)
            return false;
        return true;
    };
    auto hasBombSignal = [&](int idx) -> bool {
        if (!isResolvedBombCandidate(idx))
            return false;
        const uint16_t weaponId = (weaponIds[idx] < 20000u) ? weaponIds[idx] : 0u;
        return bombCarrierBySlot[idx] || inventoryHasBombBySlot[idx] || weaponId == 49u;
    };

    int resolvedBombCarrierSlot = -1;
    const bool bombDroppedResolved =
        !bombPlantedNow &&
        droppedBombPosValid;
    const bool lastCarrierStillFresh =
        s_lastResolvedBombCarrierSlot >= 0 &&
        s_lastResolvedBombCarrierUs > 0 &&
        (nowUs - s_lastResolvedBombCarrierUs) <= 350000;
    if (!bombPlantedNow && !bombDroppedResolved) {
        if (treatWeaponC4OwnerAsCarrier &&
            isResolvedBombCandidate(weaponC4OwnerPlayerIndex)) {
            resolvedBombCarrierSlot = weaponC4OwnerPlayerIndex;
        } else if (lastCarrierStillFresh &&
                   isResolvedBombCandidate(s_lastResolvedBombCarrierSlot) &&
                   hasBombSignal(s_lastResolvedBombCarrierSlot)) {
            resolvedBombCarrierSlot = s_lastResolvedBombCarrierSlot;
        } else {
            int bestScore = -1000000;
            for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
                const int i = playerResolvedSlots[resolvedIdx];
                if (!hasBombSignal(i))
                    continue;

                const uint16_t weaponId = (weaponIds[i] < 20000u) ? weaponIds[i] : 0u;
                int score = 0;
                if (bombCarrierBySlot[i])
                    score += 100;
                if (inventoryHasBombBySlot[i])
                    score += 40;
                if (weaponId == 49u)
                    score += 18;
                if (i == s_lastResolvedBombCarrierSlot)
                    score += 8;

                if (score > bestScore) {
                    bestScore = score;
                    resolvedBombCarrierSlot = i;
                }
            }
        }
    }

    for (int i = 0; i < 64; ++i) {
        const bool isResolvedCarrier = (i == resolvedBombCarrierSlot);
        bombCarrierBySlot[i] = isResolvedCarrier;
        inventoryHasBombBySlot[i] = false;
    }
    if (resolvedBombCarrierSlot >= 0) {
        s_lastResolvedBombCarrierSlot = resolvedBombCarrierSlot;
        s_lastResolvedBombCarrierUs = nowUs;
    } else if (s_lastResolvedBombCarrierUs > 0 && (nowUs - s_lastResolvedBombCarrierUs) > 2500000) {
        s_lastResolvedBombCarrierSlot = -1;
        s_lastResolvedBombCarrierUs = 0;
    }

    static uintptr_t s_cachedCommittedWeaponPawns[64] = {};
    static uint32_t s_cachedCommittedWeaponHandles[64] = {};
    static uintptr_t s_cachedCommittedWeaponEntities[64] = {};
    static uint16_t s_cachedCommittedWeaponIds[64] = {};
    static int s_cachedCommittedAmmoClips[64] = {};
    for (int i = 0; i < 64; ++i) {
        if (s_cachedCommittedWeaponPawns[i] == pawns[i])
            continue;
        s_cachedCommittedWeaponPawns[i] = pawns[i];
        s_cachedCommittedWeaponHandles[i] = 0;
        s_cachedCommittedWeaponEntities[i] = 0;
        s_cachedCommittedWeaponIds[i] = 0;
        s_cachedCommittedAmmoClips[i] = -1;
    }

    auto resolveCommittedWeaponState = [&](int idx, uint16_t liveWeaponId, uint16_t& outWeaponId, int& outAmmoClip) {
        outWeaponId = 0;
        outAmmoClip = ammoClips[idx];
        if (idx < 0 || idx >= 64)
            return;

        const uint32_t activeHandle = activeWeaponHandles[idx];
        const bool activeHandleValid = activeHandle != 0u && activeHandle != 0xFFFFFFFFu;
        const uintptr_t activeEntity = activeWeapons[idx];
        const bool sameWeaponIdentity =
            (activeHandleValid && activeHandle == s_cachedCommittedWeaponHandles[idx]) ||
            (activeEntity != 0 && activeEntity == s_cachedCommittedWeaponEntities[idx]);

        if (liveWeaponId != 0u) {
            outWeaponId = liveWeaponId;
            s_cachedCommittedWeaponHandles[idx] = activeHandleValid ? activeHandle : 0u;
            s_cachedCommittedWeaponEntities[idx] = activeEntity;
            s_cachedCommittedWeaponIds[idx] = liveWeaponId;
            s_cachedCommittedAmmoClips[idx] = outAmmoClip;
            return;
        }

        if (sameWeaponIdentity && s_cachedCommittedWeaponIds[idx] != 0u) {
            outWeaponId = s_cachedCommittedWeaponIds[idx];
            outAmmoClip = s_cachedCommittedAmmoClips[idx];
            return;
        }

        if (!activeHandleValid && activeEntity == 0) {
            s_cachedCommittedWeaponHandles[idx] = 0u;
            s_cachedCommittedWeaponEntities[idx] = 0;
            s_cachedCommittedWeaponIds[idx] = 0u;
            s_cachedCommittedAmmoClips[idx] = -1;
            return;
        }

        s_cachedCommittedWeaponHandles[idx] = activeHandleValid ? activeHandle : 0u;
        s_cachedCommittedWeaponEntities[idx] = activeEntity;
        s_cachedCommittedWeaponIds[idx] = 0u;
        s_cachedCommittedAmmoClips[idx] = -1;
    };

    auto hasPlausibleCommittedBones = [&](int playerSlot) -> bool {
        if (playerSlot < 0 || playerSlot >= 64 || !hasBoneData[playerSlot])
            return false;

        const Vector3& pelvis = allBones[playerSlot][esp::PELVIS];
        const Vector3& chest = allBones[playerSlot][esp::CHEST];
        const Vector3& head = allBones[playerSlot][esp::HEAD];
        if (!isValidWorldPos(pelvis) || !isValidWorldPos(chest) || !isValidWorldPos(head))
            return false;
        if (head.z <= chest.z || chest.z <= pelvis.z)
            return false;

        const Vector3& leftHeel = allBones[playerSlot][esp::FOOT_HEEL_L];
        const Vector3& rightHeel = allBones[playerSlot][esp::FOOT_HEEL_R];
        if (isValidWorldPos(leftHeel) && leftHeel.z > pelvis.z + 24.0f)
            return false;
        if (isValidWorldPos(rightHeel) && rightHeel.z > pelvis.z + 24.0f)
            return false;

        if (isValidWorldPos(positions[playerSlot])) {
            const float pelvisDelta = (pelvis - positions[playerSlot]).Length();
            const float headDelta = (head - positions[playerSlot]).Length();
            if (pelvisDelta > 240.0f && headDelta > 320.0f)
                return false;
        }

        return true;
    };

    auto collectGrenadesForSlot = [&](int idx, auto& grenadeIdsOut, int& grenadeCountOut) {
        grenadeCountOut = 0;
        memset(grenadeIdsOut, 0, sizeof(grenadeIdsOut));
        if (idx < 0 || idx >= 64)
            return;

        auto appendGrenade = [&](uint16_t weaponId) {
            if (weaponId < 43u || weaponId > 48u)
                return;
            if (grenadeCountOut >= esp::PlayerData::kMaxGrenades)
                return;
            grenadeIdsOut[grenadeCountOut++] = weaponId;
        };

        const int inventorySlotCount = getInventorySlotCount(idx);
        for (int slot = 0; slot < inventorySlotCount && grenadeCountOut < esp::PlayerData::kMaxGrenades; ++slot)
            appendGrenade(inventoryWeaponIds[idx][slot]);

        const uint16_t activeWeaponId = (weaponIds[idx] < 20000u) ? weaponIds[idx] : 0u;
        if (activeWeaponId < 43u || activeWeaponId > 48u)
            return;

        const uint32_t activeHandle = activeWeaponHandles[idx];
        const uintptr_t activeEntity = activeWeapons[idx];
        bool activeAlreadyRepresented = false;
        for (int slot = 0; slot < inventorySlotCount; ++slot) {
            if (activeHandle && activeHandle != 0xFFFFFFFFu &&
                inventoryWeaponHandles[idx][slot] == activeHandle) {
                activeAlreadyRepresented = true;
                break;
            }
            if (activeEntity && inventoryWeapons[idx][slot] == activeEntity) {
                activeAlreadyRepresented = true;
                break;
            }
        }
        if (!activeAlreadyRepresented)
            appendGrenade(activeWeaponId);
    };

    bool localHasBombResolved = false;
    int localAmmoClipResolved = s_localAmmoClip;
    bool localHasDefuserResolved = s_localHasDefuser;
    bool localIsDeadResolved = s_localIsDead;
    int localHealthResolved = s_localHealth;
    int localArmorResolved = s_localArmor;
    int localMoneyResolved = s_localMoney;
    char localNameResolved[128] = {};
    memcpy(localNameResolved, s_localName, sizeof(localNameResolved));
    uint16_t localGrenadeIdsResolved[esp::PlayerData::kMaxGrenades] = {};
    int localGrenadeCountResolved = 0;
    const LocalPlayerIndexSource localPlayerIndexSource =
        ResolveLocalPlayerIndexSource(localControllerMaskBit, localMaskBit);
    const int localPlayerIndex = ResolveLocalPlayerIndex(localControllerMaskBit, localMaskBit);
    const bool localPlayerIndexValid = IsValidLocalPlayerIndex(localPlayerIndex);
    const bool localPlayerIndexHasLiveEvidence =
        IsLiveLocalPlayerIndexSource(localPlayerIndexSource);
    ResolveSharedLocalIdentityFromSlot(
        localPlayerIndex,
        localPlayerIndexValid && localPlayerIndexHasLiveEvidence,
        names,
        healths,
        lifeStates,
        armors,
        moneys,
        hasDefuserFlags,
        localNameResolved,
        localIsDeadResolved,
        localHealthResolved,
        localArmorResolved,
        localMoneyResolved,
        localHasDefuserResolved);
    if (localPlayerIndexValid && localPlayerIndexHasLiveEvidence) {
        const uint16_t liveLocalWeaponId =
            (weaponIds[localPlayerIndex] < 20000u) ? weaponIds[localPlayerIndex] : 0;
        uint16_t committedLocalWeaponId = 0;
        int committedLocalAmmoClip = ammoClips[localPlayerIndex];
        resolveCommittedWeaponState(
            localPlayerIndex,
            liveLocalWeaponId,
            committedLocalWeaponId,
            committedLocalAmmoClip);
        localWeaponIdResolved = committedLocalWeaponId;
        localAmmoClipResolved = committedLocalAmmoClip;
        localHasBombResolved = (localPlayerIndex == resolvedBombCarrierSlot);
        collectGrenadesForSlot(localPlayerIndex, localGrenadeIdsResolved, localGrenadeCountResolved);
    } else if (localControllerMaskBit > 0 && localControllerMaskBit <= 64) {
        localHasBombResolved = ((localControllerMaskBit - 1) == resolvedBombCarrierSlot);
    }

    ApplySharedLocalIdentityState(
        localNameResolved,
        localIsDeadResolved,
        localHealthResolved,
        localArmorResolved,
        localMoneyResolved,
        localHasDefuserResolved);
    s_localWeaponId = localWeaponIdResolved;
    s_localAmmoClip = localAmmoClipResolved;
    s_localHasBomb = localHasBombResolved;
    s_localGrenadeCount = localGrenadeCountResolved;
    std::copy(std::begin(localGrenadeIdsResolved), std::end(localGrenadeIdsResolved), std::begin(s_localGrenadeIds));

#include "commit_players_enrichment.inl"
#include "commit_world.inl"
#include "commit_bomb.inl"

    
    {
        int activeCount = 0;
        for (int i = 0; i < 64; ++i) {
            if (!s_players[i].valid)
                continue;
            if (s_localPawn != 0 && s_players[i].pawn == s_localPawn)
                continue;
            if (localPlayerIndexValid && localPlayerIndexHasLiveEvidence && i == localPlayerIndex)
                continue;
            ++activeCount;
        }
        
        
        
        
        
        
        
        
        
        
        
        if (localPlayerIndexValid &&
            localPlayerIndexHasLiveEvidence &&
            !localIsDeadResolved &&
            localHealthResolved > 0)
            ++activeCount;
        s_activePlayerCount.store(activeCount, std::memory_order_relaxed);
        s_highestEntityIdxStat.store(highestEntityIndex, std::memory_order_relaxed);
        s_worldMarkerCountStat.store(s_worldMarkerCount, std::memory_order_relaxed);
        uint32_t bombFlags = 0;
        if (s_bombState.planted)
            bombFlags |= 1u << 0;
        if (s_bombState.ticking)
            bombFlags |= 1u << 1;
        if (s_bombState.beingDefused)
            bombFlags |= 1u << 2;
        if (s_bombState.dropped)
            bombFlags |= 1u << 3;
        if (s_bombState.boundsValid)
            bombFlags |= 1u << 4;
        s_bombDebugFlags.store(bombFlags, std::memory_order_relaxed);
        s_bombDebugSourceFlags.store(s_bombState.sourceFlags, std::memory_order_relaxed);
        s_bombDebugConfidence.store(s_bombState.confidence, std::memory_order_relaxed);
    }

    static bool s_webRadarCacheLive = false;
    if (webRadarDemandActive) {
        s_webRadarCacheLive = true;
        std::lock_guard<std::mutex> lock(s_dataMutex);
        for (int i = 0; i < 64; ++i) {
            const bool isLocalByIndex =
                localPlayerIndexValid &&
                localPlayerIndexHasLiveEvidence &&
                (i == localPlayerIndex);
            const bool isLocalByControllerSlot =
                localControllerMaskBit > 0 &&
                localControllerMaskBit <= 64 &&
                (i == (localControllerMaskBit - 1));
            const bool isLocalByPawn =
                s_localPawn != 0 &&
                (pawns[i] == s_localPawn);
            const bool localControllerPawnHandleValid =
                localControllerPawnHandle != 0u &&
                localControllerPawnHandle != 0xFFFFFFFFu;
            const bool isLocalByHandle =
                localControllerPawnHandleValid &&
                ((pawnHandles[i] & kEntityHandleMask) ==
                 (localControllerPawnHandle & kEntityHandleMask));
            const bool isLocalWebRadarSlot =
                isLocalByIndex ||
                isLocalByControllerSlot ||
                isLocalByPawn ||
                isLocalByHandle;
            if (isLocalWebRadarSlot) {
                s_webRadarPlayers[i] = {};
                continue;
            }

            
            
            
            if (!pawns[i]) {
                if (s_players[i].valid && s_players[i].pawn != 0 &&
                    s_webRadarPlayers[i].valid && s_webRadarPlayers[i].pawn != 0)
                    continue; 
                s_webRadarPlayers[i] = {};
                continue;
            }

            
            
            Vector3 webRadarPos = positions[i];
            const bool haveWebRadarPos = isValidWorldPos(webRadarPos);
            if (!haveWebRadarPos || (teams[i] != 2 && teams[i] != 3)) {
                s_webRadarPlayers[i] = {};
                continue;
            }
            const bool isDead = healths[i] <= 0 || lifeStates[i] != 0;
            esp::PlayerData& wp = s_webRadarPlayers[i];
            wp.valid = true;
            wp.pawn = pawns[i];
            wp.staleFrames = 0;
            wp.health = isDead ? 0 : healths[i];
            wp.armor = std::clamp(armors[i], 0, 100);
            wp.team = teams[i];
            wp.money = std::max(0, moneys[i]);
            wp.ping = static_cast<int>(pings[i]);
            wp.position = webRadarPos;
            wp.velocity = isDead ? Vector3{} : velocities[i];
            wp.scoped = scopedFlags[i] != 0;
            wp.defusing = defusingFlags[i] != 0;
            wp.hasDefuser = hasDefuserFlags[i] != 0;
            wp.flashDuration = flashDurations[i];
            wp.flashed = wp.flashDuration > 0.05f;
            wp.eyeYaw = eyeAnglesPerPlayer[i].y;
            wp.visible = false;
            wp.soundUntilMs = 0;
            wp.spottedMask = 0;
            const uint16_t liveWeaponId = (weaponIds[i] < 20000u) ? weaponIds[i] : 0;
            uint16_t committedWeaponId = 0;
            int committedAmmoClip = ammoClips[i];
            resolveCommittedWeaponState(i, liveWeaponId, committedWeaponId, committedAmmoClip);
            wp.ammoClip = committedAmmoClip;
            wp.weaponId = committedWeaponId;
            wp.hasBomb = (i == resolvedBombCarrierSlot);
            wp.hasBones = hasPlausibleCommittedBones(i);
            collectGrenadesForSlot(i, wp.grenadeIds, wp.grenadeCount);
            memcpy(wp.name, names[i], 128);
            wp.name[127] = '\0';
            if (wp.hasBones) {
                for (int boneIdx = 0; boneIdx < esp::kPlayerStoredBoneCount; ++boneIdx)
                    wp.bones[boneIdx] = allBones[i][esp::kPlayerStoredBoneIds[boneIdx]];
            }
        }
    } else if (s_webRadarCacheLive) {
        s_webRadarCacheLive = false;
        std::lock_guard<std::mutex> lock(s_dataMutex);
        memset(s_webRadarPlayers, 0, sizeof(s_webRadarPlayers));
    }

    if (minimapBoundsValid)
        HandleMapCalibration(minimapMins, minimapMaxs);

    PublishCurrentSnapshot(SnapshotAll);
    s_lastPublishUs.store(TickNowUs(), std::memory_order_relaxed);

    s_requiredReadFailureCount = 0;
    MarkDmaReadSuccess();
