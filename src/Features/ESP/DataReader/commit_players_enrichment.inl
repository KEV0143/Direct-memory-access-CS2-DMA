        static uintptr_t s_cachedNoKnifePrimaryPawns[64] = {};
        static uint16_t s_cachedNoKnifePrimaryIconIds[64] = {};
        static uint16_t s_lastGoodWeaponIconId[64] = {};
        for (int i = 0; i < 64; ++i) {
            if (s_cachedNoKnifePrimaryPawns[i] == pawns[i])
                continue;
            s_cachedNoKnifePrimaryPawns[i] = pawns[i];
            s_cachedNoKnifePrimaryIconIds[i] = 0;
            s_lastGoodWeaponIconId[i] = 0;
        }

        auto resolvePrimaryInventoryWeaponState = [&](int playerSlot, bool& inventoryKnown) -> uint16_t {
            const int inventorySlotCount = getInventorySlotCount(playerSlot);
            inventoryKnown = false;
            for (int slot = 0; slot < inventorySlotCount; ++slot) {
                const uint16_t candidate = inventoryWeaponIds[playerSlot][slot];
                if (candidate == 0 || candidate >= 20000u)
                    continue;
                inventoryKnown = true;
                if (IsPrimaryWeaponItemId(candidate))
                    return candidate;
            }
            return 0;
        };

        auto resolveDisplayWeaponIconId = [&](int playerSlot, uint16_t activeWeaponId) -> uint16_t {
            uint16_t result = 0;
            if (activeWeaponId == 0) {
                result = s_lastGoodWeaponIconId[playerSlot];
            } else if (!g::espWeaponIconNoKnife || !IsKnifeItemId(activeWeaponId)) {
                result = activeWeaponId;
            } else {
                bool inventoryKnown = false;
                const uint16_t primaryWeaponId = resolvePrimaryInventoryWeaponState(playerSlot, inventoryKnown);
                if (primaryWeaponId != 0) {
                    s_cachedNoKnifePrimaryIconIds[playerSlot] = primaryWeaponId;
                    result = primaryWeaponId;
                } else if (inventoryKnown) {
                    s_cachedNoKnifePrimaryIconIds[playerSlot] = 0;
                    result = activeWeaponId;
                } else {
                    result = s_cachedNoKnifePrimaryIconIds[playerSlot];
                    if (result == 0)
                        result = s_lastGoodWeaponIconId[playerSlot];
                    if (result == 0)
                        result = activeWeaponId;
                }
            }
            if (result != 0)
                s_lastGoodWeaponIconId[playerSlot] = result;
            return result;
        };

        for (int i = 0; i < 64; ++i) {
            esp::PlayerData& p = s_players[i];
            if (!p.valid || !p.pawn || p.pawn != pawns[i])
                continue;

            p.money = std::max(0, moneys[i]);
            p.ping = static_cast<int>(pings[i]);
            p.scoped = scopedFlags[i] != 0;
            p.defusing = defusingFlags[i] != 0;
            p.hasDefuser = hasDefuserFlags[i] != 0;
            p.flashDuration = flashDurations[i];
            p.flashed = p.flashDuration > 0.05f;
            p.eyeYaw = eyeAnglesPerPlayer[i].y;
            p.spottedMask = (uint64_t)spottedMasks[i][1] << 32 | (uint64_t)spottedMasks[i][0];
            const bool spottedByAny = spottedFlags[i] != 0;
            bool visibleByMask = false;
            if (p.spottedMask != 0ULL) {
                visibleByMask =
                    IsMaskBitSet(p.spottedMask, localMaskBit) ||
                    IsMaskBitSet(p.spottedMask, localMaskBit - 1) ||
                    IsMaskBitSet(p.spottedMask, localMaskBit + 1) ||
                    IsMaskBitSet(p.spottedMask, localMaskSlotBit) ||
                    IsMaskBitSet(p.spottedMask, localMaskSlotBit - 1) ||
                    IsMaskBitSet(p.spottedMask, localMaskSlotBit + 1) ||
                    IsMaskBitSet(p.spottedMask, localHandleSlotBit) ||
                    IsMaskBitSet(p.spottedMask, localHandleSlotBit - 1) ||
                    IsMaskBitSet(p.spottedMask, localHandleSlotBit + 1) ||
                    IsMaskBitSet(p.spottedMask, localControllerMaskBit) ||
                    IsMaskBitSet(p.spottedMask, localControllerMaskBit - 1) ||
                    IsMaskBitSet(p.spottedMask, localControllerMaskBit + 1);
            }
            p.visible = visibleByMask;
            if (!p.visible && !localMaskResolved && spottedByAny)
                p.visible = true;
            memcpy(p.name, names[i], 128);
            p.name[127] = '\0';

            const uint16_t liveWeaponId = (weaponIds[i] < 20000u) ? weaponIds[i] : 0;
            uint16_t committedWeaponId = 0;
            int committedAmmoClip = ammoClips[i];
            resolveCommittedWeaponState(i, liveWeaponId, committedWeaponId, committedAmmoClip);
            p.ammoClip = committedAmmoClip;
            p.weaponId = committedWeaponId;
            p.weaponIconId = resolveDisplayWeaponIconId(i, p.weaponId);
            p.hasBomb = (i == resolvedBombCarrierSlot);
            collectGrenadesForSlot(i, p.grenadeIds, p.grenadeCount);

            if (!copyResolvedBones(i, p)) {
                p.hasBones = false;
            }
        }
