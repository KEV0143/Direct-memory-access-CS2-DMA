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
        return bombCarrierBySlot[idx] || inventoryHasBombBySlot[idx] || weaponId == kWeaponC4Id;
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
            if (weaponId == kWeaponC4Id)
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

    static uintptr_t s_lastGoodBonePawns[64] = {};
    static uint64_t s_lastGoodBoneUs[64] = {};
    static Vector3 s_lastGoodBones[64][esp::kPlayerStoredBoneCount] = {};

    auto getStoredBoneCompactIndex = [&](int boneId) -> int {
        for (int storedIdx = 0; storedIdx < esp::kPlayerStoredBoneCount; ++storedIdx) {
            if (esp::kPlayerStoredBoneIds[storedIdx] == boneId)
                return storedIdx;
        }
        return -1;
    };

    auto storedBoneLooksPlausible = [&](const Vector3* storedBones, int playerSlot) -> bool {
        if (!storedBones || playerSlot < 0 || playerSlot >= 64)
            return false;

        const int pelvisIdx = getStoredBoneCompactIndex(esp::PELVIS);
        const int chestIdx = getStoredBoneCompactIndex(esp::CHEST);
        const int spine2Idx = getStoredBoneCompactIndex(esp::SPINE2);
        const int headIdx = getStoredBoneCompactIndex(esp::HEAD);
        const int leftHeelIdx = getStoredBoneCompactIndex(esp::FOOT_HEEL_L);
        const int rightHeelIdx = getStoredBoneCompactIndex(esp::FOOT_HEEL_R);
        if (pelvisIdx < 0 || headIdx < 0)
            return false;

        const Vector3& pelvis = storedBones[pelvisIdx];
        const Vector3& head = storedBones[headIdx];
        if (!isValidWorldPos(pelvis) || !isValidWorldPos(head))
            return false;
        if (head.z <= pelvis.z)
            return false;

        const bool hasChest = chestIdx >= 0 && isValidWorldPos(storedBones[chestIdx]);
        const bool hasSpine2 = spine2Idx >= 0 && isValidWorldPos(storedBones[spine2Idx]);
        if (hasChest) {
            const Vector3& chest = storedBones[chestIdx];
            if (head.z <= chest.z || chest.z <= pelvis.z)
                return false;
        } else if (hasSpine2) {
            const Vector3& spine2 = storedBones[spine2Idx];
            if (head.z <= spine2.z || spine2.z <= pelvis.z)
                return false;
        }

        if (leftHeelIdx >= 0) {
            const Vector3& leftHeel = storedBones[leftHeelIdx];
            if (isValidWorldPos(leftHeel) && leftHeel.z > pelvis.z + 24.0f)
                return false;
        }
        if (rightHeelIdx >= 0) {
            const Vector3& rightHeel = storedBones[rightHeelIdx];
            if (isValidWorldPos(rightHeel) && rightHeel.z > pelvis.z + 24.0f)
                return false;
        }

        if (isValidWorldPos(positions[playerSlot])) {
            const float pelvisDelta = (pelvis - positions[playerSlot]).Length();
            const float headDelta = (head - positions[playerSlot]).Length();
            if (pelvisDelta > 240.0f && headDelta > 320.0f)
                return false;
        }

        return true;
    };

    auto hasPlausibleCommittedBones = [&](int playerSlot) -> bool {
        if (playerSlot < 0 || playerSlot >= 64 || !hasBoneData[playerSlot])
            return false;

        Vector3 currentStoredBones[esp::kPlayerStoredBoneCount] = {};
        for (int storedIdx = 0; storedIdx < esp::kPlayerStoredBoneCount; ++storedIdx)
            currentStoredBones[storedIdx] = allBones[playerSlot][esp::kPlayerStoredBoneIds[storedIdx]];

        return storedBoneLooksPlausible(currentStoredBones, playerSlot);
    };

    auto rememberGoodCommittedBones = [&](int playerSlot, const Vector3* bones) {
        if (playerSlot < 0 || playerSlot >= 64 || !bones || !pawns[playerSlot])
            return;
        s_lastGoodBonePawns[playerSlot] = pawns[playerSlot];
        s_lastGoodBoneUs[playerSlot] = nowUs;
        for (int boneIdx = 0; boneIdx < esp::kPlayerStoredBoneCount; ++boneIdx)
            s_lastGoodBones[playerSlot][boneIdx] = bones[boneIdx];
    };

    constexpr uint64_t kCommittedBoneHoldUs = 850000;
    constexpr uint64_t kLastGoodBoneHoldUs = 1800000;
    auto canReusePreviousCommittedBones = [&](int playerSlot) -> bool {
        if (playerSlot < 0 || playerSlot >= 64)
            return false;
        if (pawns[playerSlot] == 0)
            return false;
        if (s_prevCaptureTimeUs == 0 || nowUs <= s_prevCaptureTimeUs)
            return false;
        if ((nowUs - s_prevCaptureTimeUs) > kCommittedBoneHoldUs)
            return false;

        const esp::PlayerData& prevPlayer = s_prevPlayers[playerSlot];
        if (!prevPlayer.valid || !prevPlayer.hasBones || prevPlayer.pawn != pawns[playerSlot])
            return false;

        return storedBoneLooksPlausible(prevPlayer.bones, playerSlot);
    };

    auto canReuseLastGoodCommittedBones = [&](int playerSlot) -> bool {
        if (playerSlot < 0 || playerSlot >= 64 || !pawns[playerSlot])
            return false;
        if (s_lastGoodBonePawns[playerSlot] != pawns[playerSlot] ||
            s_lastGoodBoneUs[playerSlot] == 0 ||
            nowUs <= s_lastGoodBoneUs[playerSlot] ||
            (nowUs - s_lastGoodBoneUs[playerSlot]) > kLastGoodBoneHoldUs) {
            return false;
        }
        return storedBoneLooksPlausible(s_lastGoodBones[playerSlot], playerSlot);
    };

    auto copyResolvedBones = [&](int playerSlot, esp::PlayerData& targetPlayer) -> bool {
        if (hasPlausibleCommittedBones(playerSlot)) {
            targetPlayer.hasBones = true;
            for (int boneIdx = 0; boneIdx < esp::kPlayerStoredBoneCount; ++boneIdx)
                targetPlayer.bones[boneIdx] = allBones[playerSlot][esp::kPlayerStoredBoneIds[boneIdx]];
            rememberGoodCommittedBones(playerSlot, targetPlayer.bones);
            return true;
        }

        if (canReusePreviousCommittedBones(playerSlot)) {
            targetPlayer.hasBones = true;
            for (int boneIdx = 0; boneIdx < esp::kPlayerStoredBoneCount; ++boneIdx)
                targetPlayer.bones[boneIdx] = s_prevPlayers[playerSlot].bones[boneIdx];
            rememberGoodCommittedBones(playerSlot, targetPlayer.bones);
            return true;
        }

        if (canReuseLastGoodCommittedBones(playerSlot)) {
            targetPlayer.hasBones = true;
            for (int boneIdx = 0; boneIdx < esp::kPlayerStoredBoneCount; ++boneIdx)
                targetPlayer.bones[boneIdx] = s_lastGoodBones[playerSlot][boneIdx];
            return true;
        }

        targetPlayer.hasBones = false;
        return false;
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
        int resolvedLiveCount = 0;
        int committedLiveCount = 0;
        bool activeCountedSlots[64] = {};
        const bool localAliveEvidence =
            localPlayerIndexValid &&
            localPlayerIndexHasLiveEvidence &&
            !localIsDeadResolved &&
            localHealthResolved > 0;
        const bool localControllerPawnHandleValidForCount =
            localControllerPawnHandle != 0u &&
            localControllerPawnHandle != 0xFFFFFFFFu;
        const uint32_t localControllerPawnSlotForCount =
            localControllerPawnHandleValidForCount
            ? (localControllerPawnHandle & kEntityHandleMask)
            : 0u;
        auto isFreshLiveCoreSlot = [&](int i) -> bool {
            if (i < 0 || i >= 64 || !pawns[i])
                return false;
            if (!coreReadFresh[i] || !coreReadAlive[i])
                return false;
            if (teams[i] != 2 && teams[i] != 3)
                return false;
            return isValidWorldPos(positions[i]);
        };
        auto isLocalActiveSlot = [&](int i) -> bool {
            if (i < 0 || i >= 64)
                return false;
            if (s_localPawn != 0 && pawns[i] == s_localPawn)
                return true;
            if (localPlayerIndexValid && localPlayerIndexHasLiveEvidence && i == localPlayerIndex)
                return true;
            if (localControllerMaskBit > 0 && localControllerMaskBit <= 64 && i == (localControllerMaskBit - 1))
                return true;
            const uint32_t pawnSlot = pawnHandles[i] & kEntityHandleMask;
            return localControllerPawnSlotForCount != 0u &&
                   pawnSlot != 0u &&
                   pawnSlot == localControllerPawnSlotForCount;
        };
        for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
            const int i = playerResolvedSlots[resolvedIdx];
            if (!isFreshLiveCoreSlot(i))
                continue;
            ++resolvedLiveCount;
            if (!activeCountedSlots[i]) {
                activeCountedSlots[i] = true;
                ++activeCount;
            }
        }
        bool localAlreadyCounted = false;
        for (int i = 0; i < 64; ++i) {
            if (activeCountedSlots[i] && isLocalActiveSlot(i)) {
                localAlreadyCounted = true;
                break;
            }
        }
        if (localAliveEvidence && !localAlreadyCounted)
            ++activeCount;
        for (int i = 0; i < 64; ++i) {
            if (!s_players[i].valid || s_players[i].health <= 0)
                continue;
            if (s_players[i].team != 2 && s_players[i].team != 3)
                continue;
            if (!isValidWorldPos(s_players[i].position))
                continue;
            ++committedLiveCount;
            if (activeCountedSlots[i] || isLocalActiveSlot(i))
                continue;
            activeCountedSlots[i] = true;
            ++activeCount;
        }

        {
            static uint64_t s_populationWatchdogResetSerial = 0;
            static uint32_t s_populationWatchdogStreak = 0;
            static uint64_t s_populationWatchdogSinceUs = 0;
            static uint64_t s_lastPopulationWatchdogRefreshUs = 0;
            static uint64_t s_lastPopulationWatchdogHardUs = 0;
            static int s_expectedActivePopulation = 0;
            static uint64_t s_expectedActivePopulationUs = 0;
            static uint64_t s_launchUnderresolvedSinceUs = 0;
            static uint64_t s_lastLaunchUnderresolvedRefreshUs = 0;
            const uint64_t watchdogResetSerial = s_sceneResetSerial.load(std::memory_order_relaxed);
            if (s_populationWatchdogResetSerial != watchdogResetSerial) {
                s_populationWatchdogResetSerial = watchdogResetSerial;
                s_populationWatchdogStreak = 0;
                s_populationWatchdogSinceUs = 0;
                s_lastPopulationWatchdogRefreshUs = 0;
                s_lastPopulationWatchdogHardUs = 0;
                s_expectedActivePopulation = 0;
                s_expectedActivePopulationUs = 0;
                s_launchUnderresolvedSinceUs = 0;
                s_lastLaunchUnderresolvedRefreshUs = 0;
            }

            const bool liveMatchContext =
                s_engineInGame.load(std::memory_order_relaxed) &&
                !s_engineMenu.load(std::memory_order_relaxed) &&
                s_engineSignOnState.load(std::memory_order_relaxed) == 6;
            const auto populationWarmupState =
                static_cast<esp::SceneWarmupState>(s_sceneWarmupState.load(std::memory_order_relaxed));
            const uint64_t populationLastResetUs = s_lastSceneResetUs.load(std::memory_order_relaxed);
            const uint64_t populationWarmupEnteredUs = s_sceneWarmupEnteredUs.load(std::memory_order_relaxed);
            const uint64_t populationResetAgeUs =
                populationLastResetUs > 0 && nowUs >= populationLastResetUs
                ? nowUs - populationLastResetUs
                : 0;
            const uint64_t populationWarmupAgeUs =
                populationWarmupEnteredUs > 0 && nowUs >= populationWarmupEnteredUs
                ? nowUs - populationWarmupEnteredUs
                : 0;
            const bool populationGraceElapsed =
                populationResetAgeUs >= 2500000u &&
                populationWarmupAgeUs >= 1800000u;
            const int observedPopulation = std::max(activeCount, resolvedLiveCount);
            const bool healthyPopulationSample =
                liveMatchContext &&
                populationWarmupState == esp::SceneWarmupState::Stable &&
                observedPopulation >= 2 &&
                observedPopulation <= 64 &&
                playerResolvedSlotCount >= 2;
            if (healthyPopulationSample) {
                if (observedPopulation > s_expectedActivePopulation ||
                    s_expectedActivePopulation == 0 ||
                    (s_expectedActivePopulationUs > 0 &&
                     nowUs >= s_expectedActivePopulationUs &&
                     (nowUs - s_expectedActivePopulationUs) > 60000000u)) {
                    s_expectedActivePopulation = observedPopulation;
                    s_expectedActivePopulationUs = nowUs;
                }
            }

            const bool launchUnderresolved =
                liveMatchContext &&
                localAliveEvidence &&
                populationResetAgeUs >= 800000u &&
                populationResetAgeUs <= 12000000u &&
                highestEntityIndex >= 64 &&
                observedPopulation <= 1 &&
                resolvedLiveCount <= 1;
            if (launchUnderresolved) {
                if (s_launchUnderresolvedSinceUs == 0)
                    s_launchUnderresolvedSinceUs = nowUs;
                const uint64_t underresolvedAgeUs =
                    nowUs >= s_launchUnderresolvedSinceUs
                    ? nowUs - s_launchUnderresolvedSinceUs
                    : 0;
                if (populationWarmupState == esp::SceneWarmupState::Stable)
                    setSceneWarmupState(esp::SceneWarmupState::HierarchyWarming);
                const bool launchRefreshCooldownElapsed =
                    s_lastLaunchUnderresolvedRefreshUs == 0 ||
                    nowUs <= s_lastLaunchUnderresolvedRefreshUs ||
                    (nowUs - s_lastLaunchUnderresolvedRefreshUs) >= 900000u;
                if (launchRefreshCooldownElapsed && underresolvedAgeUs >= 600000u) {
                    s_lastLaunchUnderresolvedRefreshUs = nowUs;
                    const bool repairLaunch =
                        underresolvedAgeUs >= 2200000u ||
                        playerResolvedSlotCount <= 1;
                    if (repairLaunch && underresolvedAgeUs >= 3200000u)
                        BumpSceneReset(nowUs);
                    setSceneWarmupState(esp::SceneWarmupState::HierarchyWarming);
                    refreshDmaCaches(
                        repairLaunch ? "launch_underresolved_players_repair" : "launch_underresolved_players_probe",
                        repairLaunch ? DmaRefreshTier::Repair : DmaRefreshTier::Probe,
                        false);
                }
            } else {
                s_launchUnderresolvedSinceUs = 0;
                s_lastLaunchUnderresolvedRefreshUs = 0;
            }

            const bool watchdogEligible =
                liveMatchContext &&
                populationGraceElapsed &&
                highestEntityIndex >= 64 &&
                !launchUnderresolved &&
                !s_dmaRecovering.load(std::memory_order_relaxed) &&
                !s_dmaRecoveryRequested.load(std::memory_order_relaxed);
            const bool hierarchyBlackout =
                watchdogEligible &&
                localAliveEvidence &&
                playerResolvedSlotCount == 0;
            const bool coreBlackout =
                watchdogEligible &&
                localAliveEvidence &&
                playerResolvedSlotCount >= 2 &&
                resolvedLiveCount == 0;
            const bool activeBlackout =
                watchdogEligible &&
                localAliveEvidence &&
                activeCount == 0;
            const bool activePopulationCollapsed =
                watchdogEligible &&
                localAliveEvidence &&
                s_expectedActivePopulation >= 4 &&
                activeCount > 0 &&
                activeCount <= std::max(1, s_expectedActivePopulation / 4) &&
                resolvedLiveCount <= std::max(1, s_expectedActivePopulation / 4);
            const bool staleCommittedPopulation =
                watchdogEligible &&
                resolvedLiveCount >= 2 &&
                committedLiveCount >= 6 &&
                committedLiveCount > resolvedLiveCount + 2;
            const bool populationBroken =
                hierarchyBlackout ||
                coreBlackout ||
                activeBlackout ||
                activePopulationCollapsed ||
                staleCommittedPopulation;

            if (populationBroken) {
                if (s_populationWatchdogSinceUs == 0)
                    s_populationWatchdogSinceUs = nowUs;
                if (s_populationWatchdogStreak < 0xFFFFFFFFu)
                    ++s_populationWatchdogStreak;

                const uint64_t brokenAgeUs =
                    nowUs >= s_populationWatchdogSinceUs
                    ? nowUs - s_populationWatchdogSinceUs
                    : 0;
                const bool refreshCooldownElapsed =
                    s_lastPopulationWatchdogRefreshUs == 0 ||
                    nowUs <= s_lastPopulationWatchdogRefreshUs ||
                    (nowUs - s_lastPopulationWatchdogRefreshUs) >= 2500000u;
                if (refreshCooldownElapsed &&
                    s_populationWatchdogStreak >= 8u &&
                    brokenAgeUs >= 1200000u) {
                    s_lastPopulationWatchdogRefreshUs = nowUs;
                    const bool hardRefresh = brokenAgeUs >= 5500000u || s_populationWatchdogStreak >= 90u;
                    const bool repairRefresh = hardRefresh || brokenAgeUs >= 2500000u || s_populationWatchdogStreak >= 30u;
                    const char* reason =
                        hierarchyBlackout ? "population_watchdog_hierarchy_blackout" :
                        coreBlackout ? "population_watchdog_core_blackout" :
                        activeBlackout ? "population_watchdog_active_blackout" :
                        activePopulationCollapsed ? "population_watchdog_active_collapse" :
                        "population_watchdog_stale_committed_players";
                    if (staleCommittedPopulation && brokenAgeUs >= 1500000u) {
                        clearAllState(true);
                        activeCount = 0;
                        resolvedLiveCount = 0;
                    } else if (repairRefresh) {
                        BumpSceneReset(nowUs);
                    }
                    setSceneWarmupState(esp::SceneWarmupState::Recovery);
                    refreshDmaCaches(
                        reason,
                        hardRefresh ? DmaRefreshTier::Full : repairRefresh ? DmaRefreshTier::Repair : DmaRefreshTier::Probe,
                        hardRefresh);
                    if (hardRefresh) {
                        const bool hardCooldownElapsed =
                            s_lastPopulationWatchdogHardUs == 0 ||
                            nowUs <= s_lastPopulationWatchdogHardUs ||
                            (nowUs - s_lastPopulationWatchdogHardUs) >= 8000000u;
                        if (hardCooldownElapsed) {
                            s_lastPopulationWatchdogHardUs = nowUs;
                            RequestDmaRecovery(reason);
                        }
                    }
                }
            } else {
                s_populationWatchdogStreak = 0;
                s_populationWatchdogSinceUs = 0;
            }
        }

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
                if (s_webRadarPlayers[i].valid &&
                    s_webRadarPlayers[i].pawn != 0 &&
                    s_webRadarPlayers[i].staleFrames < 300) {
                    ++s_webRadarPlayers[i].staleFrames;
                    continue;
                }
                s_webRadarPlayers[i] = {};
                continue;
            }

            Vector3 webRadarPos = positions[i];
            bool haveWebRadarPos = isValidWorldPos(webRadarPos);
            const bool teamValid = (teams[i] == 2 || teams[i] == 3);
            const bool coreReliable = coreReadFresh[i] && coreReadPlausible[i];
            const bool isDead = healths[i] <= 0 || lifeStates[i] != 0;
            const bool cachedSamePawn =
                s_webRadarPlayers[i].valid &&
                s_webRadarPlayers[i].pawn == pawns[i];
            if (!haveWebRadarPos && cachedSamePawn && isValidWorldPos(s_webRadarPlayers[i].position)) {
                webRadarPos = s_webRadarPlayers[i].position;
                haveWebRadarPos = true;
            }
            if ((!coreReliable || !teamValid || !haveWebRadarPos) && cachedSamePawn) {
                esp::PlayerData& cached = s_webRadarPlayers[i];
                if (cached.staleFrames < 300) {
                    ++cached.staleFrames;
                    if (isDead) {
                        cached.health = 0;
                        cached.velocity = {};
                        cached.hasBomb = false;
                        cached.hasBones = false;
                    }
                    continue;
                }
            }
            if (!coreReliable || !haveWebRadarPos || !teamValid) {
                s_webRadarPlayers[i] = {};
                continue;
            }
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
            wp.hasBomb = !isDead && (i == resolvedBombCarrierSlot);
            wp.hasBones = false;
            collectGrenadesForSlot(i, wp.grenadeIds, wp.grenadeCount);
            memcpy(wp.name, names[i], 128);
            wp.name[127] = '\0';
            copyResolvedBones(i, wp);
        }
    } else if (s_webRadarCacheLive) {
        s_webRadarCacheLive = false;
        std::lock_guard<std::mutex> lock(s_dataMutex);
        memset(s_webRadarPlayers, 0, sizeof(s_webRadarPlayers));
    }

    if (minimapBoundsValid)
        HandleMapCalibration(minimapMins, minimapMaxs, liveMapKey);

    PublishCurrentSnapshot(SnapshotAll);

    s_requiredReadFailureCount = 0;
    MarkDmaReadSuccess();
