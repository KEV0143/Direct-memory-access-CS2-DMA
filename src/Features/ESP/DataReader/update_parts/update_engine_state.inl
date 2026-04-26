    const auto& ofs = runtime_offsets::Get();
    auto isLikelyGamePointer = [](uintptr_t ptr) -> bool {
        if (ptr < 0x10000ULL || ptr >= 0x00007FF000000000ULL)
            return false;
        return (ptr & (sizeof(uintptr_t) - 1u)) == 0u;
    };
    bool engineSignonResolved = false;
    bool engineSignonInGame = false;
    bool engineSignonMenu = false;
    int32_t engineSignonState = -1;
    int32_t engineLocalPlayerSlot = -1;
    int32_t engineMaxClients = 0;
    uint8_t engineBackgroundMap = 0;
    uintptr_t engine2Base = g::engine2Base;
    static bool s_cachedEngineResolved = false;
    static bool s_cachedEngineMenu = false;
    static bool s_cachedEngineInGame = false;
    static int32_t s_cachedEngineSignOnState = -1;
    static int32_t s_cachedEngineLocalPlayerSlot = -1;
    static int32_t s_cachedEngineMaxClients = 0;
    static uint8_t s_cachedEngineBackgroundMap = 0;
    static uint32_t s_engineResolveMissCount = 0;
    const char* engineDebugSource = "fresh";

    static uintptr_t s_cachedNetworkGameClient = 0;
    static uint32_t s_zeroNetworkGameClientConfirmCount = 0;
    static constexpr uint32_t kZeroNetworkGameClientConfirmations = 3;
    static constexpr uint32_t kEngineResolveMissThreshold = 24;

    auto tryResolveEngineState = [&]() -> bool {
        if (!engine2Base)
            engine2Base = mem.GetBaseDaddy("engine2.dll");
        if (!engine2Base || ofs.dwNetworkGameClient <= 0)
            return false;

        g::engine2Base = engine2Base;

        uintptr_t networkGameClient = 0;
        {
            auto engineHandle = mem.CreateScatterHandle();
            if (!engineHandle)
                return false;
            mem.AddScatterReadRequest(engineHandle, engine2Base + ofs.dwNetworkGameClient, &networkGameClient, sizeof(networkGameClient));
            const bool scatterOk = mem.ExecuteReadScatter(engineHandle);
            mem.CloseScatterHandle(engineHandle);
            if (!scatterOk)
                return false;
        }

        if (networkGameClient == 0) {
            uintptr_t directNetworkGameClient = 0;
            if (mem.Read(engine2Base + ofs.dwNetworkGameClient, &directNetworkGameClient, sizeof(directNetworkGameClient)))
                networkGameClient = directNetworkGameClient;
        }

        if (networkGameClient == 0) {
            const bool requireZeroConfirmation =
                s_cachedEngineResolved &&
                (s_cachedEngineInGame || s_cachedEngineSignOnState == 6);
            if (requireZeroConfirmation &&
                s_zeroNetworkGameClientConfirmCount < kZeroNetworkGameClientConfirmations) {
                ++s_zeroNetworkGameClientConfirmCount;
                return false;
            }
            s_zeroNetworkGameClientConfirmCount = 0;
            s_cachedNetworkGameClient = 0;
            engineSignonResolved = true;
            engineSignonMenu = true;
            engineSignonInGame = false;
            engineSignonState = 0;
            engineLocalPlayerSlot = -1;
            engineMaxClients = 0;
            engineBackgroundMap = 0;
            return true;
        }

        if (!isLikelyGamePointer(networkGameClient)) {
            return false;
        }
        s_zeroNetworkGameClientConfirmCount = 0;
        s_cachedNetworkGameClient = networkGameClient;

        const std::ptrdiff_t signOffsets[3] = {
            ofs.dwNetworkGameClient_signOnState,
            static_cast<std::ptrdiff_t>(0x230),
            static_cast<std::ptrdiff_t>(0x228)
        };
        int32_t signOnCandidates[3] = { -1, -1, -1 };
        int32_t rawLocalPlayerSlot = -1;
        int32_t rawMaxClients = 0;
        uint8_t rawBackgroundMap = 0;

        {
            auto engineHandle = mem.CreateScatterHandle();
            if (!engineHandle)
                return false;

            for (int i = 0; i < 3; ++i) {
                if (signOffsets[i] > 0)
                    mem.AddScatterReadRequest(engineHandle, networkGameClient + static_cast<uintptr_t>(signOffsets[i]), &signOnCandidates[i], sizeof(int32_t));
            }
            if (ofs.dwNetworkGameClient_localPlayer > 0)
                mem.AddScatterReadRequest(engineHandle, networkGameClient + ofs.dwNetworkGameClient_localPlayer, &rawLocalPlayerSlot, sizeof(int32_t));
            if (ofs.dwNetworkGameClient_maxClients > 0)
                mem.AddScatterReadRequest(engineHandle, networkGameClient + ofs.dwNetworkGameClient_maxClients, &rawMaxClients, sizeof(int32_t));
            if (ofs.dwNetworkGameClient_isBackgroundMap > 0)
                mem.AddScatterReadRequest(engineHandle, networkGameClient + ofs.dwNetworkGameClient_isBackgroundMap, &rawBackgroundMap, sizeof(uint8_t));

            const bool scatterOk = mem.ExecuteReadScatter(engineHandle);
            mem.CloseScatterHandle(engineHandle);
            if (!scatterOk)
                return false;
        }

        int32_t signOnValue = -1;
        bool signResolved = false;
        for (int i = 0; i < 3; ++i) {
            if (signOffsets[i] <= 0)
                continue;
            if (signOnCandidates[i] >= 0 && signOnCandidates[i] <= 12) {
                signOnValue = signOnCandidates[i];
                signResolved = true;
                break;
            }
        }
        if (!signResolved)
            return false;

        engineSignonResolved = true;
        engineSignonState = signOnValue;

        const bool localPlayerSlotResolved = rawLocalPlayerSlot >= -1 && rawLocalPlayerSlot <= 128;
        const bool maxClientsResolved = rawMaxClients >= 0 && rawMaxClients <= 256;
        const bool backgroundMapResolved = rawBackgroundMap <= 1;

        if (localPlayerSlotResolved)
            engineLocalPlayerSlot = rawLocalPlayerSlot;
        else if (s_cachedEngineResolved)
            engineLocalPlayerSlot = s_cachedEngineLocalPlayerSlot;

        if (maxClientsResolved)
            engineMaxClients = rawMaxClients;
        else if (s_cachedEngineResolved)
            engineMaxClients = s_cachedEngineMaxClients;

        if (backgroundMapResolved)
            engineBackgroundMap = rawBackgroundMap;
        else if (s_cachedEngineResolved)
            engineBackgroundMap = s_cachedEngineBackgroundMap;

        if (signOnValue == 6) {
            if (engineMaxClients <= 1 &&
                s_cachedEngineInGame &&
                s_cachedEngineMaxClients >= 2) {
                engineMaxClients = s_cachedEngineMaxClients;
            }

            if (engineMaxClients <= 1 && engineBackgroundMap == 0)
                engineMaxClients = 64;

            if (engineBackgroundMap == 0 && engineMaxClients >= 2) {
                engineSignonInGame = true;
                engineSignonMenu = false;
            } else if (engineMaxClients == 1 && !s_cachedEngineInGame) {
                engineSignonInGame = false;
                engineSignonMenu = true;
            } else if (s_cachedEngineInGame) {
                engineSignonInGame = true;
                engineSignonMenu = false;
            } else {
                engineSignonInGame = false;
                engineSignonMenu = false;
            }
        } else {
            engineSignonInGame = false;
            engineSignonMenu = true;
        }

        return true;
    };

    static uint64_t s_lastEngineResolveUs = 0;
    constexpr uint64_t kEngineResolveInGameIntervalUs = 12000;
    constexpr uint64_t kEngineResolveIdleIntervalUs = 30000;
    const uint64_t engineNowUs = TickNowUs();
    const uint64_t engineResolveIntervalUs =
        (s_cachedEngineResolved && s_cachedEngineInGame)
            ? kEngineResolveInGameIntervalUs
            : kEngineResolveIdleIntervalUs;
    const bool engineCacheFresh =
        s_cachedEngineResolved &&
        s_lastEngineResolveUs > 0 &&
        (engineNowUs - s_lastEngineResolveUs) < engineResolveIntervalUs;

    if (engineCacheFresh) {
        engineDebugSource = "throttled";
        engineSignonResolved = true;
        engineSignonMenu = s_cachedEngineMenu;
        engineSignonInGame = s_cachedEngineInGame;
        engineSignonState = s_cachedEngineSignOnState;
        engineLocalPlayerSlot = s_cachedEngineLocalPlayerSlot;
        engineMaxClients = s_cachedEngineMaxClients;
        engineBackgroundMap = s_cachedEngineBackgroundMap;
    } else if (tryResolveEngineState()) {
        s_cachedEngineResolved = true;
        s_cachedEngineMenu = engineSignonMenu;
        s_cachedEngineInGame = engineSignonInGame;
        s_cachedEngineSignOnState = engineSignonState;
        s_cachedEngineLocalPlayerSlot = engineLocalPlayerSlot;
        s_cachedEngineMaxClients = engineMaxClients;
        s_cachedEngineBackgroundMap = engineBackgroundMap;
        s_engineResolveMissCount = 0;
        s_lastEngineResolveUs = engineNowUs;
    } else if (s_cachedEngineResolved && s_engineResolveMissCount < kEngineResolveMissThreshold) {
        ++s_engineResolveMissCount;
        engineDebugSource = "cached";
        engineSignonResolved = true;
        engineSignonMenu = s_cachedEngineMenu;
        engineSignonInGame = s_cachedEngineInGame;
        engineSignonState = s_cachedEngineSignOnState;
        engineLocalPlayerSlot = s_cachedEngineLocalPlayerSlot;
        engineMaxClients = s_cachedEngineMaxClients;
        engineBackgroundMap = s_cachedEngineBackgroundMap;
    } else {
        engineDebugSource = "none";
        s_engineResolveMissCount = std::min(s_engineResolveMissCount + 1, 255u);
        if (s_engineResolveMissCount >= kEngineResolveMissThreshold) {
            s_cachedEngineResolved = false;
            s_cachedEngineMenu = false;
            s_cachedEngineInGame = false;
            s_cachedEngineSignOnState = -1;
            s_cachedEngineLocalPlayerSlot = -1;
            s_cachedEngineMaxClients = 0;
            s_cachedEngineBackgroundMap = 0;
            s_cachedNetworkGameClient = 0;
            s_zeroNetworkGameClientConfirmCount = 0;
        }
    }
    s_engineStatusResolved.store(engineSignonResolved, std::memory_order_relaxed);
    s_engineSignOnState.store(engineSignonState, std::memory_order_relaxed);
    s_engineLocalPlayerSlot.store(engineLocalPlayerSlot, std::memory_order_relaxed);
    s_engineMaxClients.store(engineMaxClients, std::memory_order_relaxed);
    s_engineBackgroundMap.store(engineBackgroundMap != 0, std::memory_order_relaxed);
    s_engineMenu.store(engineSignonMenu, std::memory_order_relaxed);
    s_engineInGame.store(engineSignonInGame, std::memory_order_relaxed);
    {
        static bool s_lastEngineResolved = false;
        static bool s_lastEngineMenu = false;
        static bool s_lastEngineInGame = false;
        static int32_t s_lastEngineSignOnState = -1;
        static int32_t s_lastEngineLocalPlayerSlot = -1;
        static int32_t s_lastEngineMaxClients = 0;
        static uint8_t s_lastEngineBackgroundMap = 0;
        if (engineSignonResolved != s_lastEngineResolved ||
            engineSignonMenu != s_lastEngineMenu ||
            engineSignonInGame != s_lastEngineInGame ||
            engineSignonState != s_lastEngineSignOnState ||
            engineLocalPlayerSlot != s_lastEngineLocalPlayerSlot ||
            engineMaxClients != s_lastEngineMaxClients ||
            engineBackgroundMap != s_lastEngineBackgroundMap) {
            DmaLogPrintf(
                "[DEBUG] EngineStatus: source=%s resolved=%d menu=%d ingame=%d signOn=%d localSlot=%d maxClients=%d bg=%d engine2=0x%llX",
                engineDebugSource,
                engineSignonResolved ? 1 : 0,
                engineSignonMenu ? 1 : 0,
                engineSignonInGame ? 1 : 0,
                engineSignonState,
                engineLocalPlayerSlot,
                engineMaxClients,
                engineBackgroundMap ? 1 : 0,
                static_cast<unsigned long long>(engine2Base));
            s_lastEngineResolved = engineSignonResolved;
            s_lastEngineMenu = engineSignonMenu;
            s_lastEngineInGame = engineSignonInGame;
            s_lastEngineSignOnState = engineSignonState;
            s_lastEngineLocalPlayerSlot = engineLocalPlayerSlot;
            s_lastEngineMaxClients = engineMaxClients;
            s_lastEngineBackgroundMap = engineBackgroundMap;
        }
    }

    const bool engineMatchLike =
        engineSignonResolved &&
        engineSignonState == 6 &&
        engineMaxClients >= 2 &&
        engineBackgroundMap == 0;
    {
        static bool s_prevEngineMatchLike = false;
        static uintptr_t s_prevNetworkGameClient = 0;
        static bool s_pendingSceneTransition = false;
        static bool s_pendingSceneTransitionMatchLike = false;
        static uintptr_t s_pendingSceneTransitionNetworkClient = 0;
        static uint32_t s_pendingSceneTransitionCount = 0;
        
        
        
        
        
        
        
        
        
        const bool matchLikeChanged = (engineMatchLike != s_prevEngineMatchLike);
        const bool networkClientChanged =
            s_prevNetworkGameClient != 0 &&
            s_cachedNetworkGameClient != 0 &&
            s_cachedNetworkGameClient != s_prevNetworkGameClient;
        bool sceneTransition = matchLikeChanged || networkClientChanged;
        const bool requiresTransitionConfirmation =
            (matchLikeChanged && !engineMatchLike) ||
            networkClientChanged;

        if (sceneTransition && requiresTransitionConfirmation) {
            const bool samePendingTransition =
                s_pendingSceneTransition &&
                s_pendingSceneTransitionMatchLike == engineMatchLike &&
                s_pendingSceneTransitionNetworkClient == s_cachedNetworkGameClient;
            if (!samePendingTransition) {
                s_pendingSceneTransition = true;
                s_pendingSceneTransitionMatchLike = engineMatchLike;
                s_pendingSceneTransitionNetworkClient = s_cachedNetworkGameClient;
                s_pendingSceneTransitionCount = 1;
                sceneTransition = false;
            } else {
                ++s_pendingSceneTransitionCount;
                sceneTransition = s_pendingSceneTransitionCount >= 2u;
            }
        } else if (!sceneTransition) {
            s_pendingSceneTransition = false;
            s_pendingSceneTransitionMatchLike = false;
            s_pendingSceneTransitionNetworkClient = 0;
            s_pendingSceneTransitionCount = 0;
        }

        if (sceneTransition) {
            s_pendingSceneTransition = false;
            s_pendingSceneTransitionMatchLike = false;
            s_pendingSceneTransitionNetworkClient = 0;
            s_pendingSceneTransitionCount = 0;
            const char* reason =
                matchLikeChanged      ? (engineMatchLike ? "engine_match_enter" : "engine_match_exit")
                                      : "network_client_changed";
            const bool bumpMapEpoch = networkClientChanged || (matchLikeChanged && engineMatchLike);
            handleSceneTransition(reason, bumpMapEpoch, true);
            
            
            
            
            s_lastEngineResolveUs = 0;
        }
        s_prevEngineMatchLike = engineMatchLike;
        s_prevNetworkGameClient = s_cachedNetworkGameClient;
    }

    {
        static uint64_t s_lastPeriodicRefreshUs = 0;
        const uint64_t periodicNowUs = TickNowUs();
        constexpr uint64_t kPeriodicRefreshIntervalUs = 120000000;
        if (s_lastPeriodicRefreshUs == 0)
            s_lastPeriodicRefreshUs = periodicNowUs;
        if (periodicNowUs - s_lastPeriodicRefreshUs >= kPeriodicRefreshIntervalUs) {
            s_lastPeriodicRefreshUs = periodicNowUs;
            if (mem.vHandle) {
                VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_FREQ_TLB, 1);
                DmaLogPrintf("[INFO] Periodic TLB refresh (lightweight)");
            }
        }
    }
