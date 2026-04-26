    {
        static uint64_t s_coreRepairResetSerial = 0;
        static uint8_t s_coreRepairStreaks[64] = {};
        static uint64_t s_coreRepairBadSinceUs[64] = {};
        static uint64_t s_lastCoreRepairAttemptUs[64] = {};
        static uint32_t s_partialCoreStreak = 0;
        static uint64_t s_partialCoreSinceUs = 0;
        static uint64_t s_lastPartialCoreRefreshUs = 0;
        static uint64_t s_lastPartialCoreBadMask = 0;
        static uint32_t s_partialCoreConfirmedStreak = 0;
        const uint64_t coreRepairResetSerial = s_sceneResetSerial.load(std::memory_order_relaxed);
        if (s_coreRepairResetSerial != coreRepairResetSerial) {
            s_coreRepairResetSerial = coreRepairResetSerial;
            memset(s_coreRepairStreaks, 0, sizeof(s_coreRepairStreaks));
            memset(s_coreRepairBadSinceUs, 0, sizeof(s_coreRepairBadSinceUs));
            memset(s_lastCoreRepairAttemptUs, 0, sizeof(s_lastCoreRepairAttemptUs));
            s_partialCoreStreak = 0;
            s_partialCoreSinceUs = 0;
            s_lastPartialCoreRefreshUs = 0;
            s_lastPartialCoreBadMask = 0;
            s_partialCoreConfirmedStreak = 0;
        }
        const int repairSlotLimit = std::max(
            playerSlotScanLimit,
            std::clamp(s_playerHierarchyHighWaterSlot.load(std::memory_order_relaxed), 0, 64));
        for (int i = 0; i < repairSlotLimit; ++i) {
            if (pawns[i])
                continue;
            s_coreRepairStreaks[i] = 0;
            s_coreRepairBadSinceUs[i] = 0;
            s_lastCoreRepairAttemptUs[i] = 0;
        }

        int pawnCount = 0;
        int saneCoreCount = 0;
        uint64_t partialCoreBadMask = 0;
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
        
        bool anyCoreRepairNeeded = false;
        for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
            const int i = playerResolvedSlots[resolvedIdx];
            if (!pawns[i]) {
                s_coreRepairStreaks[i] = 0;
                s_coreRepairBadSinceUs[i] = 0;
                s_lastCoreRepairAttemptUs[i] = 0;
                continue;
            }
            ++pawnCount;
            if (coreStateLooksSane(i)) {
                s_coreRepairStreaks[i] = 0;
                s_coreRepairBadSinceUs[i] = 0;
                continue;
            }
            partialCoreBadMask |= (1ull << static_cast<unsigned>(i));
            if (s_coreRepairBadSinceUs[i] == 0)
                s_coreRepairBadSinceUs[i] = coreRepairNowUs;
            if (s_coreRepairStreaks[i] < 255u)
                ++s_coreRepairStreaks[i];
            
            
            
            
            const uint64_t repairBadAgeUs =
                coreRepairNowUs >= s_coreRepairBadSinceUs[i]
                ? coreRepairNowUs - s_coreRepairBadSinceUs[i]
                : 0;
            if (allowStructuralEviction &&
                s_coreRepairStreaks[i] >= 160u &&
                repairBadAgeUs >= 1800000u) {
                s_stalePawnEvictionQueue[i] = true;
                pawns[i] = 0;
                s_coreRepairStreaks[i] = 0;
                s_coreRepairBadSinceUs[i] = 0;
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
                s_coreRepairBadSinceUs[i] = 0;
                coreReadFresh[i] = true;
                coreReadPlausible[i] = true;
                coreReadAlive[i] = healths[i] > 0 && lifeStates[i] == 0;
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
            if (s_partialCoreSinceUs == 0)
                s_partialCoreSinceUs = coreRepairNowUs;
            const bool repeatedBadSlots =
                s_lastPartialCoreBadMask != 0 &&
                partialCoreBadMask != 0 &&
                ((partialCoreBadMask & s_lastPartialCoreBadMask) != 0);
            if (repeatedBadSlots) {
                if (s_partialCoreConfirmedStreak < 0xFFFFFFFFu)
                    ++s_partialCoreConfirmedStreak;
            } else {
                s_partialCoreConfirmedStreak = 0;
            }
            s_lastPartialCoreBadMask = partialCoreBadMask;
            const uint64_t partialAgeUs =
                coreRepairNowUs >= s_partialCoreSinceUs
                ? coreRepairNowUs - s_partialCoreSinceUs
                : 0;
            const bool refreshCooldownElapsed =
                s_lastPartialCoreRefreshUs == 0 ||
                coreRepairNowUs <= s_lastPartialCoreRefreshUs ||
                (coreRepairNowUs - s_lastPartialCoreRefreshUs) >= 3500000u;
            const bool confirmedPartial =
                !sceneSettling &&
                repeatedBadSlots &&
                partialCoreBadMask != 0 &&
                s_partialCoreConfirmedStreak >= 12u &&
                partialAgeUs >= (recentStructuralReset ? 550000u : 1200000u);
            if (!sceneSettling &&
                refreshCooldownElapsed &&
                s_partialCoreStreak >= partialCoreThreshold &&
                confirmedPartial) {
                s_lastPartialCoreRefreshUs = coreRepairNowUs;
                refreshDmaCaches(
                    "partial_player_core_confirmed",
                    DmaRefreshTier::Repair);
            }
        } else {
            s_partialCoreStreak = 0;
            s_partialCoreSinceUs = 0;
            s_lastPartialCoreBadMask = 0;
            s_partialCoreConfirmedStreak = 0;
        }
    }
