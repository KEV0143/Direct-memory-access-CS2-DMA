        struct WorldReadScratch {
            uintptr_t worldBlocks[kMaxTrackedWorldBlocks];
            uintptr_t worldEntities[kMaxTrackedWorldEntities + 1];
            uintptr_t worldSceneNodes[kMaxTrackedWorldEntities + 1];
            uint32_t worldOwnerHandles[kMaxTrackedWorldEntities + 1];
            uint32_t worldSubclassIds[kMaxTrackedWorldEntities + 1];
            uint16_t worldItemDefs[kMaxTrackedWorldEntities + 1];
            int worldSmokeTick[kMaxTrackedWorldEntities + 1];
            uint8_t worldSmokeActive[kMaxTrackedWorldEntities + 1];
            uint8_t worldSmokeVolumeDataReceived[kMaxTrackedWorldEntities + 1];
            uint8_t worldSmokeEffectSpawned[kMaxTrackedWorldEntities + 1];
            int worldInfernoTick[kMaxTrackedWorldEntities + 1];
            float worldInfernoLife[kMaxTrackedWorldEntities + 1];
            int worldInfernoFireCount[kMaxTrackedWorldEntities + 1];
            uint8_t worldInfernoInPostEffect[kMaxTrackedWorldEntities + 1];
            int worldDecoyTick[kMaxTrackedWorldEntities + 1];
            int worldDecoyClientTick[kMaxTrackedWorldEntities + 1];
            int worldExplodeTick[kMaxTrackedWorldEntities + 1];
            Vector3 worldVelocities[kMaxTrackedWorldEntities + 1];
            Vector3 worldPositions[kMaxTrackedWorldEntities + 1];
        };
        static thread_local WorldReadScratch s_worldReadScratch = {};
        auto& worldBlocks = s_worldReadScratch.worldBlocks;
        auto& worldEntities = s_worldReadScratch.worldEntities;
        auto& worldSceneNodes = s_worldReadScratch.worldSceneNodes;
        auto& worldOwnerHandles = s_worldReadScratch.worldOwnerHandles;
        auto& worldSubclassIds = s_worldReadScratch.worldSubclassIds;
        auto& worldItemDefs = s_worldReadScratch.worldItemDefs;
        auto& worldSmokeTick = s_worldReadScratch.worldSmokeTick;
        auto& worldSmokeActive = s_worldReadScratch.worldSmokeActive;
        auto& worldSmokeVolumeDataReceived = s_worldReadScratch.worldSmokeVolumeDataReceived;
        auto& worldSmokeEffectSpawned = s_worldReadScratch.worldSmokeEffectSpawned;
        auto& worldInfernoTick = s_worldReadScratch.worldInfernoTick;
        auto& worldInfernoLife = s_worldReadScratch.worldInfernoLife;
        auto& worldInfernoFireCount = s_worldReadScratch.worldInfernoFireCount;
        auto& worldInfernoInPostEffect = s_worldReadScratch.worldInfernoInPostEffect;
        auto& worldDecoyTick = s_worldReadScratch.worldDecoyTick;
        auto& worldDecoyClientTick = s_worldReadScratch.worldDecoyClientTick;
        auto& worldExplodeTick = s_worldReadScratch.worldExplodeTick;
        auto& worldVelocities = s_worldReadScratch.worldVelocities;
        auto& worldPositions = s_worldReadScratch.worldPositions;
        const int worldLimit = std::clamp(highestEntityIndex, 64, kMaxTrackedWorldEntities);
        const int blockCount = std::min((worldLimit >> 9) + 1, kMaxTrackedWorldBlocks);
        static thread_local int s_worldCandidateIndices[kMaxTrackedWorldEntities + 1];
        static thread_local uint8_t s_worldCandidateMask[kMaxTrackedWorldEntities + 1];
        static thread_local int s_worldProcessIndices[kMaxTrackedWorldEntities + 1];
        static thread_local uint8_t s_worldEntityChangedFlags[kMaxTrackedWorldEntities + 1];
        std::memset(s_worldCandidateMask, 0, sizeof(s_worldCandidateMask));
        int worldCandidateCount = 0;
        auto pushWorldCandidate = [&](int idx) {
            if (idx < 65 || idx > worldLimit || s_worldCandidateMask[idx] != 0)
                return;
            s_worldCandidateMask[idx] = 1u;
            s_worldCandidateIndices[worldCandidateCount++] = idx;
        };
        auto isTrackedWorldEntitySlot = [&](int idx) {
            return
                s_worldEntityRefs[idx] != 0 ||
                s_worldEntityItemIds[idx] != 0 ||
                s_worldEntitySubclassIds[idx] != 0 ||
                s_worldUtilityHasHistory[idx] ||
                s_worldSmokeLatched[idx] ||
                s_worldInfernoLatched[idx] ||
                s_worldDecoyLatched[idx] ||
                s_worldExplosiveLatched[idx] ||
                s_worldSmokeEvidenceCount[idx] != 0 ||
                s_worldInfernoEvidenceCount[idx] != 0 ||
                s_worldDecoyEvidenceCount[idx] != 0 ||
                s_worldExplosiveEvidenceCount[idx] != 0;
        };
        auto addTrackedWorldIndex = [&](int idx) {
            if (idx < 65 || idx > kMaxTrackedWorldEntities || s_worldTrackedIndexPos[idx] != 0)
                return;
            if (s_worldTrackedIndexCount >= kMaxTrackedWorldEntities)
                return;
            s_worldTrackedIndices[s_worldTrackedIndexCount] = idx;
            s_worldTrackedIndexPos[idx] = static_cast<uint16_t>(s_worldTrackedIndexCount + 1);
            ++s_worldTrackedIndexCount;
        };
        auto removeTrackedWorldIndex = [&](int idx) {
            if (idx < 65 || idx > kMaxTrackedWorldEntities)
                return;
            const uint16_t posPlusOne = s_worldTrackedIndexPos[idx];
            if (posPlusOne == 0)
                return;
            const int pos = static_cast<int>(posPlusOne - 1u);
            const int lastPos = s_worldTrackedIndexCount - 1;
            const int lastIdx = s_worldTrackedIndices[lastPos];
            s_worldTrackedIndices[pos] = lastIdx;
            s_worldTrackedIndices[lastPos] = 0;
            s_worldTrackedIndexPos[idx] = 0;
            --s_worldTrackedIndexCount;
            if (pos != lastPos && lastIdx >= 65 && lastIdx <= kMaxTrackedWorldEntities)
                s_worldTrackedIndexPos[lastIdx] = static_cast<uint16_t>(pos + 1);
        };
        auto refreshTrackedWorldIndex = [&](int idx) {
            if (isTrackedWorldEntitySlot(idx))
                addTrackedWorldIndex(idx);
            else
                removeTrackedWorldIndex(idx);
        };
        auto clearWorldBombCandidateSlots = [&]() {
            if (s_worldBombCandidateSlotCount == 0)
                return;
            std::memset(s_worldBombCandidateSlots, 0, sizeof(s_worldBombCandidateSlots));
            s_worldBombCandidateSlotCount = 0;
        };
        auto rememberWorldBombCandidateSlot = [&](int idx) {
            if (idx < 65 || idx > kMaxTrackedWorldEntities)
                return;
            constexpr int kMaxWorldBombCandidateSlots =
                static_cast<int>(sizeof(s_worldBombCandidateSlots) / sizeof(s_worldBombCandidateSlots[0]));
            for (int i = 0; i < static_cast<int>(s_worldBombCandidateSlotCount); ++i) {
                if (s_worldBombCandidateSlots[i] == idx)
                    return;
            }
            if (s_worldBombCandidateSlotCount < kMaxWorldBombCandidateSlots) {
                s_worldBombCandidateSlots[s_worldBombCandidateSlotCount++] = idx;
                return;
            }
            std::memmove(
                s_worldBombCandidateSlots,
                s_worldBombCandidateSlots + 1,
                sizeof(s_worldBombCandidateSlots[0]) * static_cast<size_t>(kMaxWorldBombCandidateSlots - 1));
            s_worldBombCandidateSlots[kMaxWorldBombCandidateSlots - 1] = idx;
        };
        std::memset(worldBlocks, 0, sizeof(uintptr_t) * static_cast<size_t>(blockCount));
        auto resetWorldScratchSlot = [&](int idx) {
            worldEntities[idx] = 0;
            worldSceneNodes[idx] = 0;
            worldOwnerHandles[idx] = 0;
            worldSubclassIds[idx] = 0;
            worldItemDefs[idx] = 0;
            worldSmokeTick[idx] = 0;
            worldSmokeActive[idx] = 0;
            worldSmokeVolumeDataReceived[idx] = 0;
            worldSmokeEffectSpawned[idx] = 0;
            worldInfernoTick[idx] = 0;
            worldInfernoLife[idx] = 0.0f;
            worldInfernoFireCount[idx] = 0;
            worldInfernoInPostEffect[idx] = 0;
            worldDecoyTick[idx] = 0;
            worldDecoyClientTick[idx] = 0;
            worldExplodeTick[idx] = 0;
            worldVelocities[idx] = {};
            worldPositions[idx] = {};
        };
        for (int candidateIdx = 0; candidateIdx < worldCandidateCount; ++candidateIdx)
            resetWorldScratchSlot(s_worldCandidateIndices[candidateIdx]);

        static uintptr_t s_cachedWorldBlocks[kMaxTrackedWorldBlocks] = {};
        static int s_cachedWorldBlockCount = 0;
        static uint64_t s_worldBlockCacheResetSerial = 0;
        const uint64_t worldBlockSceneResetSerial = s_sceneResetSerial.load(std::memory_order_relaxed);
        if (s_worldBlockCacheResetSerial != worldBlockSceneResetSerial) {
            s_worldBlockCacheResetSerial = worldBlockSceneResetSerial;
            memset(s_cachedWorldBlocks, 0, sizeof(s_cachedWorldBlocks));
            s_cachedWorldBlockCount = 0;
        }

        auto hasTrackedSubclass = [](const uint32_t (&tracked)[kTrackedWorldSubclassSlots], uint32_t subclassId) -> bool {
            if (subclassId == 0u)
                return false;
            for (uint32_t trackedId : tracked) {
                if (trackedId == subclassId)
                    return true;
            }
            return false;
        };
        auto rememberTrackedSubclass = [](uint32_t (&tracked)[kTrackedWorldSubclassSlots], uint32_t subclassId) {
            if (subclassId == 0u)
                return;
            for (uint32_t& trackedId : tracked) {
                if (trackedId == subclassId)
                    return;
                if (trackedId == 0u) {
                    trackedId = subclassId;
                    return;
                }
            }
            tracked[0] = subclassId;
        };
        auto normalizeWorldItemId = [](uint16_t itemId) -> uint16_t {
            if (itemId == 0 || itemId >= 1200)
                return 0;
            if (WeaponNameFromItemId(itemId) == nullptr)
                return 0;
            return itemId;
        };
        auto isUtilityWorldItemId = [](uint16_t itemId) -> bool {
            return itemId >= 43 && itemId <= 48;
        };
        auto needsWorldOwnerRefreshForItemId = [&](uint16_t itemId) -> bool {
            if (itemId == 0)
                return false;
            if (itemId == kWeaponC4Id)
                return true;
            return WeaponNameFromItemId(itemId) != nullptr &&
                   !isUtilityWorldItemId(itemId) &&
                   !IsKnifeItemId(itemId);
        };

        #include "world_parts/world_domain_candidates.inl"
