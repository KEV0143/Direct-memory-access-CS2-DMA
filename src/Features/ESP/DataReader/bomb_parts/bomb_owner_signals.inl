    const bool weaponC4OwnerValid = (weaponC4OwnerHandle != 0u && weaponC4OwnerHandle != 0xFFFFFFFFu);
    if (weaponC4OwnerValid)
        weaponC4OwnerPlayerIndex = findPlayerIndexByEntityHandle(weaponC4OwnerHandle);
    if (weaponC4OwnerPlayerIndex >= 0) {
        if (s_lastObservedBombOwnerSlot != weaponC4OwnerPlayerIndex) {
            s_lastObservedBombOwnerSlot = weaponC4OwnerPlayerIndex;
            s_cachedBombAttachedOwnerSlot = weaponC4OwnerPlayerIndex;
            s_cachedBombAttachedOwnerUs = nowUs;
        }
    } else {
        s_lastObservedBombOwnerSlot = -1;
    }

    bool weaponC4OwnerLooksCarried = false;
    bool weaponC4LooksDroppedNearOwner = false;
    bool weaponC4OwnerAlive = false;
    float weaponC4OwnerDist2D = FLT_MAX;
    bool weaponC4StrongCarrySignal = false;
    bool weaponC4OwnerSelectedC4 = false;
    bool weaponC4OwnerSlowInventoryBomb = false;
    bool weaponC4OwnerNear = false;
    bool weaponC4OwnerGroundEnvelope = false;
    bool weaponC4OwnerCarrySticky = false;
    bool weaponC4OwnerAttachGrace = false;
    bool weaponC4SpatialCarryVeto = false;
    int weaponC4SpatialCarrySlot = -1;
    constexpr uint64_t kBombOwnerCarryStickyUs = 250000u;
    constexpr uint64_t kBombOwnerAttachGraceUs = 200000u;
    if (weaponC4OwnerPlayerIndex >= 0) {
        weaponC4OwnerAlive =
            healths[weaponC4OwnerPlayerIndex] > 0 &&
            lifeStates[weaponC4OwnerPlayerIndex] == 0 &&
            std::isfinite(positions[weaponC4OwnerPlayerIndex].x) &&
            std::isfinite(positions[weaponC4OwnerPlayerIndex].y) &&
            std::isfinite(positions[weaponC4OwnerPlayerIndex].z);
        if (weaponC4OwnerAlive) {
            weaponC4OwnerSelectedC4 =
                (activeWeapons[weaponC4OwnerPlayerIndex] != 0 && activeWeapons[weaponC4OwnerPlayerIndex] == weaponC4Entity) ||
            (weaponIds[weaponC4OwnerPlayerIndex] == kWeaponC4Id);
            weaponC4OwnerSlowInventoryBomb = inventoryHasBombBySlot[weaponC4OwnerPlayerIndex];
            weaponC4OwnerCarrySticky =
                weaponC4OwnerPlayerIndex == s_cachedBombCarryOwnerSlot &&
                s_cachedBombCarryOwnerUs > 0 &&
                (nowUs - s_cachedBombCarryOwnerUs) <= kBombOwnerCarryStickyUs;
            if (!weaponC4PosValid) {
                weaponC4StrongCarrySignal =
                    weaponC4OwnerSelectedC4 ||
                    weaponC4OwnerSlowInventoryBomb ||
                    (!bombDroppedByRules && weaponC4OwnerCarrySticky);
            } else {
                const Vector3& ownerPos = positions[weaponC4OwnerPlayerIndex];
                const float dx = weaponC4WorldPos.x - ownerPos.x;
                const float dy = weaponC4WorldPos.y - ownerPos.y;
                const float dz = weaponC4WorldPos.z - ownerPos.z;
                weaponC4OwnerDist2D = std::sqrt(dx * dx + dy * dy);
                weaponC4OwnerNear = weaponC4OwnerDist2D <= 96.0f;
                weaponC4OwnerGroundEnvelope = std::fabs(dz) <= 18.0f;
                weaponC4OwnerAttachGrace =
                    weaponC4OwnerPlayerIndex == s_cachedBombAttachedOwnerSlot &&
                    s_cachedBombAttachedOwnerUs > 0 &&
                    (nowUs - s_cachedBombAttachedOwnerUs) <= kBombOwnerAttachGraceUs &&
                    weaponC4OwnerNear &&
                    weaponC4OwnerGroundEnvelope;
                weaponC4StrongCarrySignal =
                    weaponC4OwnerSelectedC4 ||
                    weaponC4OwnerSlowInventoryBomb ||
                    (!bombDroppedByRules && weaponC4OwnerCarrySticky) ||
                    weaponC4OwnerAttachGrace;
                weaponC4LooksDroppedNearOwner =
                    weaponC4OwnerNear &&
                    weaponC4OwnerGroundEnvelope &&
                    !weaponC4StrongCarrySignal &&
                    bombDroppedByRules;
            }

            weaponC4OwnerLooksCarried = weaponC4StrongCarrySignal;
        }
    }

    
    
    

    
    
    if (weaponC4PosValid && weaponC4OwnerAlive && weaponC4OwnerDist2D > 96.0f) {
        weaponC4OwnerCarrySticky = false;
        weaponC4OwnerAttachGrace = false;
        weaponC4StrongCarrySignal =
            weaponC4OwnerSelectedC4 || weaponC4OwnerSlowInventoryBomb;
        weaponC4OwnerLooksCarried = weaponC4StrongCarrySignal;
        s_cachedBombCarryOwnerSlot = -1;
        s_cachedBombCarryOwnerUs = 0;
        s_cachedBombAttachedOwnerSlot = -1;
        s_cachedBombAttachedOwnerUs = 0;
    }

    
    
    
    if (bombDroppedByRules) {
        weaponC4OwnerCarrySticky = false;
        weaponC4OwnerAttachGrace = false;
        weaponC4StrongCarrySignal = weaponC4OwnerSelectedC4;
        weaponC4OwnerLooksCarried = weaponC4StrongCarrySignal;
        s_cachedBombCarryOwnerSlot = -1;
        s_cachedBombCarryOwnerUs = 0;
        s_cachedBombAttachedOwnerSlot = -1;
        s_cachedBombAttachedOwnerUs = 0;
    }

    const bool treatWeaponC4OwnerAsCarrier =
        weaponC4OwnerValid &&
        weaponC4OwnerPlayerIndex >= 0 &&
        weaponC4OwnerLooksCarried;
    if (treatWeaponC4OwnerAsCarrier) {
        s_cachedBombCarryOwnerSlot = weaponC4OwnerPlayerIndex;
        s_cachedBombCarryOwnerUs = nowUs;
    } else if (s_cachedBombCarryOwnerUs > 0 &&
               (nowUs - s_cachedBombCarryOwnerUs) > 800000u) {
        s_cachedBombCarryOwnerSlot = -1;
        s_cachedBombCarryOwnerUs = 0;
    }
    if (treatWeaponC4OwnerAsCarrier)
        bombCarrierBySlot[weaponC4OwnerPlayerIndex] = true;

    bool anyBombCarrierNow = false;
    for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
        const int i = playerResolvedSlots[resolvedIdx];
        if (bombCarrierBySlot[i]) {
            anyBombCarrierNow = true;
            break;
        }
    }

    
    
    
    
    
    
    
    
    if (weaponC4PosValid && !bombDroppedByRules) {
        constexpr int kBombCarrierTeamT = 2;
        int bombPositionCarrierSlot = -1;
        float bestDist2DSq = FLT_MAX;
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
            const bool hasCarryEvidence =
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
            if (!hasCarryEvidence)
                continue;
            const float dx = weaponC4WorldPos.x - positions[i].x;
            const float dy = weaponC4WorldPos.y - positions[i].y;
            const float dz = std::fabs(weaponC4WorldPos.z - positions[i].z);
            const float dist2DSq = dx * dx + dy * dy;
            if (dist2DSq <= (32.0f * 32.0f) &&
                dz <= 64.0f &&
                dist2DSq < bestDist2DSq) {
                bestDist2DSq = dist2DSq;
                bombPositionCarrierSlot = i;
            }
        }
        if (bombPositionCarrierSlot >= 0) {
            weaponC4SpatialCarryVeto = true;
            weaponC4SpatialCarrySlot = bombPositionCarrierSlot;
            weaponC4LooksDroppedNearOwner = false;
        }
    }
