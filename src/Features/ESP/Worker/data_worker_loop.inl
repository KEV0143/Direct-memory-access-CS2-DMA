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
                esp::RequestCacheRefresh();
                DmaLogPrintf("[INFO] Map entry detected (signOnState %d -> 6), flushing caches", prevSignOnState);
            }
            prevSignOnState = signOnState;

            const bool recoveryRequested = IsDmaRecoveryRequested();
            const bool missingBases = !g::clientBase || !g::engine2Base;
            
            
            
            
            
            
            
            
            const bool stuckInMenu =
                signOnState != 6 &&
                nonLiveSignOnSince != Clock::time_point::max() &&
                (now - nonLiveSignOnSince) >= std::chrono::milliseconds(1500);

            
            
            
            
            
            
            
            
            
            
            
            
            
            
            static auto zeroPlayerSince = Clock::time_point::max();
            static bool zeroPlayerWatchdogLogged = false;
            {
                const int activePlayers = s_activePlayerCount.load(std::memory_order_relaxed);
                const bool looksInGame = (signOnState == 6) && g::clientBase && g::engine2Base;
                if (looksInGame && activePlayers == 0) {
                    if (zeroPlayerSince == Clock::time_point::max())
                        zeroPlayerSince = now;
                } else {
                    zeroPlayerSince = Clock::time_point::max();
                    zeroPlayerWatchdogLogged = false;
                }
            }
            const bool zeroPlayerWatchdog =
                zeroPlayerSince != Clock::time_point::max() &&
                (now - zeroPlayerSince) > std::chrono::seconds(3);

            if (recoveryRequested || missingBases || stuckInMenu || zeroPlayerWatchdog) {
                if (zeroPlayerWatchdog) {
                    if (!zeroPlayerWatchdogLogged) {
                        DmaLogPrintf("[INFO] Stale memory detected, re-attaching DMA...");
                        zeroPlayerWatchdogLogged = true;
                    }
                    zeroPlayerSince = Clock::time_point::max();
                }
                const auto recoveryInterval =
                    (recoveryRequested || missingBases || zeroPlayerWatchdog)
                        ? std::chrono::milliseconds(500)
                        : std::chrono::seconds(2);
                if (now - lastRecoveryAttempt > recoveryInterval) {
                    lastRecoveryAttempt = now;
                    if (TryRecoverDma())
                        ClearDmaRecoveryRequest();
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
