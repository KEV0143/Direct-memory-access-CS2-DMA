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
        SetSceneWarmupState(state);
    };

    auto clearAllState = [&](bool publishClearedSnapshot = false) {
        ResetRuntimeState(publishClearedSnapshot);
    };

    auto enterWaitCs2 = [&]() {
        const auto prevStatus = static_cast<esp::GameStatus>(s_gameStatus.load(std::memory_order_relaxed));
        const uint64_t nowUs = TickNowUs();
        const auto warmupState = static_cast<esp::SceneWarmupState>(s_sceneWarmupState.load(std::memory_order_relaxed));
        const bool recentColdAttachReset =
            prevStatus == esp::GameStatus::WaitCs2 &&
            warmupState == esp::SceneWarmupState::ColdAttach &&
            s_lastSceneResetUs.load(std::memory_order_relaxed) > 0 &&
            nowUs > s_lastSceneResetUs.load(std::memory_order_relaxed) &&
            (nowUs - s_lastSceneResetUs.load(std::memory_order_relaxed)) < 2000000u;
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
        if (!recentColdAttachReset)
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
        RefreshDmaCaches(reason, tier, force);
    };

    auto handleSceneTransition = [&](const char* reason, bool bumpMapEpoch, bool publishClearedSnapshot = true) {
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
        const bool matchExit =
            reason &&
            strcmp(reason, "engine_match_exit") == 0;
        if (matchExit)
            refreshDmaCaches(reason, DmaRefreshTier::Probe, false);
        else
            refreshDmaCaches(reason, DmaRefreshTier::Full, true);
    };

    if (!g::clientBase || !g::engine2Base) {
        static uint64_t s_missingPidSinceUs = 0;
        static uint64_t s_lastBaseRecoveryRequestUs = 0;
        const uint64_t bootstrapNowUs = TickNowUs();
        const bool recoveryInProgress =
            s_dmaRecovering.load(std::memory_order_relaxed) ||
            IsDmaRecoveryRequested();
        const DWORD liveCs2Pid = mem.GetPidFromName("cs2.exe");
        if (!liveCs2Pid) {
            if (!recoveryInProgress) {
                if (s_missingPidSinceUs == 0)
                    s_missingPidSinceUs = bootstrapNowUs;
                if ((bootstrapNowUs - s_missingPidSinceUs) >= 3000000u)
                    enterWaitCs2();
            }
            MarkDmaReadSuccess();
            return false;
        }
        s_missingPidSinceUs = 0;

        promoteInGameFrame("cs2_pid_present_bases_pending");
        if (!recoveryInProgress &&
            (s_lastBaseRecoveryRequestUs == 0 ||
             (bootstrapNowUs - s_lastBaseRecoveryRequestUs) >= 1500000u)) {
            s_lastBaseRecoveryRequestUs = bootstrapNowUs;
            RequestDmaRecovery("module_bases_missing_with_pid");
        }
        MarkDmaReadSuccess();
        return false;
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
