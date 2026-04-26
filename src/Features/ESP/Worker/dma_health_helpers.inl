static void MarkDmaReadFailure()
{
    s_dmaTotalFailures.fetch_add(1, std::memory_order_relaxed);
    s_dmaConsecutiveFailures.fetch_add(1, std::memory_order_relaxed);
}

static void MarkDmaReadSuccess()
{
    s_dmaTotalSuccesses.fetch_add(1, std::memory_order_relaxed);
    s_dmaConsecutiveFailures.store(0, std::memory_order_relaxed);
    s_dmaLastSuccessTick.store(TickNowUs() / 1000u, std::memory_order_relaxed);
}

esp::DmaHealthStats esp::GetDmaHealthStats()
{
    const uint64_t nowMs = TickNowUs() / 1000u;
    esp::DmaHealthStats stats = {};
    stats.workerRunning = s_dataWorkerRunning.load(std::memory_order_relaxed);
    stats.cameraWorkerRunning = s_cameraWorkerRunning.load(std::memory_order_relaxed);
    stats.dataWorkerInFlight = s_dataWorkerUpdateInFlight.load(std::memory_order_acquire);
    stats.recovering = s_dmaRecovering.load(std::memory_order_relaxed);
    stats.recoveryRequested = s_dmaRecoveryRequested.load(std::memory_order_relaxed);
    stats.consecutiveFailures = s_dmaConsecutiveFailures.load(std::memory_order_relaxed);
    stats.totalFailures = s_dmaTotalFailures.load(std::memory_order_relaxed);
    stats.totalSuccesses = s_dmaTotalSuccesses.load(std::memory_order_relaxed);
    stats.totalRecoveries = s_dmaTotalRecoveries.load(std::memory_order_relaxed);
    const uint64_t lastLoopStartUs = s_dataWorkerLastLoopStartUs.load(std::memory_order_relaxed);
    if (lastLoopStartUs > 0) {
        const uint64_t lastLoopStartMs = lastLoopStartUs / 1000u;
        if (nowMs >= lastLoopStartMs)
            stats.dataWorkerLoopAgeMs = nowMs - lastLoopStartMs;
    }
    const uint64_t inFlightSinceUs = s_dataWorkerInFlightSinceUs.load(std::memory_order_relaxed);
    if (stats.dataWorkerInFlight && inFlightSinceUs > 0) {
        const uint64_t inFlightSinceMs = inFlightSinceUs / 1000u;
        if (nowMs >= inFlightSinceMs)
            stats.dataWorkerInFlightAgeMs = nowMs - inFlightSinceMs;
    }
    stats.dataWorkerStalled =
        stats.workerRunning &&
        stats.dataWorkerInFlight &&
        stats.dataWorkerInFlightAgeMs >= (kDataWorkerStallUs / 1000u);
    const uint64_t lastSuccessMs = s_dmaLastSuccessTick.load(std::memory_order_relaxed);
    if (lastSuccessMs > 0 && nowMs >= lastSuccessMs)
        stats.lastSuccessAgeMs = nowMs - lastSuccessMs;
    const uint64_t recoveryRequestedUs = s_dmaRecoveryRequestedAtUs.load(std::memory_order_relaxed);
    if (stats.recoveryRequested && recoveryRequestedUs > 0) {
        const uint64_t recoveryRequestedMs = recoveryRequestedUs / 1000u;
        if (nowMs >= recoveryRequestedMs)
            stats.recoveryRequestAgeMs = nowMs - recoveryRequestedMs;
    }
    {
        std::lock_guard<std::mutex> lock(s_dmaEventMutex);
        stats.eventCount = static_cast<int>(std::min<uint32_t>(
            s_dmaEventCount,
            static_cast<uint32_t>(esp::DmaHealthStats::kMaxEvents)));
        for (int i = 0; i < stats.eventCount; ++i) {
            const uint32_t newestIndex =
                (s_dmaEventWriteIndex + esp::DmaHealthStats::kMaxEvents - 1u - static_cast<uint32_t>(i)) %
                esp::DmaHealthStats::kMaxEvents;
            const DmaEventRecord& source = s_dmaEvents[newestIndex];
            strncpy_s(stats.events[i].action, sizeof(stats.events[i].action), source.action, _TRUNCATE);
            strncpy_s(stats.events[i].reason, sizeof(stats.events[i].reason), source.reason, _TRUNCATE);
            if (source.timeUs > 0) {
                const uint64_t eventMs = source.timeUs / 1000u;
                stats.events[i].ageMs = nowMs >= eventMs ? nowMs - eventMs : 0;
            }
        }
    }
    stats.gameStatus = static_cast<esp::GameStatus>(s_gameStatus.load(std::memory_order_relaxed));
    return stats;
}

