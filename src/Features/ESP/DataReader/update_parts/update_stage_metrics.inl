    {
        const uint64_t playerAuxDelta = _stagePlayerAuxEnd - _stageCommitEnd;
        const uint64_t inventoryDelta = _stageInvEnd - _stagePlayerAuxEnd;
        const uint64_t boneReadsDelta = _stageBoneEnd - _stageInvEnd;
        s_stageEngineUs.store(_stageEngineEnd - _stagePipelineStart, std::memory_order_relaxed);
        s_stageBaseReadsUs.store(_stageBaseEnd - _stageEngineEnd, std::memory_order_relaxed);
        s_stagePlayerReadsUs.store(_stagePlayerEnd - _stageBaseEnd, std::memory_order_relaxed);
        s_stagePlayerHierarchyUs.store(_playerHierarchyUs, std::memory_order_relaxed);
        s_stagePlayerCoreUs.store(_playerCoreUs, std::memory_order_relaxed);
        s_stagePlayerRepairUs.store(_playerRepairUs, std::memory_order_relaxed);
        s_stageCommitStateUs.store(_stageCommitEnd - _stagePlayerEnd, std::memory_order_relaxed);
        s_stagePlayerAuxUs.store(playerAuxDelta, std::memory_order_relaxed);
        s_stageInventoryUs.store(inventoryDelta, std::memory_order_relaxed);
        s_stageBoneReadsUs.store(boneReadsDelta, std::memory_order_relaxed);
        if (_playerAuxActiveTick || playerAuxDelta > 50) {
            s_stagePlayerAuxLastUs.store(playerAuxDelta, std::memory_order_relaxed);
            s_stagePlayerAuxLastAtUs.store(_stagePlayerAuxEnd, std::memory_order_relaxed);
        }
        if (_inventoryActiveTick || _inventoryFullTick || inventoryDelta > 50) {
            s_stageInventoryLastUs.store(inventoryDelta, std::memory_order_relaxed);
            s_stageInventoryLastAtUs.store(_stageInvEnd, std::memory_order_relaxed);
        }
        if (_boneReadsActiveTick || boneReadsDelta > 50) {
            s_stageBoneReadsLastUs.store(boneReadsDelta, std::memory_order_relaxed);
            s_stageBoneReadsLastAtUs.store(_stageBoneEnd, std::memory_order_relaxed);
        }
        const uint64_t worldScanDelta = _stageWorldEnd - _stageBoneEnd;
        s_stageWorldScanUs.store(worldScanDelta, std::memory_order_relaxed);
        if (worldScanCommitted || worldScanDelta > 500)
            s_stageWorldScanLastUs.store(worldScanDelta, std::memory_order_relaxed);
        s_stageBombScanUs.store(_stageBombEnd - _stageWorldEnd, std::memory_order_relaxed);
        s_stageCommitEnrichUs.store(_stageEnrichEnd - _stageBombEnd, std::memory_order_relaxed);
    }
