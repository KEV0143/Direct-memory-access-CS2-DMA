    uintptr_t controllers[64] = {};
    uint32_t pawnHandles[64] = {};
    char names[64][128] = {};
    uint32_t pings[64] = {};
    uintptr_t moneyServices[64] = {};
    int moneys[64] = {};
    uintptr_t pawnEntries[64] = {};
    uintptr_t pawns[64] = {};
    int localMaskBit = -1;
    int localMaskSlotBit = -1;
    int localHandleSlotBit = -1;
    int localControllerMaskBit = -1;
    bool localMaskResolved = false;
    const uint64_t _playerHierarchyStartUs = TickNowUs();
    uint64_t _playerHierarchyUs = 0;
    uint64_t _playerCoreUs = 0;
    uint64_t _playerRepairUs = 0;
    int playerResolvedSlots[64] = {};
    int playerResolvedSlotCount = 0;
    static bool s_stalePawnEvictionQueue[64] = {};

    if (canReadEntityHierarchy) {
        #include "player_parts/player_hierarchy.inl"
    }
    _playerHierarchyUs = TickNowUs() - _playerHierarchyStartUs;
    const uint64_t _playerCoreStartUs = TickNowUs();

    int healths[64] = {};
    int armors[64] = {};
    int teams[64] = {};
    bool liveTeamReads[64] = {};
    uint8_t lifeStates[64] = {};
    Vector3 positions[64] = {};
    uintptr_t sceneNodes[64] = {};
    uintptr_t clippingWeapons[64] = {};
    uintptr_t weaponServices[64] = {};
    uint32_t activeWeaponHandles[64] = {};
    uintptr_t activeWeaponEntries[64] = {};
    uintptr_t activeWeapons[64] = {};
    static constexpr int kMaxInventoryWeapons = 16;
    int inventoryWeaponCounts[64] = {};
    uintptr_t inventoryWeaponHandleArrays[64] = {};
    uint32_t inventoryWeaponHandles[64][kMaxInventoryWeapons] = {};
    uintptr_t inventoryWeaponEntries[64][kMaxInventoryWeapons] = {};
    uintptr_t inventoryWeapons[64][kMaxInventoryWeapons] = {};
    uint16_t inventoryWeaponIds[64][kMaxInventoryWeapons] = {};
    bool inventoryHasBombBySlot[64] = {};
    uint16_t weaponIds[64] = {};
    int ammoClips[64] = {};
    bool bombCarrierBySlot[64] = {};
    auto getInventorySlotCount = [&](int idx) -> int {
        if (idx < 0 || idx >= 64)
            return 0;
        return std::clamp(inventoryWeaponCounts[idx], 0, kMaxInventoryWeapons);
    };
    
    int weaponC4OwnerPlayerIndex = -1;
    auto findPlayerIndexByEntityHandle = [&](uint32_t handleValue) -> int {
        const uint32_t slot = handleValue & kEntityHandleMask;
        if (slot == 0u || slot == 0x7FFFu)
            return -1;
        for (int i = 0; i < 64; ++i) {
            const uint32_t pawnSlot = pawnHandles[i] & kEntityHandleMask;
            if (pawnSlot != 0u && pawnSlot == slot)
                return i;
        }
        if (slot <= 64u && controllers[slot - 1] != 0)
            return static_cast<int>(slot - 1u);
        return -1;
    };
    uintptr_t itemServices[64] = {};
    uint8_t hasDefuserFlags[64] = {};
    uint8_t scopedFlags[64] = {};
    uint8_t defusingFlags[64] = {};
    float flashDurations[64] = {};
    Vector3 eyeAnglesPerPlayer[64] = {};
    Vector3 velocities[64] = {};
    uint8_t spottedFlags[64] = {};
    uint32_t spottedMasks[64][2] = {};
    Vector3 allBones[64][28] = {};
    bool hasBoneData[64] = {};
    static char s_cachedPlayerNames[64][128] = {};
    static uint32_t s_cachedPlayerPings[64] = {};
    static uintptr_t s_cachedIdentityControllers[64] = {};
    static uintptr_t s_cachedMoneyControllers[64] = {};
    static uintptr_t s_cachedMoneyServices[64] = {};
    static int s_cachedPlayerMoneys[64] = {};
    static uintptr_t s_cachedDynamicPawns[64] = {};
    static uintptr_t s_cachedItemServices[64] = {};
    static uint8_t s_cachedHasDefuserFlags[64] = {};
    static uint8_t s_cachedScopedFlags[64] = {};
    static uint8_t s_cachedDefusingFlags[64] = {};
    static float s_cachedFlashDurations[64] = {};
    static Vector3 s_cachedEyeAnglesPerPlayer[64] = {};
    static uint8_t s_cachedSpottedFlags[64] = {};
    static uint32_t s_cachedSpottedMasks[64][2] = {};
    static uint64_t s_playerAuxCacheResetSerial = 0;
    {
        const uint64_t auxResetSerial = s_sceneResetSerial.load(std::memory_order_relaxed);
        if (s_playerAuxCacheResetSerial != auxResetSerial) {
            s_playerAuxCacheResetSerial = auxResetSerial;
            memset(s_cachedPlayerNames, 0, sizeof(s_cachedPlayerNames));
            memset(s_cachedPlayerPings, 0, sizeof(s_cachedPlayerPings));
            memset(s_cachedIdentityControllers, 0, sizeof(s_cachedIdentityControllers));
            memset(s_cachedMoneyControllers, 0, sizeof(s_cachedMoneyControllers));
            memset(s_cachedMoneyServices, 0, sizeof(s_cachedMoneyServices));
            memset(s_cachedPlayerMoneys, 0, sizeof(s_cachedPlayerMoneys));
            memset(s_cachedDynamicPawns, 0, sizeof(s_cachedDynamicPawns));
            memset(s_cachedItemServices, 0, sizeof(s_cachedItemServices));
            memset(s_cachedHasDefuserFlags, 0, sizeof(s_cachedHasDefuserFlags));
            memset(s_cachedScopedFlags, 0, sizeof(s_cachedScopedFlags));
            memset(s_cachedDefusingFlags, 0, sizeof(s_cachedDefusingFlags));
            memset(s_cachedFlashDurations, 0, sizeof(s_cachedFlashDurations));
            memset(s_cachedEyeAnglesPerPlayer, 0, sizeof(s_cachedEyeAnglesPerPlayer));
            memset(s_cachedSpottedFlags, 0, sizeof(s_cachedSpottedFlags));
            memset(s_cachedSpottedMasks, 0, sizeof(s_cachedSpottedMasks));
        }
    }
    
    
    
    memcpy(names, s_cachedPlayerNames, sizeof(names));
    memcpy(pings, s_cachedPlayerPings, sizeof(pings));
    memcpy(moneyServices, s_cachedMoneyServices, sizeof(moneyServices));
    memcpy(moneys, s_cachedPlayerMoneys, sizeof(moneys));
    memcpy(itemServices, s_cachedItemServices, sizeof(itemServices));
    memcpy(hasDefuserFlags, s_cachedHasDefuserFlags, sizeof(hasDefuserFlags));
    memcpy(scopedFlags, s_cachedScopedFlags, sizeof(scopedFlags));
    memcpy(defusingFlags, s_cachedDefusingFlags, sizeof(defusingFlags));
    memcpy(flashDurations, s_cachedFlashDurations, sizeof(flashDurations));
    memcpy(eyeAnglesPerPlayer, s_cachedEyeAnglesPerPlayer, sizeof(eyeAnglesPerPlayer));
    memcpy(spottedFlags, s_cachedSpottedFlags, sizeof(spottedFlags));
    memcpy(spottedMasks, s_cachedSpottedMasks, sizeof(spottedMasks));
    static int s_cachedCoreTeams[64] = {};
    static uintptr_t s_cachedCoreTeamPawns[64] = {};
    static uint64_t s_lastCoreTeamReadUs[64] = {};
    static uint64_t s_coreTeamCacheResetSerial = 0;
    {
        const uint64_t coreTeamResetSerial = s_sceneResetSerial.load(std::memory_order_relaxed);
        if (s_coreTeamCacheResetSerial != coreTeamResetSerial) {
            s_coreTeamCacheResetSerial = coreTeamResetSerial;
            memset(s_cachedCoreTeams, 0, sizeof(s_cachedCoreTeams));
            memset(s_cachedCoreTeamPawns, 0, sizeof(s_cachedCoreTeamPawns));
            memset(s_lastCoreTeamReadUs, 0, sizeof(s_lastCoreTeamReadUs));
        }
    }
    const int playerCacheSlotLimit = std::max(
        playerSlotScanLimit,
        std::clamp(s_playerHierarchyHighWaterSlot.load(std::memory_order_relaxed), 0, 64));
    for (int i = 0; i < playerCacheSlotLimit; ++i) {
        if (controllers[i] != s_cachedIdentityControllers[i]) {
            memset(names[i], 0, sizeof(names[i]));
            pings[i] = 0;
        }
        if (controllers[i] != s_cachedMoneyControllers[i]) {
            moneyServices[i] = 0;
            moneys[i] = 0;
        }
        if (pawns[i] != s_cachedDynamicPawns[i]) {
            itemServices[i] = 0;
            hasDefuserFlags[i] = 0;
            scopedFlags[i] = 0;
            defusingFlags[i] = 0;
            flashDurations[i] = 0.0f;
            eyeAnglesPerPlayer[i] = {};
            spottedFlags[i] = 0;
            spottedMasks[i][0] = 0;
            spottedMasks[i][1] = 0;
        }
        if (!pawns[i]) {
            teams[i] = 0;
            s_cachedCoreTeamPawns[i] = 0;
            s_lastCoreTeamReadUs[i] = 0;
            continue;
        }
        if (s_cachedCoreTeamPawns[i] != pawns[i]) {
            s_cachedCoreTeams[i] = 0;
            s_lastCoreTeamReadUs[i] = 0;
            s_cachedCoreTeamPawns[i] = pawns[i];
        }
        teams[i] = s_cachedCoreTeams[i];
    }
    const bool hasSpottedStateOffsets =
        ofs.C_CSPlayerPawn_m_entitySpottedState > 0 &&
        ofs.EntitySpottedState_t_m_bSpottedByMask > 0;
    const std::ptrdiff_t spottedFlagOffset =
        hasSpottedStateOffsets ? (ofs.EntitySpottedState_t_m_bSpottedByMask - 4) : -1;
    auto teamLooksValid = [](int teamValue) -> bool {
        return teamValue == 2 || teamValue == 3;
    };
    #include "player_parts/player_core_reads.inl"
    _playerCoreUs = TickNowUs() - _playerCoreStartUs;
    const uint64_t _playerRepairStartUs = TickNowUs();
    #include "player_parts/player_repair.inl"
    _playerRepairUs = TickNowUs() - _playerRepairStartUs;
