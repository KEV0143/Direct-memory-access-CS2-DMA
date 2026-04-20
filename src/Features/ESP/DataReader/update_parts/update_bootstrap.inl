    static uint32_t s_requiredReadFailureCount = 0;

    auto statusToString = [](esp::GameStatus status) -> const char* {
        switch (status) {
        case esp::GameStatus::Ok:      return "OK";
        case esp::GameStatus::WaitCs2: return "Wait cs2.exe";
        default:                       return "Unknown";
        }
    };

    auto logStatusTransition = [&](esp::GameStatus from, esp::GameStatus to, const char* reason) {
        if (from == to)
            return;
        DmaLogPrintf(
            "[INFO] GameStatus: %s -> %s (%s)",
            statusToString(from),
            statusToString(to),
            reason ? reason : "no-reason");
    };

    auto setSceneWarmupState = [&](esp::SceneWarmupState state) {
        s_sceneWarmupState.store(static_cast<uint8_t>(state), std::memory_order_relaxed);
        s_sceneWarmupEnteredUs.store(TickNowUs(), std::memory_order_relaxed);
    };

    enum class DmaRefreshTier : uint8_t {
        Probe = 0,
        Repair,
        Full,
    };

    auto clearAllState = [&](bool publishClearedSnapshot = false) {
        s_sceneResetSerial.fetch_add(1, std::memory_order_relaxed);
        s_lastSceneResetUs.store(TickNowUs(), std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lock(s_dataMutex);
            for (int i = 0; i < 64; ++i) {
                s_players[i] = {};
                s_prevPlayers[i] = {};
                s_webRadarPlayers[i] = {};
                s_playerLastSeenMs[i] = 0;
                s_playerInvalidReadStreak[i] = 0;
                s_playerDeathConfirmCount[i] = 0;
                s_prevRawPlayerPos[i] = {};
                s_prevRawPlayerPosReady[i] = false;
            }
            s_bombState = {};
            s_worldMarkerCount = 0;
            s_localPawn = 0;
            memset(s_localName, 0, sizeof(s_localName));
            s_localIsDead = false;
            s_localHealth = 0;
            s_localArmor = 0;
            s_localMoney = 0;
            s_localHasBomb = false;
            s_localHasDefuser = false;
            s_localGrenadeCount = 0;
            memset(s_localGrenadeIds, 0, sizeof(s_localGrenadeIds));
            s_localWeaponId = 0;
            s_localAmmoClip = -1;
            s_localPlayerIndex = -1;
            s_localTeam = 0;
            s_localPos = {};
            s_prevLocalPos = {};
            s_viewAngles = {};
            memset(&s_viewMatrix, 0, sizeof(s_viewMatrix));
            s_captureTimeUs = 0;
            s_prevCaptureTimeUs = 0;
            s_minimapMins = {};
            s_minimapMaxs = {};
            s_hasMinimapBounds = false;
            s_localMaskResolved = false;
            s_lastWorldScanUs = 0;
            s_activePlayerCount.store(0, std::memory_order_relaxed);
            s_playerSlotScanLimitStat.store(64, std::memory_order_relaxed);
            s_playerHierarchyHighWaterSlot.store(0, std::memory_order_relaxed);
            s_highestEntityIdxStat.store(0, std::memory_order_relaxed);
            s_worldMarkerCountStat.store(0, std::memory_order_relaxed);
            s_lastWorldScanCommittedUs.store(0, std::memory_order_relaxed);
            s_bombDebugFlags.store(0, std::memory_order_relaxed);
            s_bombDebugSourceFlags.store(0, std::memory_order_relaxed);
            s_bombDebugConfidence.store(0, std::memory_order_relaxed);
            s_stageWorldScanUs.store(0, std::memory_order_relaxed);
            s_stageWorldScanLastUs.store(0, std::memory_order_relaxed);
            s_stagePlayerHierarchyUs.store(0, std::memory_order_relaxed);
            s_stagePlayerCoreUs.store(0, std::memory_order_relaxed);
            s_stagePlayerRepairUs.store(0, std::memory_order_relaxed);
            s_stagePlayerAuxUs.store(0, std::memory_order_relaxed);
            s_stageBoneReadsUs.store(0, std::memory_order_relaxed);
            s_stagePlayerAuxLastUs.store(0, std::memory_order_relaxed);
            s_stageInventoryLastUs.store(0, std::memory_order_relaxed);
            s_stageBoneReadsLastUs.store(0, std::memory_order_relaxed);
            s_stagePlayerAuxLastAtUs.store(0, std::memory_order_relaxed);
            s_stageInventoryLastAtUs.store(0, std::memory_order_relaxed);
            s_stageBoneReadsLastAtUs.store(0, std::memory_order_relaxed);
            s_cameraWorkerCycleUs.store(0, std::memory_order_relaxed);
            s_cameraWorkerMaxCycleUs.store(0, std::memory_order_relaxed);
            memset(s_worldSmokeSubclassIds, 0, sizeof(s_worldSmokeSubclassIds));
            memset(s_worldMolotovSubclassIds, 0, sizeof(s_worldMolotovSubclassIds));
            memset(s_worldDecoySubclassIds, 0, sizeof(s_worldDecoySubclassIds));
            memset(s_worldHeSubclassIds, 0, sizeof(s_worldHeSubclassIds));
            memset(s_worldInfernoSubclassIds, 0, sizeof(s_worldInfernoSubclassIds));
            memset(s_worldTrackedIndices, 0, sizeof(s_worldTrackedIndices));
            memset(s_worldTrackedIndexPos, 0, sizeof(s_worldTrackedIndexPos));
            s_worldTrackedIndexCount = 0;
            s_lastStableIntervalPerTick = 0.015625f;
            s_lastStableGameTime = 0.0f;
            s_lastStableGameTimeUs = 0;
            for (int i = 0; i <= kMaxTrackedWorldEntities; ++i) {
                s_worldEntityRefs[i] = 0;
                s_worldEntitySubclassIds[i] = 0;
                s_worldEntityItemIds[i] = 0;
                s_worldSmokeLatched[i] = false;
                s_worldInfernoLatched[i] = false;
                s_worldDecoyLatched[i] = false;
                s_worldExplosiveLatched[i] = false;
                s_worldUtilityHasHistory[i] = false;
                s_worldSmokeEvidenceCount[i] = 0;
                s_worldInfernoEvidenceCount[i] = 0;
                s_worldDecoyEvidenceCount[i] = 0;
                s_worldExplosiveEvidenceCount[i] = 0;
                s_worldPrevPos[i] = {};
                s_worldPrevSmokeTick[i] = 0;
                s_worldPrevSmokeActive[i] = 0;
                s_worldPrevSmokeVolumeDataReceived[i] = 0;
                s_worldPrevSmokeEffectSpawned[i] = 0;
                s_worldPrevInfernoTick[i] = 0;
                s_worldPrevInfernoLife[i] = 0.0f;
                s_worldPrevInfernoFireCount[i] = 0;
                s_worldPrevInfernoInPostEffect[i] = 0;
                s_worldPrevDecoyTick[i] = 0;
                s_worldPrevDecoyClientTick[i] = 0;
                s_worldPrevExplodeTick[i] = 0;
                s_worldPrevVelocity[i] = {};
            }

            const int writeIdx = 1 - s_readIdx.load(std::memory_order_relaxed);
            memset(&s_entityBuf[writeIdx], 0, sizeof(EntitySnapshot));
            if (publishClearedSnapshot)
                s_readIdx.store(writeIdx, std::memory_order_release);
        }
        ResetCameraSnapshot();
    };

    auto enterWaitCs2 = [&]() {
        const auto prevStatus = static_cast<esp::GameStatus>(s_gameStatus.load(std::memory_order_relaxed));
        s_requiredReadFailureCount = 0;
        s_engineStatusResolved.store(false, std::memory_order_relaxed);
        s_engineSignOnState.store(-1, std::memory_order_relaxed);
        s_engineLocalPlayerSlot.store(-1, std::memory_order_relaxed);
        s_engineMaxClients.store(0, std::memory_order_relaxed);
        s_engineBackgroundMap.store(false, std::memory_order_relaxed);
        s_engineMenu.store(false, std::memory_order_relaxed);
        s_engineInGame.store(false, std::memory_order_relaxed);
        s_gameStatus.store(static_cast<uint8_t>(esp::GameStatus::WaitCs2), std::memory_order_relaxed);
        logStatusTransition(prevStatus, esp::GameStatus::WaitCs2, "client_base_lost_or_dma_recovery");
        clearAllState(true);
        s_mapFingerprint = 0;
        setSceneWarmupState(esp::SceneWarmupState::ColdAttach);
        g::clientBase = 0;
        g::engine2Base = 0;
        MarkDmaReadSuccess();
    };

    auto promoteInGameFrame = [&](const char* reason) {
        const auto curStatus = static_cast<esp::GameStatus>(s_gameStatus.load(std::memory_order_relaxed));
        s_requiredReadFailureCount = 0;
        if (curStatus != esp::GameStatus::Ok) {
            s_gameStatus.store(static_cast<uint8_t>(esp::GameStatus::Ok), std::memory_order_relaxed);
            logStatusTransition(curStatus, esp::GameStatus::Ok, reason);
        }
    };

    auto refreshDmaCaches = [&](const char* reason, DmaRefreshTier tier = DmaRefreshTier::Probe, bool force = false) {
        static uint64_t s_lastProbeRefreshUs = 0;
        static uint64_t s_lastRepairRefreshUs = 0;
        static uint64_t s_lastFullRefreshUs = 0;
        const uint64_t nowUs = TickNowUs();
        if (tier == DmaRefreshTier::Full) {
            if (!force && s_lastFullRefreshUs > 0 && (nowUs - s_lastFullRefreshUs) < 200000u)
                return;
            s_lastFullRefreshUs = nowUs;
        } else if (tier == DmaRefreshTier::Repair) {
            if (!force && s_lastRepairRefreshUs > 0 && (nowUs - s_lastRepairRefreshUs) < 800000u)
                return;
            s_lastRepairRefreshUs = nowUs;
        } else {
            if (!force && s_lastProbeRefreshUs > 0 && (nowUs - s_lastProbeRefreshUs) < 2000000u)
                return;
            s_lastProbeRefreshUs = nowUs;
        }

        if (!mem.vHandle)
            return;

        switch (tier) {
        case DmaRefreshTier::Probe:
            VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_FREQ_FAST, 1);
            VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_FREQ_TLB_PARTIAL, 1);
            break;
        case DmaRefreshTier::Repair:
            VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_FREQ_FAST, 1);
            VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_FREQ_TLB, 1);
            VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_FREQ_MEM_PARTIAL, 1);
            VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_FREQ_MEDIUM, 1);
            break;
        case DmaRefreshTier::Full:
            VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_FREQ_MEM, 1);
            VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_FREQ_TLB, 1);
            VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_FREQ_MEDIUM, 1);
            VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_FREQ_FAST, 1);
            break;
        }

        const char* tierLabel =
            tier == DmaRefreshTier::Full ? "full" :
            tier == DmaRefreshTier::Repair ? "repair" :
            "probe";
        DmaLogPrintf("[INFO] DMA cache refresh (%s) [%s]", reason ? reason : "transition", tierLabel);
    };

    auto handleSceneTransition = [&](const char* reason, bool bumpMapEpoch, bool publishClearedSnapshot = true) {
        static uint64_t s_lastTransitionRecoveryMapEpoch = 0;
        clearAllState(publishClearedSnapshot);
        uint64_t transitionMapEpoch = s_mapEpoch.load(std::memory_order_relaxed);
        if (bumpMapEpoch) {
            transitionMapEpoch = s_mapEpoch.fetch_add(1, std::memory_order_relaxed) + 1u;
            s_mapFingerprint = 0;
            DmaLogPrintf(
                "[INFO] Scene transition: %s -> map epoch %llu",
                reason ? reason : "transition",
                static_cast<unsigned long long>(transitionMapEpoch));
        }
        setSceneWarmupState(esp::SceneWarmupState::SceneTransition);
        
        
        
        
        refreshDmaCaches(reason, DmaRefreshTier::Full, true);
        
        
        
        
        
        
        if (bumpMapEpoch && s_lastTransitionRecoveryMapEpoch != transitionMapEpoch) {
            s_lastTransitionRecoveryMapEpoch = transitionMapEpoch;
            RequestDmaRecovery(reason ? reason : "scene_transition_rebind");
        }
    };

    if (!g::clientBase || !g::engine2Base) {
        const DWORD liveCs2Pid = mem.GetPidFromName("cs2.exe");
        if (!liveCs2Pid) {
            enterWaitCs2();
            return false;
        }

        promoteInGameFrame("cs2_pid_present");
        if (mem.vHandle)
            VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_ALL, 1);
        const uintptr_t liveClientBase = mem.GetBaseDaddy("client.dll");
        const uintptr_t liveEngine2Base = mem.GetBaseDaddy("engine2.dll");
        bool basesReadable = false;
        if (liveClientBase && liveEngine2Base) {
            uint16_t mz = 0;
            if (mem.Read(liveClientBase, &mz, sizeof(mz)) && mz == 0x5A4D) {
                basesReadable = true;
            }
        }

        if (basesReadable) {
            g::clientBase = liveClientBase;
            g::engine2Base = liveEngine2Base;
        } else {
            g::clientBase = 0;
            g::engine2Base = 0;
        }
        if (g::clientBase && g::engine2Base)
            promoteInGameFrame("module_bases_resolved");
        else
        RequestDmaRecovery("module_bases_missing_with_pid");
        if (!g::clientBase || !g::engine2Base) {
            MarkDmaReadFailure();
            return false;
        }
    }
    const uint64_t _stagePipelineStart = TickNowUs();
    static bool s_cachedWebRadarConsumerDemand = false;
    static uint64_t s_lastWebRadarConsumerDemandCheckUs = 0;
    if (!g::webRadarEnabled) {
        s_cachedWebRadarConsumerDemand = false;
        s_lastWebRadarConsumerDemandCheckUs = 0;
    } else if (s_lastWebRadarConsumerDemandCheckUs == 0 ||
               (_stagePipelineStart - s_lastWebRadarConsumerDemandCheckUs) >= 250000u) {
        s_cachedWebRadarConsumerDemand = webradar::HasActiveConsumers();
        s_lastWebRadarConsumerDemandCheckUs = _stagePipelineStart;
    }
    const bool webRadarDemandActive = g::webRadarEnabled && s_cachedWebRadarConsumerDemand;

    promoteInGameFrame("client_attached");
