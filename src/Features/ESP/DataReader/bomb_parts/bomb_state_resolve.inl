    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    const bool bombEntityTainted =
        s_explodedEntityTaintPtr != 0 &&
        plantedC4Entity != 0 &&
        plantedC4Entity == s_explodedEntityTaintPtr &&
        s_explodedEntityTaintUntilUs > nowUs;
    const bool bombBlowTimeFresh =
        std::isfinite(bombBlowTime) &&
        std::isfinite(currentGameTime) &&
        bombBlowTime > currentGameTime + 0.05f &&
        bombBlowTime < currentGameTime + 120.0f;
    const bool hasLivePlantedSignal =
        bombTicking != 0 ||
        bombBeingDefused != 0 ||
        bombBlowTimeFresh;
    const bool plantedBombFromEntity =
        bombPlantedByRules &&
        (plantedC4Entity != 0) &&
        !bombEntityTainted &&
        hasLivePlantedSignal;
    const bool bombPlantedNow = plantedBombFromEntity;
    Vector3 droppedBombPos = { NAN, NAN, NAN };
    Vector3 droppedBombBoundsMins = {};
    Vector3 droppedBombBoundsMaxs = {};
    bool droppedBombPosValid = false;
    bool droppedBombBoundsValid = false;
    int droppedBombScore = (std::numeric_limits<int>::min)();
    uint32_t droppedBombSourceFlags = 0;

    
    
    
    
    
    auto bombPositionInsideAlivePlayer = [&](const Vector3& bombPos) -> bool {
        if (bombDroppedByRules)
            return false;
        if (!std::isfinite(bombPos.x) || !std::isfinite(bombPos.y) || !std::isfinite(bombPos.z))
            return false;
        constexpr int kBombCarrierTeamT = 2;
        for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
            const int i = playerResolvedSlots[resolvedIdx];
            if (healths[i] <= 0 || lifeStates[i] != 0)
                continue;
            if (teams[i] != kBombCarrierTeamT)
                continue;
            if (!std::isfinite(positions[i].x) ||
                !std::isfinite(positions[i].y) ||
                !std::isfinite(positions[i].z))
                continue;
            const bool likelyCarryCandidate =
                bombCarrierBySlot[i] ||
                inventoryHasBombBySlot[i] ||
                weaponIds[i] == kWeaponC4Id ||
                i == weaponC4OwnerPlayerIndex ||
                (i == s_cachedBombCarryOwnerSlot &&
                 s_cachedBombCarryOwnerUs > 0 &&
                 (nowUs - s_cachedBombCarryOwnerUs) <= kBombOwnerCarryStickyUs) ||
                (i == s_cachedBombAttachedOwnerSlot &&
                 s_cachedBombAttachedOwnerUs > 0 &&
                 (nowUs - s_cachedBombAttachedOwnerUs) <= kBombOwnerAttachGraceUs);
            if (!likelyCarryCandidate)
                continue;
            const float dx = bombPos.x - positions[i].x;
            const float dy = bombPos.y - positions[i].y;
            const float dz = std::fabs(bombPos.z - positions[i].z);
            if ((dx * dx + dy * dy) <= (32.0f * 32.0f) && dz <= 64.0f)
                return true;
        }
        return false;
    };

    
    
    
    
    
    constexpr int kDroppedC4AcceptScore = 80;

    const bool inDropResetGrace =
        s_bombDropResetGraceUntilUs > 0 && s_bombDropResetGraceUntilUs > nowUs;
    const bool recentWeaponC4CarryEvidence =
        anyBombCarrierNow ||
        treatWeaponC4OwnerAsCarrier ||
        weaponC4StrongCarrySignal ||
        weaponC4SpatialCarryVeto ||
        (s_cachedBombCarryOwnerSlot >= 0 &&
         s_cachedBombCarryOwnerUs > 0 &&
         (nowUs - s_cachedBombCarryOwnerUs) <= kBombOwnerCarryStickyUs) ||
        (s_cachedBombAttachedOwnerSlot >= 0 &&
         s_cachedBombAttachedOwnerUs > 0 &&
         (nowUs - s_cachedBombAttachedOwnerUs) <= kBombOwnerAttachGraceUs);
    const bool worldScanC4DetachedFromLiveOwner =
        bombDroppedByRules ||
        worldScanC4NoOwner ||
        worldScanC4OwnerIdx < 0 ||
        !worldScanC4OwnerAlive;
    const bool worldC4DropEvidence =
        worldScanFoundC4 &&
        (bombDroppedByRules ? isDroppedC4PositionPlausible(worldScanC4Pos)
                            : isDroppedC4WeakPositionPlausible(worldScanC4Pos)) &&
        worldScanC4Score >= kDroppedC4AcceptScore &&
        worldScanC4DetachedFromLiveOwner;
    const bool weakWeaponDropAllowed =
        bombDroppedByRules ||
        (worldC4DropEvidence &&
         !recentWeaponC4CarryEvidence &&
         !bombPositionInsideAlivePlayer(worldScanC4Pos));

    const bool weaponC4DroppedByRulesNow =
        weaponC4PosValid &&
        bombDroppedByRules &&
        !weaponC4StrongCarrySignal;
    const bool weaponC4DroppedByOwnerLoss =
        !inDropResetGrace &&
        weaponC4PosValid &&
        weakWeaponDropAllowed &&
        (!weaponC4OwnerValid ||
         weaponC4OwnerPlayerIndex < 0 ||
         !weaponC4OwnerAlive);
    const bool weaponC4DroppedByDistance =
        !inDropResetGrace &&
        weaponC4PosValid &&
        weakWeaponDropAllowed &&
        weaponC4OwnerAlive &&
        weaponC4OwnerDist2D > 160.0f;
    const bool weaponC4LooksDropped =
        !bombPlantedNow &&
        (bombDroppedByRules ? isDroppedC4PositionPlausible(weaponC4WorldPos)
                            : isDroppedC4WeakPositionPlausible(weaponC4WorldPos)) &&
        !weaponC4StrongCarrySignal &&
        (weaponC4DroppedByRulesNow ||
         weaponC4DroppedByOwnerLoss ||
         weaponC4DroppedByDistance ||
         (bombDroppedByRules && weaponC4LooksDroppedNearOwner));
    const bool weaponC4DefinitelyNotCarried =
        !anyBombCarrierNow &&
        !treatWeaponC4OwnerAsCarrier &&
        (weaponC4DroppedByRulesNow ||
         weaponC4DroppedByOwnerLoss ||
         weaponC4DroppedByDistance);
    if (!bombPlantedNow && weaponC4PosValid &&
        (weaponC4LooksDropped || weaponC4DefinitelyNotCarried) &&
        !bombPositionInsideAlivePlayer(weaponC4WorldPos)) {
        droppedBombPos = weaponC4WorldPos;
        droppedBombPosValid = true;
        droppedBombBoundsMins = weaponC4CollisionMins;
        droppedBombBoundsMaxs = weaponC4CollisionMaxs;
        droppedBombBoundsValid = isValidBombBounds(droppedBombBoundsMins, droppedBombBoundsMaxs);
        droppedBombScore = bombDroppedByRules ? 120 : 140;
        droppedBombSourceFlags = BombResolveSourceWeaponEntity;
        if (bombDroppedByRules)
            droppedBombSourceFlags |= BombResolveSourceRules;
    }

    const uint64_t worldC4StickyUs = bombDroppedByRules ? 1200000u : 220000u;
    if (worldScanFoundC4 &&
        (bombDroppedByRules ? isDroppedC4PositionPlausible(worldScanC4Pos)
                            : isDroppedC4WeakPositionPlausible(worldScanC4Pos)) &&
        worldScanC4Score >= kDroppedC4AcceptScore &&
        worldScanC4DetachedFromLiveOwner) {
        s_lastWorldC4Pos = worldScanC4Pos;
        s_lastWorldC4PosUs = nowUs;
        s_lastWorldC4NoOwner = worldScanC4NoOwner;
        s_lastWorldC4OwnerIdx = worldScanC4OwnerIdx;
        s_lastWorldC4OwnerAlive = worldScanC4OwnerAlive;
        s_lastWorldC4OwnerNearby = worldScanC4OwnerNearby;
    } else if (s_lastWorldC4PosUs > 0 &&
               (nowUs - s_lastWorldC4PosUs) <= worldC4StickyUs &&
               (bombDroppedByRules ? isDroppedC4PositionPlausible(s_lastWorldC4Pos)
                                   : isDroppedC4WeakPositionPlausible(s_lastWorldC4Pos))) {
        worldScanFoundC4 = true;
        worldScanC4Pos = s_lastWorldC4Pos;
        worldScanC4NoOwner = s_lastWorldC4NoOwner;
        worldScanC4OwnerIdx = s_lastWorldC4OwnerIdx;
        worldScanC4OwnerAlive = s_lastWorldC4OwnerAlive;
        worldScanC4OwnerNearby = s_lastWorldC4OwnerNearby;
        worldScanC4Score = kDroppedC4AcceptScore;
    }

    auto scoreDroppedC4Candidate = [&](bool noOwner,
                                       int ownerIdx,
                                       bool ownerAlive,
                                       bool ownerNearby,
                                       bool ownerCarrySignal,
                                       const Vector3& pos) -> int {
        int score = 0;
        if (bombDroppedByRules ? isDroppedC4PositionPlausible(pos)
                               : isDroppedC4WeakPositionPlausible(pos))
            score += 40;
        if (bombDroppedByRules)
            score += 180;
        if (noOwner)
            score += 220;
        if (ownerIdx < 0)
            score += 120;
        if (!ownerAlive)
            score += 100;
        if (!ownerNearby)
            score += 80;
        if (ownerCarrySignal)
            score -= bombDroppedByRules ? 80 : 220;
        if (!bombDroppedByRules && ownerAlive && ownerNearby)
            score -= 120;
        if (s_bombState.dropped && isFiniteVec(s_bombState.position)) {
            const float dx = pos.x - s_bombState.position.x;
            const float dy = pos.y - s_bombState.position.y;
            if ((dx * dx + dy * dy) <= (160.0f * 160.0f))
                score += 45;
        }
        if (s_lastDroppedBombPosUs > 0 && isValidWorldPos(s_lastDroppedBombPos)) {
            const float dx = pos.x - s_lastDroppedBombPos.x;
            const float dy = pos.y - s_lastDroppedBombPos.y;
            if ((dx * dx + dy * dy) <= (160.0f * 160.0f))
                score += 40;
        }
        return score;
    };

    if (worldScanFoundC4) {
        bool worldOwnerCarrySignal = false;
        int worldCandidateScore = (std::numeric_limits<int>::min)();
        if (!bombDroppedByRules &&
            worldScanC4OwnerAlive &&
            worldScanC4OwnerNearby &&
            worldScanC4OwnerIdx >= 0 &&
            worldScanC4OwnerIdx < 64) {
            worldOwnerCarrySignal =
                (activeWeapons[worldScanC4OwnerIdx] != 0 && activeWeapons[worldScanC4OwnerIdx] == worldScanC4Entity) ||
            (weaponIds[worldScanC4OwnerIdx] == kWeaponC4Id) ||
                inventoryHasBombBySlot[worldScanC4OwnerIdx] ||
                (worldScanC4OwnerIdx == s_cachedBombCarryOwnerSlot &&
                 s_cachedBombCarryOwnerUs > 0 &&
                 (nowUs - s_cachedBombCarryOwnerUs) <= kBombOwnerCarryStickyUs) ||
                (worldScanC4OwnerIdx == s_cachedBombAttachedOwnerSlot &&
                 s_cachedBombAttachedOwnerUs > 0 &&
                 (nowUs - s_cachedBombAttachedOwnerUs) <= kBombOwnerAttachGraceUs);
        }
        if (!bombDroppedByRules &&
            worldScanC4OwnerAlive &&
            worldScanC4OwnerNearby &&
            worldScanC4OwnerIdx >= 0 &&
            worldScanC4OwnerIdx < 64 &&
            worldOwnerCarrySignal) {
            bombCarrierBySlot[worldScanC4OwnerIdx] = true;
        }
        if (!bombPlantedNow && !inDropResetGrace) {
            worldCandidateScore = scoreDroppedC4Candidate(
                worldScanC4NoOwner,
                worldScanC4OwnerIdx,
                worldScanC4OwnerAlive,
                worldScanC4OwnerNearby,
            worldOwnerCarrySignal,
            worldScanC4Pos);
            const bool c4LooksDropped =
                worldCandidateScore >= kDroppedC4AcceptScore &&
                (bombDroppedByRules ||
                 worldScanC4NoOwner ||
                 worldScanC4OwnerIdx < 0 ||
                 !worldScanC4OwnerAlive);
            if (c4LooksDropped && worldCandidateScore > droppedBombScore &&
                (bombDroppedByRules ? isDroppedC4PositionPlausible(worldScanC4Pos)
                                    : isDroppedC4WeakPositionPlausible(worldScanC4Pos)) &&
                !bombPositionInsideAlivePlayer(worldScanC4Pos)) {
                droppedBombPos = worldScanC4Pos;
                droppedBombPosValid = true;
                droppedBombBoundsMins = {};
                droppedBombBoundsMaxs = {};
                droppedBombBoundsValid = false;
                droppedBombScore = worldCandidateScore;
                droppedBombSourceFlags = BombResolveSourceWorldC4;
                if (bombDroppedByRules)
                    droppedBombSourceFlags |= BombResolveSourceRules;
            }
        }
    }

    const uint64_t droppedStickyUs =
        bombDroppedByRules ? esp::intervals::kBombStickyDroppedUs : 220000u;
    const uint64_t confirmedDroppedStickyUs =
        bombDroppedByRules ? 1200000u : 300000u;
    BombResolveResult bombResolve = {};
    if (bombPlantedNow && isValidWorldPos(bombWorldPos)) {
        bombResolve.kind = BombResolveKind::Planted;
        bombResolve.sourceFlags = BombResolveSourceRules | BombResolveSourceWeaponEntity;
        bombResolve.confidence = 255;
        bombResolve.position = bombWorldPos;
        bombResolve.boundsMins = bombCollisionMins;
        bombResolve.boundsMaxs = bombCollisionMaxs;
        bombResolve.boundsValid = isValidBombBounds(bombCollisionMins, bombCollisionMaxs);
    } else {
        
        
        
        const bool carryResolved =
            treatWeaponC4OwnerAsCarrier ||
            weaponC4StrongCarrySignal ||
            anyBombCarrierNow;
        const uint32_t carrySourceFlags =
            (treatWeaponC4OwnerAsCarrier || weaponC4StrongCarrySignal ? BombResolveSourceCarrySignal : 0u) |
            (weaponC4OwnerCarrySticky ? BombResolveSourceCarrySticky : 0u) |
            (weaponC4OwnerAttachGrace ? BombResolveSourceAttachGrace : 0u);

        if (droppedBombPosValid && isValidWorldPos(droppedBombPos)) {
            const bool droppedConfirmed = droppedBombScore >= 120;
            bombResolve.kind = droppedConfirmed ? BombResolveKind::DroppedConfirmed : BombResolveKind::DroppedProbable;
            bombResolve.sourceFlags = droppedBombSourceFlags;
            bombResolve.confidence = static_cast<uint8_t>(std::clamp(droppedBombScore, 80, 255));
            bombResolve.position = droppedBombPos;
            bombResolve.boundsMins = droppedBombBoundsMins;
            bombResolve.boundsMaxs = droppedBombBoundsMaxs;
            bombResolve.boundsValid =
                droppedBombBoundsValid &&
                isValidBombBounds(droppedBombBoundsMins, droppedBombBoundsMaxs);
        } else if (carryResolved) {
            bombResolve.kind = BombResolveKind::Carried;
            bombResolve.sourceFlags = carrySourceFlags;
            bombResolve.confidence = 200;
        } else if (!inDropResetGrace &&
                   !recentWeaponC4CarryEvidence &&
                   s_lastDroppedBombPosUs > 0 &&
                   (nowUs - s_lastDroppedBombPosUs) <= droppedStickyUs &&
                   isValidWorldPos(s_lastDroppedBombPos)) {
            bombResolve.kind = BombResolveKind::DroppedProbable;
            bombResolve.sourceFlags = BombResolveSourceStickyDrop;
            bombResolve.confidence = 96;
            bombResolve.position = s_lastDroppedBombPos;
        } else if (!inDropResetGrace &&
                   !recentWeaponC4CarryEvidence &&
                   s_lastConfirmedBombStateUs > 0 &&
                   (nowUs - s_lastConfirmedBombStateUs) <= confirmedDroppedStickyUs &&
                   isValidWorldPos(s_lastConfirmedBombState.position) &&
                   s_lastConfirmedBombState.dropped) {
            bombResolve.kind = BombResolveKind::DroppedProbable;
            bombResolve.sourceFlags = BombResolveSourceStickyState;
            bombResolve.confidence = std::max<uint8_t>(s_lastConfirmedBombState.confidence, static_cast<uint8_t>(88));
            bombResolve.position = s_lastConfirmedBombState.position;
            bombResolve.boundsMins = s_lastConfirmedBombState.boundsMins;
            bombResolve.boundsMaxs = s_lastConfirmedBombState.boundsMaxs;
            bombResolve.boundsValid = s_lastConfirmedBombState.boundsValid;
        }
    }

    if (narrowDebugEnabled(kNarrowDebugC4)) {
        static bool s_prevBombPlantedNow = false;
        static bool s_prevDroppedBombPosValid = false;
        static bool s_prevAnyBombCarrierNow = false;
        static bool s_prevWorldScanFoundC4 = false;
        const bool emitC4Debug =
            (bombPlantedNow != s_prevBombPlantedNow) ||
            (droppedBombPosValid != s_prevDroppedBombPosValid) ||
            (anyBombCarrierNow != s_prevAnyBombCarrierNow) ||
            (worldScanFoundC4 != s_prevWorldScanFoundC4);
        if (emitC4Debug) {
            int carrierCount = 0;
            int inventoryBombCount = 0;
            int carrierSlot = -1;
            for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
                const int i = playerResolvedSlots[resolvedIdx];
                if (bombCarrierBySlot[i]) {
                    ++carrierCount;
                    if (carrierSlot < 0)
                        carrierSlot = i;
                }
                if (inventoryHasBombBySlot[i])
                    ++inventoryBombCount;
            }
            DmaLogPrintf(
                "[DEBUG] C4: rules(drop/planted)=%d/%d plantedNow=%d carriers=%d invSlots=%d ownerValid=%d ownerIdx=%d ownerCarry=%d ownerInv=%d ownerAttachGrace=%d dropNearOwner=%d droppedValid=%d weaponPosValid=%d plantedPosValid=%d worldFallback=%d dropPos=(%.1f,%.1f,%.1f) weaponPos=(%.1f,%.1f,%.1f) plantPos=(%.1f,%.1f,%.1f)",
                bombDroppedByRules ? 1 : 0,
                bombPlantedByRules ? 1 : 0,
                bombPlantedNow ? 1 : 0,
                carrierCount,
                inventoryBombCount,
                weaponC4OwnerValid ? 1 : 0,
                weaponC4OwnerPlayerIndex,
                treatWeaponC4OwnerAsCarrier ? 1 : 0,
                weaponC4OwnerSlowInventoryBomb ? 1 : 0,
                weaponC4OwnerAttachGrace ? 1 : 0,
                weaponC4LooksDroppedNearOwner ? 1 : 0,
                droppedBombPosValid ? 1 : 0,
                weaponC4PosValid ? 1 : 0,
                isValidWorldPos(bombWorldPos) ? 1 : 0,
                worldScanFoundC4 ? 1 : 0,
                droppedBombPos.x,
                droppedBombPos.y,
                droppedBombPos.z,
                weaponC4WorldPos.x,
                weaponC4WorldPos.y,
                weaponC4WorldPos.z,
                bombWorldPos.x,
                bombWorldPos.y,
                bombWorldPos.z);
            if (carrierSlot >= 0) {
                DmaLogPrintf(
                    "[DEBUG] C4 carrier: slot=%d pawn=0x%llX pos=(%.1f,%.1f,%.1f) health=%d life=%u invHasBomb=%d",
                    carrierSlot,
                    static_cast<unsigned long long>(pawns[carrierSlot]),
                    positions[carrierSlot].x,
                    positions[carrierSlot].y,
                    positions[carrierSlot].z,
                    healths[carrierSlot],
                    static_cast<unsigned>(lifeStates[carrierSlot]),
                    inventoryHasBombBySlot[carrierSlot] ? 1 : 0);
            }
        }
        s_prevBombPlantedNow = bombPlantedNow;
        s_prevDroppedBombPosValid = droppedBombPosValid;
        s_prevAnyBombCarrierNow = anyBombCarrierNow;
        s_prevWorldScanFoundC4 = worldScanFoundC4;
    }