esp::DebugStats esp::GetDebugStats()
{
    const uint64_t nowUs = TickNowUs();
    esp::DebugStats stats = {};
    stats.publishCount = s_publishCount.load(std::memory_order_relaxed);
    stats.lastPublishUs = s_lastPublishUs.load(std::memory_order_relaxed);
    stats.cycleUs = s_dataWorkerCycleUs.load(std::memory_order_relaxed);
    stats.maxCycleUs = s_dataWorkerMaxCycleUs.load(std::memory_order_relaxed);
    stats.activePlayers = s_activePlayerCount.load(std::memory_order_relaxed);
    stats.playerSlotBudget = s_playerSlotScanLimitStat.load(std::memory_order_relaxed);
    stats.engineMaxClients = s_engineMaxClients.load(std::memory_order_relaxed);
    stats.highestEntityIdx = s_highestEntityIdxStat.load(std::memory_order_relaxed);
    stats.worldMarkerCount = s_worldMarkerCountStat.load(std::memory_order_relaxed);
    stats.uptimeUs = nowUs;
    const uint64_t sessionStartUs = s_sessionStartUs.load(std::memory_order_relaxed);
    if (sessionStartUs > 0 && nowUs >= sessionStartUs)
        stats.sessionUptimeUs = nowUs - sessionStartUs;
    if (stats.highestEntityIdx <= 800) stats.worldScanTargetIntervalUs = 50000;
    else if (stats.highestEntityIdx <= 1200) stats.worldScanTargetIntervalUs = 70000;
    else if (stats.highestEntityIdx <= 2000) stats.worldScanTargetIntervalUs = 90000;
    else stats.worldScanTargetIntervalUs = 120000;
    const uint64_t lastWorldScanUs = s_lastWorldScanCommittedUs.load(std::memory_order_relaxed);
    if (lastWorldScanUs > 0 && nowUs >= lastWorldScanUs)
        stats.worldScanAgeUs = nowUs - lastWorldScanUs;
    stats.stages.engineUs = s_stageEngineUs.load(std::memory_order_relaxed);
    stats.stages.baseReadsUs = s_stageBaseReadsUs.load(std::memory_order_relaxed);
    stats.stages.playerReadsUs = s_stagePlayerReadsUs.load(std::memory_order_relaxed);
    stats.stages.playerHierarchyUs = s_stagePlayerHierarchyUs.load(std::memory_order_relaxed);
    stats.stages.playerCoreUs = s_stagePlayerCoreUs.load(std::memory_order_relaxed);
    stats.stages.playerRepairUs = s_stagePlayerRepairUs.load(std::memory_order_relaxed);
    stats.stages.commitStateUs = s_stageCommitStateUs.load(std::memory_order_relaxed);
    stats.stages.playerAuxUs = s_stagePlayerAuxUs.load(std::memory_order_relaxed);
    stats.stages.inventoryUs = s_stageInventoryUs.load(std::memory_order_relaxed);
    stats.stages.boneReadsUs = s_stageBoneReadsUs.load(std::memory_order_relaxed);
    stats.stages.bombScanUs = s_stageBombScanUs.load(std::memory_order_relaxed);
    stats.stages.worldScanUs = s_stageWorldScanUs.load(std::memory_order_relaxed);
    stats.stages.worldScanLastUs = s_stageWorldScanLastUs.load(std::memory_order_relaxed);
    stats.stages.commitEnrichUs = s_stageCommitEnrichUs.load(std::memory_order_relaxed);
    stats.stages.playerAuxLastUs = s_stagePlayerAuxLastUs.load(std::memory_order_relaxed);
    stats.stages.inventoryLastUs = s_stageInventoryLastUs.load(std::memory_order_relaxed);
    stats.stages.boneReadsLastUs = s_stageBoneReadsLastUs.load(std::memory_order_relaxed);
    const uint64_t playerAuxLastAtUs = s_stagePlayerAuxLastAtUs.load(std::memory_order_relaxed);
    const uint64_t inventoryLastAtUs = s_stageInventoryLastAtUs.load(std::memory_order_relaxed);
    const uint64_t boneReadsLastAtUs = s_stageBoneReadsLastAtUs.load(std::memory_order_relaxed);
    if (playerAuxLastAtUs > 0 && nowUs >= playerAuxLastAtUs)
        stats.stages.playerAuxAgeUs = nowUs - playerAuxLastAtUs;
    if (inventoryLastAtUs > 0 && nowUs >= inventoryLastAtUs)
        stats.stages.inventoryAgeUs = nowUs - inventoryLastAtUs;
    if (boneReadsLastAtUs > 0 && nowUs >= boneReadsLastAtUs)
        stats.stages.boneReadsAgeUs = nowUs - boneReadsLastAtUs;
    stats.stages.totalUs = stats.stages.engineUs + stats.stages.baseReadsUs +
        stats.stages.playerReadsUs + stats.stages.commitStateUs +
        stats.stages.playerAuxUs + stats.stages.inventoryUs +
        stats.stages.boneReadsUs + stats.stages.bombScanUs +
        stats.stages.worldScanUs + stats.stages.commitEnrichUs;
    uint64_t heldWorldScanUs = stats.stages.worldScanLastUs;
    if (stats.worldScanAgeUs > 0) {
        const uint64_t staleWorldThresholdUs =
            std::max<uint64_t>(stats.worldScanTargetIntervalUs * 2u, 120000u);
        if (stats.worldScanAgeUs > staleWorldThresholdUs)
            heldWorldScanUs = 0;
    }
    stats.stages.totalHeldUs = stats.stages.engineUs + stats.stages.baseReadsUs +
        stats.stages.playerReadsUs + stats.stages.commitStateUs +
        std::max(stats.stages.playerAuxUs, stats.stages.playerAuxLastUs) +
        std::max(stats.stages.inventoryUs, stats.stages.inventoryLastUs) +
        std::max(stats.stages.boneReadsUs, stats.stages.boneReadsLastUs) +
        stats.stages.bombScanUs +
        heldWorldScanUs + stats.stages.commitEnrichUs;
    stats.camera.cycleUs = s_cameraWorkerCycleUs.load(std::memory_order_relaxed);
    stats.camera.maxCycleUs = s_cameraWorkerMaxCycleUs.load(std::memory_order_relaxed);
    const auto overlayStats = overlay::GetPerfStats();
    stats.overlay.frameUs = overlayStats.frameUs;
    stats.overlay.maxFrameUs = overlayStats.maxFrameUs;
    stats.overlay.syncUs = overlayStats.syncUs;
    stats.overlay.drawUs = overlayStats.drawUs;
    stats.overlay.presentUs = overlayStats.presentUs;
    stats.overlay.pacingWaitUs = overlayStats.pacingWaitUs;
    {
        std::lock_guard<std::mutex> lock(s_cameraMutex);
        stats.liveViewValid = s_liveViewValid;
        stats.liveLocalPosValid = s_liveLocalPosValid;
        if (s_liveViewUpdatedUs > 0 && nowUs >= s_liveViewUpdatedUs)
            stats.cameraViewAgeUs = nowUs - s_liveViewUpdatedUs;
        if (s_liveLocalPosUpdatedUs > 0 && nowUs >= s_liveLocalPosUpdatedUs)
            stats.cameraLocalPosAgeUs = nowUs - s_liveLocalPosUpdatedUs;
    }
    stats.liveViewFresh =
        stats.liveViewValid &&
        stats.cameraViewAgeUs > 0 &&
        stats.cameraViewAgeUs <= kLiveCameraFreshnessUs;
    stats.liveLocalPosFresh =
        stats.liveLocalPosValid &&
        stats.cameraLocalPosAgeUs > 0 &&
        stats.cameraLocalPosAgeUs <= kLiveCameraFreshnessUs;
    stats.engineMenu = s_engineMenu.load(std::memory_order_relaxed);
    stats.engineInGame = s_engineInGame.load(std::memory_order_relaxed);
    stats.sceneEpoch = s_sceneResetSerial.load(std::memory_order_relaxed);
    stats.mapEpoch = s_mapEpoch.load(std::memory_order_relaxed);
    stats.bombEpoch = s_bombEpoch.load(std::memory_order_relaxed);
    stats.warmupState =
        static_cast<esp::SceneWarmupState>(s_sceneWarmupState.load(std::memory_order_relaxed));
    const uint64_t warmupEnteredUs = s_sceneWarmupEnteredUs.load(std::memory_order_relaxed);
    if (warmupEnteredUs > 0 && nowUs >= warmupEnteredUs)
        stats.warmupAgeUs = nowUs - warmupEnteredUs;
    stats.playersCore = GetSubsystemHealthInfo(RuntimeSubsystem::PlayersCore, nowUs);
    stats.cameraView = GetSubsystemHealthInfo(RuntimeSubsystem::CameraView, nowUs);
    stats.gamerulesMap = GetSubsystemHealthInfo(RuntimeSubsystem::GameRulesMap, nowUs);
    stats.bones = GetSubsystemHealthInfo(RuntimeSubsystem::Bones, nowUs);
    stats.world = GetSubsystemHealthInfo(RuntimeSubsystem::World, nowUs);
    const uint32_t bombFlags = s_bombDebugFlags.load(std::memory_order_relaxed);
    stats.bombPlanted = (bombFlags & (1u << 0)) != 0u;
    stats.bombTicking = (bombFlags & (1u << 1)) != 0u;
    stats.bombBeingDefused = (bombFlags & (1u << 2)) != 0u;
    stats.bombDropped = (bombFlags & (1u << 3)) != 0u;
    stats.bombBoundsValid = (bombFlags & (1u << 4)) != 0u;
    stats.bombSourceFlags = s_bombDebugSourceFlags.load(std::memory_order_relaxed);
    stats.bombConfidence = s_bombDebugConfidence.load(std::memory_order_relaxed);
    return stats;
}
