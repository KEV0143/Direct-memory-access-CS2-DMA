namespace {
    
    
    
    template <typename Clock, typename TimePoint>
    void PreciseSleepUntil(const TimePoint& target)
    {
        
        const auto sleepTarget = target - std::chrono::microseconds(900);
        const auto now = Clock::now();
        if (sleepTarget > now)
            std::this_thread::sleep_until(sleepTarget);
        
        while (Clock::now() < target)
            _mm_pause(); 
    }

    void DataWorkerLoop()
    {
        using Clock = std::chrono::steady_clock;
        const auto tickInterval = std::chrono::microseconds(1000000 / DATA_WORKER_HZ);
        auto nextTick = Clock::now() + JitterDuration(tickInterval, kDataWorkerJitterPercent);
        auto lastRecoveryAttempt = Clock::now() - std::chrono::seconds(10);

        while (!s_dataWorkerStopRequested.load(std::memory_order_relaxed)) {
            try {
                const auto cycleStart = Clock::now();
                const uint64_t cycleStartUs = TickNowUs();
                s_dataWorkerLastLoopStartUs.store(cycleStartUs, std::memory_order_relaxed);
                s_dataWorkerInFlightSinceUs.store(cycleStartUs, std::memory_order_relaxed);
                s_dataWorkerUpdateInFlight.store(true, std::memory_order_release);

                esp::UpdateData();

                const auto cycleEnd = Clock::now();
                const uint64_t cycleEndUs = TickNowUs();
                const uint64_t cycleUs = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(cycleEnd - cycleStart).count());

                s_dataWorkerLastLoopEndUs.store(cycleEndUs, std::memory_order_relaxed);
                s_dataWorkerUpdateInFlight.store(false, std::memory_order_release);
                s_dataWorkerCycleUs.store(cycleUs, std::memory_order_relaxed);

                
                uint64_t prevMax = s_dataWorkerMaxCycleUs.load(std::memory_order_relaxed);
                while (cycleUs > prevMax &&
                       !s_dataWorkerMaxCycleUs.compare_exchange_weak(prevMax, cycleUs, std::memory_order_relaxed))
                    ;
            } catch (...) {
                s_dataWorkerLastLoopEndUs.store(TickNowUs(), std::memory_order_relaxed);
                s_dataWorkerUpdateInFlight.store(false, std::memory_order_release);
                
                DmaLogPrintf("[ERROR] DataWorkerLoop: UpdateData exception caught, continuing");
                s_dmaConsecutiveFailures.fetch_add(1, std::memory_order_relaxed);
                s_dmaTotalFailures.fetch_add(1, std::memory_order_relaxed);
            }

            const auto now = Clock::now();
            const auto signOnState = s_engineSignOnState.load(std::memory_order_relaxed);
            static auto nonLiveSignOnSince = Clock::time_point::max();
            static auto lastNonLiveSignOnSpan = Clock::duration::zero();

            if (signOnState != 6) {
                if (nonLiveSignOnSince == Clock::time_point::max())
                    nonLiveSignOnSince = now;
            } else if (nonLiveSignOnSince != Clock::time_point::max()) {
                lastNonLiveSignOnSpan = now - nonLiveSignOnSince;
                nonLiveSignOnSince = Clock::time_point::max();
            }

            
            
            
            
            
            
            
            
            
            static int prevSignOnState = -1;
            if (prevSignOnState != 6 &&
                signOnState == 6 &&
                lastNonLiveSignOnSpan >= std::chrono::milliseconds(500)) {
                DmaLogPrintf("[INFO] signOnState re-entered live state (%d -> 6) after %lld ms; keeping caches intact",
                    prevSignOnState,
                    static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(lastNonLiveSignOnSpan).count()));
            }
            prevSignOnState = signOnState;

            const bool recoveryRequested = IsDmaRecoveryRequested();
            const bool missingBases = !g::clientBase || !g::engine2Base;
            const bool engineResolved = s_engineStatusResolved.load(std::memory_order_relaxed);
            const bool engineInGame = s_engineInGame.load(std::memory_order_relaxed);
            const bool engineMenu = s_engineMenu.load(std::memory_order_relaxed);
            
            
            
            
            
            
            
            
            const bool stuckInMenu =
                engineResolved &&
                engineMenu &&
                !engineInGame &&
                signOnState != 6 &&
                nonLiveSignOnSince != Clock::time_point::max() &&
                (now - nonLiveSignOnSince) >= std::chrono::milliseconds(3000);

            
            
            
            
            
            
            
            
            
            
            
            
            
            
            static auto zeroPlayerSince = Clock::time_point::max();
            static bool zeroPlayerWatchdogLogged = false;
            static auto zeroPopulationSince = Clock::time_point::max();
            static uint8_t zeroPopulationStage = 0;
            static uint64_t lastZeroPopulationHardUs = 0;
            bool populationWatchdogRecovery = false;
            {
                const uint64_t watchdogNowUs = TickNowUs();
                const uint64_t sceneResetUs = s_lastSceneResetUs.load(std::memory_order_relaxed);
                const uint64_t warmupEnteredUs = s_sceneWarmupEnteredUs.load(std::memory_order_relaxed);
                const bool localIdentityMissing =
                    s_localPawn == 0 &&
                    !s_localMaskResolved;
                const bool looksInGame =
                    engineResolved &&
                    engineInGame &&
                    signOnState == 6 &&
                    g::clientBase &&
                    g::engine2Base;
                const bool inGameLongEnough =
                    sceneResetUs > 0 &&
                    watchdogNowUs > sceneResetUs &&
                    (watchdogNowUs - sceneResetUs) >= 8000000u;

                const bool localIdentityMissingUnexpected =
                    looksInGame &&
                    inGameLongEnough &&
                    localIdentityMissing;

                if (localIdentityMissingUnexpected) {
                    if (zeroPlayerSince == Clock::time_point::max())
                        zeroPlayerSince = now;
                } else {
                    zeroPlayerSince = Clock::time_point::max();
                    zeroPlayerWatchdogLogged = false;
                }

                const int activePlayers = std::clamp(s_activePlayerCount.load(std::memory_order_relaxed), 0, 64);
                const int highestEntityIndex = std::max(0, s_highestEntityIdxStat.load(std::memory_order_relaxed));
                const int playerSlotBudget = std::clamp(s_playerSlotScanLimitStat.load(std::memory_order_relaxed), 0, 64);
                const int maxClients = std::clamp(s_engineMaxClients.load(std::memory_order_relaxed), 0, 256);
                const uint64_t sceneAgeUs =
                    sceneResetUs > 0 && watchdogNowUs >= sceneResetUs
                    ? watchdogNowUs - sceneResetUs
                    : 0;
                const uint64_t warmupAgeUs =
                    warmupEnteredUs > 0 && watchdogNowUs >= warmupEnteredUs
                    ? watchdogNowUs - warmupEnteredUs
                    : 0;
                const bool liveByEngine =
                    engineResolved &&
                    engineInGame &&
                    !engineMenu &&
                    signOnState == 6;
                const bool liveByShape =
                    signOnState == 6 &&
                    maxClients >= 2 &&
                    playerSlotBudget >= 32 &&
                    g::clientBase &&
                    g::engine2Base;
                const bool definitelyMenu =
                    engineResolved &&
                    engineMenu &&
                    !engineInGame &&
                    signOnState != 6;
                const bool livePopulationExpected =
                    !definitelyMenu &&
                    (liveByEngine || liveByShape) &&
                    g::clientBase &&
                    g::engine2Base;
                const bool suspiciousFlatEntityRange =
                    maxClients >= 2 &&
                    playerSlotBudget >= 32 &&
                    highestEntityIndex > 0 &&
                    highestEntityIndex < 32;
                const bool zeroPopulationObserved =
                    livePopulationExpected &&
                    activePlayers == 0;
                const bool zeroPopulationGraceElapsed =
                    sceneAgeUs >= 1800000u ||
                    warmupAgeUs >= 1800000u ||
                    suspiciousFlatEntityRange ||
                    localIdentityMissing;

                if (zeroPopulationObserved) {
                    if (zeroPopulationSince == Clock::time_point::max())
                        zeroPopulationSince = now;

                    const auto zeroAge = now - zeroPopulationSince;
                    const bool canAct = zeroPopulationGraceElapsed && !s_dmaRecovering.load(std::memory_order_relaxed);
                    if (canAct && zeroPopulationStage < 1u && zeroAge >= std::chrono::milliseconds(900)) {
                        zeroPopulationStage = 1u;
                        SetSceneWarmupState(esp::SceneWarmupState::HierarchyWarming, watchdogNowUs);
                        RefreshDmaCaches(
                            suspiciousFlatEntityRange ? "zero_players_flat_entity_probe" : "zero_players_live_probe",
                            DmaRefreshTier::Probe);
                    }
                    if (canAct && zeroPopulationStage < 2u && zeroAge >= std::chrono::milliseconds(2400)) {
                        zeroPopulationStage = 2u;
                        ResetRuntimeState(true);
                        SetSceneWarmupState(esp::SceneWarmupState::HierarchyWarming, watchdogNowUs);
                        RefreshDmaCaches(
                            suspiciousFlatEntityRange ? "zero_players_flat_entity_repair" : "zero_players_live_repair",
                            DmaRefreshTier::Repair,
                            true);
                    }
                    if (canAct && zeroPopulationStage < 3u && zeroAge >= std::chrono::milliseconds(5200)) {
                        zeroPopulationStage = 3u;
                        ResetRuntimeState(true);
                        SetSceneWarmupState(
                            suspiciousFlatEntityRange ? esp::SceneWarmupState::HierarchyWarming : esp::SceneWarmupState::Recovery,
                            watchdogNowUs);
                        RefreshDmaCaches(
                            suspiciousFlatEntityRange ? "zero_players_flat_entity_full" : "zero_players_live_full",
                            DmaRefreshTier::Full,
                            true);
                        if (!suspiciousFlatEntityRange) {
                            RequestDmaRecovery("zero_players_live_persistent");
                            populationWatchdogRecovery = true;
                        }
                        lastZeroPopulationHardUs = watchdogNowUs;
                    } else if (canAct &&
                               !suspiciousFlatEntityRange &&
                               zeroPopulationStage >= 3u &&
                               zeroAge >= std::chrono::milliseconds(9000) &&
                               (lastZeroPopulationHardUs == 0 ||
                                watchdogNowUs <= lastZeroPopulationHardUs ||
                                (watchdogNowUs - lastZeroPopulationHardUs) >= 8000000u)) {
                        lastZeroPopulationHardUs = watchdogNowUs;
                        SetSceneWarmupState(esp::SceneWarmupState::Recovery, watchdogNowUs);
                        RequestDmaRecovery("zero_players_live_retry");
                        populationWatchdogRecovery = true;
                    }
                } else {
                    zeroPopulationSince = Clock::time_point::max();
                    zeroPopulationStage = 0;
                    if (activePlayers > 0)
                        lastZeroPopulationHardUs = 0;
                }
            }
            const bool localIdentityWatchdog =
                zeroPlayerSince != Clock::time_point::max() &&
                (now - zeroPlayerSince) > std::chrono::seconds(8);

            const bool recoveryRequestedNow = recoveryRequested || populationWatchdogRecovery || IsDmaRecoveryRequested();
            if (recoveryRequestedNow || missingBases || localIdentityWatchdog) {
                if (localIdentityWatchdog) {
                    if (!zeroPlayerWatchdogLogged) {
                        DmaLogPrintf("[INFO] Local identity missing for 8s, re-attaching DMA...");
                        zeroPlayerWatchdogLogged = true;
                    }
                    zeroPlayerSince = Clock::time_point::max();
                }
                const auto recoveryInterval =
                    recoveryRequestedNow
                        ? std::chrono::milliseconds(3000)
                        : missingBases
                            ? std::chrono::milliseconds(3000)
                            : std::chrono::milliseconds(2000);
                if (now - lastRecoveryAttempt > recoveryInterval) {
                    lastRecoveryAttempt = now;
                    if (TryRecoverDma()) {
                        ClearDmaRecoveryRequest();
                        ResetRuntimeState(true);
                    }
                }
            } else if (stuckInMenu && (now - lastRecoveryAttempt) > std::chrono::seconds(2)) {
                lastRecoveryAttempt = now;
                if (mem.vHandle) {
                    VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_FREQ_TLB_PARTIAL, 1);
                    VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_FREQ_FAST, 1);
                }
            }

            nextTick += JitterDuration(tickInterval, kDataWorkerJitterPercent);
            PreciseSleepUntil<Clock>(nextTick);

            const auto postSleepNow = Clock::now();
            if (postSleepNow > nextTick + std::chrono::milliseconds(50))
                nextTick = postSleepNow;
        }

        s_dataWorkerUpdateInFlight.store(false, std::memory_order_release);
        s_dataWorkerRunning.store(false, std::memory_order_relaxed);
    }
}
