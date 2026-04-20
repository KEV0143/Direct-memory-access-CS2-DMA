    {
        static uint64_t s_coreRepairResetSerial = 0;
        static uint8_t s_coreRepairStreaks[64] = {};
        static uint64_t s_lastCoreRepairAttemptUs[64] = {};
        static uint32_t s_partialCoreStreak = 0;
        static uint64_t s_lastPartialCoreRecoveryUs = 0;
        const uint64_t coreRepairResetSerial = s_sceneResetSerial.load(std::memory_order_relaxed);
        if (s_coreRepairResetSerial != coreRepairResetSerial) {
            s_coreRepairResetSerial = coreRepairResetSerial;
            memset(s_coreRepairStreaks, 0, sizeof(s_coreRepairStreaks));
            memset(s_lastCoreRepairAttemptUs, 0, sizeof(s_lastCoreRepairAttemptUs));
            s_partialCoreStreak = 0;
            s_lastPartialCoreRecoveryUs = 0;
        }
        const int repairSlotLimit = std::max(
            playerSlotScanLimit,
            std::clamp(s_playerHierarchyHighWaterSlot.load(std::memory_order_relaxed), 0, 64));
        for (int i = 0; i < repairSlotLimit; ++i) {
            if (pawns[i])
                continue;
            s_coreRepairStreaks[i] = 0;
            s_lastCoreRepairAttemptUs[i] = 0;
        }

        int pawnCount = 0;
        int saneCoreCount = 0;
        const uint64_t coreRepairNowUs = TickNowUs();
        const auto sceneWarmupState =
            static_cast<esp::SceneWarmupState>(s_sceneWarmupState.load(std::memory_order_relaxed));
        const bool allowStructuralEviction =
            !sceneSettling &&
            sceneWarmupState == esp::SceneWarmupState::Stable &&
            s_engineInGame.load(std::memory_order_relaxed);
        const uint64_t recentResetUs = s_lastSceneResetUs.load(std::memory_order_relaxed);
        const bool recentStructuralReset =
            recentResetUs > 0 &&
            coreRepairNowUs > recentResetUs &&
            (coreRepairNowUs - recentResetUs) <= 4000000u;
        const uint64_t kCoreRepairRetryIntervalUs = recentStructuralReset ? 8000u : 16000u;
        const uint8_t coreRepairThreshold = (sceneSettling || recentStructuralReset) ? 1u : 2u;
        const int coreMissingTolerance = recentStructuralReset ? 0 : 1;
        const uint32_t partialCoreThreshold = recentStructuralReset ? 24u : 60u;
        const uint32_t partialCoreCadence = recentStructuralReset ? 12u : 30u;
        
        bool anyCoreRepairNeeded = false;
        for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
            const int i = playerResolvedSlots[resolvedIdx];
            if (!pawns[i]) {
                s_coreRepairStreaks[i] = 0;
                s_lastCoreRepairAttemptUs[i] = 0;
                continue;
            }
            ++pawnCount;
            if (coreStateLooksSane(i)) {
                s_coreRepairStreaks[i] = 0;
                continue;
            }
            if (s_coreRepairStreaks[i] < 255u)
                ++s_coreRepairStreaks[i];
            
            
            
            
            if (allowStructuralEviction && s_coreRepairStreaks[i] >= 30u) {
                s_stalePawnEvictionQueue[i] = true;
                pawns[i] = 0;
                s_coreRepairStreaks[i] = 0;
                continue;
            }
            if (s_coreRepairStreaks[i] < coreRepairThreshold)
                continue;
            if (s_lastCoreRepairAttemptUs[i] > 0 &&
                (coreRepairNowUs - s_lastCoreRepairAttemptUs[i]) < kCoreRepairRetryIntervalUs)
                continue;
            s_lastCoreRepairAttemptUs[i] = coreRepairNowUs;
            mem.AddScatterReadRequest(handle, pawns[i] + ofs.C_BaseEntity_m_iHealth, &healths[i], sizeof(int));
            mem.AddScatterReadRequest(handle, pawns[i] + ofs.C_CSPlayerPawn_m_ArmorValue, &armors[i], sizeof(int));
            if (coreTeamRefreshDueAt(coreRepairNowUs, i)) {
                mem.AddScatterReadRequest(handle, pawns[i] + ofs.C_BaseEntity_m_iTeamNum, &teams[i], sizeof(int));
                coreTeamReadsQueued[i] = true;
            }
            mem.AddScatterReadRequest(handle, pawns[i] + ofs.C_BaseEntity_m_lifeState, &lifeStates[i], sizeof(uint8_t));
            mem.AddScatterReadRequest(handle, pawns[i] + ofs.C_BasePlayerPawn_m_vOldOrigin, &positions[i], sizeof(Vector3));
            anyCoreRepairNeeded = true;
        }
        if (anyCoreRepairNeeded)
            executeOptionalScatterRead();
        for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
            const int i = playerResolvedSlots[resolvedIdx];
            if (pawns[i] && coreStateLooksSane(i)) {
                s_coreRepairStreaks[i] = 0;
                ++saneCoreCount;
            }
        }

        const bool localCoreEvidence =
            localPawn != 0 ||
            (localControllerPawnHandle != 0u && localControllerPawnHandle != 0xFFFFFFFFu);
        const bool coreLooksPartial =
            pawnCount >= 2 &&
            saneCoreCount + coreMissingTolerance < pawnCount;
        if (coreLooksPartial && localCoreEvidence) {
            ++s_partialCoreStreak;
            if (!sceneSettling &&
                s_partialCoreStreak >= partialCoreThreshold &&
                (s_partialCoreStreak % partialCoreCadence) == 0u) {
                refreshDmaCaches(
                    "partial_player_core",
                    recentStructuralReset ? DmaRefreshTier::Repair : DmaRefreshTier::Probe);
            }
        } else {
            s_partialCoreStreak = 0;
        }
    }
