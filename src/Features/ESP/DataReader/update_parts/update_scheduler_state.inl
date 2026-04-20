    int lastTrackedPlayerSlotHint = 0;
    for (int i = 63; i >= 0; --i) {
        if ((s_players[i].valid || s_players[i].pawn != 0) ||
            (s_webRadarPlayers[i].valid || s_webRadarPlayers[i].pawn != 0)) {
            lastTrackedPlayerSlotHint = i + 1;
            break;
        }
    }
    const uint64_t budgetNowUs = TickNowUs();
    const uint64_t lastSceneResetUs = s_lastSceneResetUs.load(std::memory_order_relaxed);
    const auto sceneWarmupState =
        static_cast<esp::SceneWarmupState>(s_sceneWarmupState.load(std::memory_order_relaxed));
    const bool recentSceneReset =
        lastSceneResetUs > 0 &&
        budgetNowUs > lastSceneResetUs &&
        (budgetNowUs - lastSceneResetUs) <= 2000000;
    const bool warmupActive = sceneWarmupState != esp::SceneWarmupState::Stable;
    const int activePlayerHint = std::clamp(s_activePlayerCount.load(std::memory_order_relaxed), 0, 64);
    const int hierarchyHighWaterSlotHint =
        std::clamp(s_playerHierarchyHighWaterSlot.load(std::memory_order_relaxed), 0, 64);
    const int localPlayerSlotHint =
        (engineLocalPlayerSlot >= 0 && engineLocalPlayerSlot < 64)
        ? (engineLocalPlayerSlot + 1)
        : 0;
    const int maxClientsHint = std::clamp(engineMaxClients, 0, 64);
    const int engineBudgetHint =
        (maxClientsHint > 0 && maxClientsHint < 64)
        ? maxClientsHint
        : 0;
    bool forceFullPlayerDiscoverySweep = false;
    int playerSlotScanLimit = 64;
    if (engineBudgetHint > 0 || lastTrackedPlayerSlotHint > 0 || activePlayerHint > 0 ||
        localPlayerSlotHint > 0 || hierarchyHighWaterSlotHint > 0) {
        const int seed =
            std::max({
                engineBudgetHint,
                lastTrackedPlayerSlotHint,
                std::min(activePlayerHint + 2, 64),
                localPlayerSlotHint,
                hierarchyHighWaterSlotHint
            });
        const int reserve =
            seed <= 10 ? 6 :
            seed <= 16 ? 8 : 10;
        playerSlotScanLimit = std::clamp(seed + reserve, 12, 64);
    }
    {
        static uint64_t s_lastFullPlayerDiscoveryUs = 0;
        constexpr uint64_t kFullPlayerDiscoveryIntervalUs = 600000;
        const bool fullPlayerDiscoveryDue =
            recentSceneReset ||
            warmupActive ||
            s_lastFullPlayerDiscoveryUs == 0 ||
            (budgetNowUs - s_lastFullPlayerDiscoveryUs) >= kFullPlayerDiscoveryIntervalUs;
        if (fullPlayerDiscoveryDue) {
            forceFullPlayerDiscoverySweep = true;
            s_lastFullPlayerDiscoveryUs = budgetNowUs;
        }
    }
    s_playerSlotScanLimitStat.store(playerSlotScanLimit, std::memory_order_relaxed);
    bool _playerAuxActiveTick = false;
    bool _inventoryActiveTick = false;
    bool _inventoryFullTick = false;
    bool _boneReadsActiveTick = false;
