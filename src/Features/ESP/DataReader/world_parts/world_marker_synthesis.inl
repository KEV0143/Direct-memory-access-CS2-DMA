        bool worldPositionsOk = worldScanOk;
        const bool reuseCachedWorldUtilityDetails =
            worldScanOk &&
            worldPositionsOk &&
            wantsWorldUtilityData &&
            !shouldReadWorldUtilityDetails;

        int scannedDroppedWeaponCount = 0;
        int scannedUtilityEffectCount = 0;
        int scannedProjectileCount = 0;
        constexpr int kMaxDroppedWeaponMarkers = 160;
        constexpr int kMaxUtilityEffectMarkers = 72;
        constexpr int kMaxProjectileMarkers = 24;
        auto pushWorldMarker = [&](WorldMarkerType type, const Vector3& pos, uint16_t weaponId, float lifeHint, uint64_t expiresAtUs) {
            if (!isFiniteVec(pos))
                return;
            if (std::fabs(pos.x) < 1.0f && std::fabs(pos.y) < 1.0f && std::fabs(pos.z) < 1.0f)
                return;
            if (scannedMarkerCount >= 256)
                return;
            if (type == WorldMarkerType::DroppedWeapon) {
                if (scannedDroppedWeaponCount >= kMaxDroppedWeaponMarkers)
                    return;
                ++scannedDroppedWeaponCount;
            } else {
                const bool isProjectile =
                    type == WorldMarkerType::SmokeProjectile ||
                    type == WorldMarkerType::MolotovProjectile ||
                    type == WorldMarkerType::DecoyProjectile;
                if (isProjectile) {
                    if (scannedProjectileCount >= kMaxProjectileMarkers)
                        return;
                    ++scannedProjectileCount;
                } else {
                    if (scannedUtilityEffectCount >= kMaxUtilityEffectMarkers)
                        return;
                    ++scannedUtilityEffectCount;
                }
            }
            WorldMarker& m = scannedMarkers[scannedMarkerCount++];
            m.valid = true;
            m.type = type;
            m.position = pos;
            m.weaponId = weaponId;
            m.lifeHint = lifeHint;
            m.expiresUs = expiresAtUs;
        };

        auto resetWorldUtilityState = [&](int idx) {
            s_worldSmokeLatched[idx] = false;
            s_worldInfernoLatched[idx] = false;
            s_worldDecoyLatched[idx] = false;
            s_worldExplosiveLatched[idx] = false;
            s_worldUtilityHasHistory[idx] = false;
            s_worldSmokeStartUs[idx] = 0;
            s_worldInfernoStartUs[idx] = 0;
            s_worldDecoyStartUs[idx] = 0;
            s_worldExplosiveStartUs[idx] = 0;
            s_worldSmokeEvidenceCount[idx] = 0;
            s_worldInfernoEvidenceCount[idx] = 0;
            s_worldDecoyEvidenceCount[idx] = 0;
            s_worldExplosiveEvidenceCount[idx] = 0;
            s_worldPrevPos[idx] = {};
            s_worldPrevSmokeTick[idx] = 0;
            s_worldPrevSmokeActive[idx] = 0;
            s_worldPrevSmokeVolumeDataReceived[idx] = 0;
            s_worldPrevSmokeEffectSpawned[idx] = 0;
            s_worldPrevInfernoTick[idx] = 0;
            s_worldPrevInfernoLife[idx] = 0.0f;
            s_worldPrevInfernoFireCount[idx] = 0;
            s_worldPrevInfernoInPostEffect[idx] = 0;
            s_worldPrevDecoyTick[idx] = 0;
            s_worldPrevDecoyClientTick[idx] = 0;
            s_worldPrevExplodeTick[idx] = 0;
            s_worldPrevVelocity[idx] = {};
        };
        const bool gameTimeWrapped = (currentGameTime > 0.0f && s_prevWorldGameTime > 5.0f && (currentGameTime + 1.0f) < s_prevWorldGameTime);
        if (gameTimeWrapped || (currentGameTime > 0.0f && currentGameTime < 1.0f && s_prevWorldGameTime > 60.0f)) {
            memset(s_worldSmokeSubclassIds, 0, sizeof(s_worldSmokeSubclassIds));
            memset(s_worldMolotovSubclassIds, 0, sizeof(s_worldMolotovSubclassIds));
            memset(s_worldDecoySubclassIds, 0, sizeof(s_worldDecoySubclassIds));
            memset(s_worldHeSubclassIds, 0, sizeof(s_worldHeSubclassIds));
            memset(s_worldInfernoSubclassIds, 0, sizeof(s_worldInfernoSubclassIds));
            memset(s_worldTrackedIndices, 0, sizeof(s_worldTrackedIndices));
            memset(s_worldTrackedIndexPos, 0, sizeof(s_worldTrackedIndexPos));
            s_worldTrackedIndexCount = 0;
            for (int resetIdx = 0; resetIdx <= kMaxTrackedWorldEntities; ++resetIdx) {
                s_worldEntityRefs[resetIdx] = 0;
                s_worldEntitySubclassIds[resetIdx] = 0;
                s_worldEntityItemIds[resetIdx] = 0;
                resetWorldUtilityState(resetIdx);
            }
            s_worldWarmupScans = 0;
            s_lastWorldUtilityDetailScanUs = 0;
            s_lastWorldUtilityProbeScanUs = 0;
        }
        if (currentGameTime > 0.0f)
            s_prevWorldGameTime = currentGameTime;
        if (s_worldWarmupScans < 2u)
            ++s_worldWarmupScans;
        const bool warmupWorldScan = (s_worldWarmupScans < 2u);

        auto pushLatchedUtility = [&](bool signal,
                                      bool& latched,
                                      uint64_t& startUs,
                                      uint64_t durationUs,
                                      WorldMarkerType type,
                                      uint16_t weaponId,
                                      const Vector3& pos,
                                      float lifeHint) {
            if (!signal) {
                if (latched && startUs > 0) {
                    const uint64_t expiresAt = startUs + durationUs;
                    if (expiresAt > nowUs) {
                        pushWorldMarker(type, pos, weaponId, lifeHint, expiresAt);
                        return;
                    }
                }
                latched = false;
                startUs = 0;
                return;
            }
            if (!latched || startUs == 0) {
                latched = true;
                startUs = nowUs;
            }
            const uint64_t expiresAt = startUs + durationUs;
            if (expiresAt > nowUs)
                pushWorldMarker(type, pos, weaponId, lifeHint, expiresAt);
            else {
                latched = false;
                startUs = 0;
            }
        };

        auto clampRemainingSeconds = [](float value) -> float {
            if (!std::isfinite(value))
                return -1.0f;
            if (value < 0.0f)
                return 0.0f;
            if (value > 60.0f)
                return -1.0f;
            return value;
        };

        if (intervalPerTick > 0.0001f && intervalPerTick < 0.10f)
            s_lastStableIntervalPerTick = intervalPerTick;
        const float safeIntervalPerTick = s_lastStableIntervalPerTick;
        if (std::isfinite(currentGameTime) && currentGameTime > 0.0f) {
            const bool monotonicEnough =
                s_lastStableGameTime <= 0.0f ||
                currentGameTime >= (s_lastStableGameTime - 0.25f) ||
                currentGameTime < 1.0f;
            if (monotonicEnough) {
                s_lastStableGameTime = currentGameTime;
                s_lastStableGameTimeUs = nowUs;
            }
        } else if (s_lastStableGameTimeUs > 0) {
            const float elapsedSec = std::clamp(
                static_cast<float>(nowUs - s_lastStableGameTimeUs) / 1000000.0f,
                0.0f,
                2.0f);
            currentGameTime = s_lastStableGameTime + elapsedSec;
        }
        auto calcRemainingFromTick = [&](int tickStart, float durationSec) -> float {
            if (safeIntervalPerTick <= 0.0f || tickStart <= 0 || durationSec <= 0.0f || currentGameTime <= 0.0f)
                return -1.0f;
            const float startSec = static_cast<float>(tickStart) * safeIntervalPerTick;
            const float elapsed = currentGameTime - startSec;
            if (elapsed < -2.0f || elapsed > durationSec + 2.0f)
                return -1.0f;
            return clampRemainingSeconds(durationSec - elapsed);
        };

        auto pushUtility = [&](bool signal,
                               float remainingSec,
                               bool& latched,
                               uint64_t& startUs,
                               uint64_t fallbackDurationUs,
                               WorldMarkerType type,
                               uint16_t weaponId,
                               const Vector3& pos,
                               float lifeHint) {
            const bool remainingUnknown = !std::isfinite(remainingSec) || remainingSec < 0.0f || remainingSec > 60.0f;
            const float clampedRemaining = clampRemainingSeconds(remainingSec);
            if (signal && !remainingUnknown && clampedRemaining > 0.0f) {
                if (!latched || startUs == 0) {
                    latched = true;
                    startUs = nowUs;
                }
                const uint64_t expiresAt = nowUs + static_cast<uint64_t>(clampedRemaining * 1000000.0f);
                pushWorldMarker(type, pos, weaponId, lifeHint, expiresAt);
                return;
            }
            if (signal && remainingUnknown) {
                if (!latched || startUs == 0) {
                    latched = true;
                    startUs = nowUs;
                }
                pushWorldMarker(type, pos, weaponId, lifeHint, startUs + fallbackDurationUs);
                return;
            }
            pushLatchedUtility(false, latched, startUs, fallbackDurationUs, type, weaponId, pos, lifeHint);
        };

        constexpr float kSmokeDurationSec = 20.0f;
        constexpr float kMolotovDurationSec = 5.5f;
        constexpr float kDecoyDurationSec = 15.0f;
        constexpr float kExplosiveDurationSec = 1.5f;
        constexpr uint64_t kSmokeFallbackMs = 20000000;
        constexpr uint64_t kMolotovFallbackMs = 5500000;
        constexpr uint64_t kDecoyFallbackMs = 15000000;
        constexpr uint64_t kExplosiveFallbackMs = 1500000;
        struct DebugSample {
            int idx = 0;
            uint16_t itemId = 0;
            uint32_t owner = 0;
            int smokeTick = 0;
            int infernoTick = 0;
            int decoyTick = 0;
            int decoyClientTick = 0;
            int explodeTick = 0;
            int infernoFireCount = 0;
            int smokeActive = 0;
            float smokeRemaining = 0.0f;
            float infernoRemaining = 0.0f;
            float decoyRemaining = 0.0f;
            float explodeRemaining = 0.0f;
        };
        int dbgRawSmoke = 0, dbgRawInferno = 0, dbgRawDecoy = 0, dbgRawExplosive = 0;
        int dbgSignalSmoke = 0, dbgSignalInferno = 0, dbgSignalDecoy = 0, dbgSignalExplosive = 0;
        int dbgEvidenceEntities = 0, dbgNoSignalSamples = 0;
        DebugSample dbgSamples[3] = {};
        auto bumpEvidenceCounter = [](bool evidence, uint8_t& counter) {
            if (!evidence) {
                counter = 0;
                return;
            }
            if (counter < 0xFFu)
                ++counter;
        };
