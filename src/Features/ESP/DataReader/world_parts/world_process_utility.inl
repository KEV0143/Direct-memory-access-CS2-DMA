            bool smokeSubclassKnownNow = knownSmokeSubclass;
            bool decoySubclassKnownNow = knownDecoySubclass;
            bool heSubclassKnownNow = knownHeSubclass;
            bool infernoSubclassKnownNow = knownInfernoSubclass;
            
            const bool unknownUtilityProbeCandidate = isUnknownUtilityProbe[idx];
            if (unknownUtilityProbeCandidate)
                ++unknownUtilityProbeCount;
            const bool relevantUtilityEntity =
                wantsWorldUtilityData &&
                (isUtilityGrenade ||
                 knownSmokeSubclass ||
                 knownDecoySubclass ||
                 knownHeSubclass ||
                 knownInfernoSubclass ||
                 unknownUtilityProbeCandidate);
            if (!relevantUtilityEntity)
                continue;
            const bool firstHistory = !s_worldUtilityHasHistory[idx];
            if (firstHistory) {
                s_worldUtilityHasHistory[idx] = true;
                refreshTrackedWorldIndex(idx);
            }

            float infernoLife = reuseCachedWorldUtilityDetails ? s_worldPrevInfernoLife[idx] : worldInfernoLife[idx];
            if (!std::isfinite(infernoLife) || infernoLife < 0.0f || infernoLife > 20.0f)
                infernoLife = 0.0f;

            const int smokeTick = reuseCachedWorldUtilityDetails ? s_worldPrevSmokeTick[idx] : worldSmokeTick[idx];
            const uint8_t smokeActive = reuseCachedWorldUtilityDetails ? s_worldPrevSmokeActive[idx] : worldSmokeActive[idx];
            const uint8_t smokeVolumeDataReceived = reuseCachedWorldUtilityDetails ? s_worldPrevSmokeVolumeDataReceived[idx] : worldSmokeVolumeDataReceived[idx];
            const uint8_t smokeEffectSpawned = reuseCachedWorldUtilityDetails ? s_worldPrevSmokeEffectSpawned[idx] : worldSmokeEffectSpawned[idx];
            const int infernoTick = reuseCachedWorldUtilityDetails ? s_worldPrevInfernoTick[idx] : worldInfernoTick[idx];
            const int infernoFireCount = reuseCachedWorldUtilityDetails ? s_worldPrevInfernoFireCount[idx] : worldInfernoFireCount[idx];
            const uint8_t infernoInPostEffect = reuseCachedWorldUtilityDetails ? s_worldPrevInfernoInPostEffect[idx] : worldInfernoInPostEffect[idx];
            const int decoyTick = reuseCachedWorldUtilityDetails ? s_worldPrevDecoyTick[idx] : worldDecoyTick[idx];
            const int decoyClientTick = reuseCachedWorldUtilityDetails ? s_worldPrevDecoyClientTick[idx] : worldDecoyClientTick[idx];
            const int explodeTick = reuseCachedWorldUtilityDetails ? s_worldPrevExplodeTick[idx] : worldExplodeTick[idx];

            constexpr int kMinUsefulTick = 16;
            bool smokeTickValid = (smokeTick >= kMinUsefulTick && smokeTick < 20000000);
            bool infernoTickValid = (infernoTick >= kMinUsefulTick && infernoTick < 20000000);
            bool decoyTickValid = (decoyTick >= kMinUsefulTick && decoyTick < 20000000);
            bool decoyClientTickValid = (decoyClientTick >= kMinUsefulTick && decoyClientTick < 20000000);
            bool explodeTickValid = (explodeTick >= kMinUsefulTick && explodeTick < 20000000);
            const bool smokeActiveSane = (smokeActive <= 1);
            const bool smokeVolumeSane = (smokeVolumeDataReceived <= 1);
            const bool smokeSpawnedSane = (smokeEffectSpawned <= 1);
            const bool infernoPostEffectSane = (infernoInPostEffect <= 1);
            const bool infernoFireCountSane = (infernoFireCount >= 0 && infernoFireCount <= 64);
            const bool infernoLifeValid = (infernoLife > 0.05f && infernoLife < 15.0f);
            const bool infernoStrongState =
                infernoLifeValid &&
                infernoFireCountSane &&
                (infernoFireCount > 0) &&
                (!infernoPostEffectSane || infernoInPostEffect == 0);
            const bool smokeEffectState =
                smokeActiveSane &&
                (smokeActive != 0 ||
                 (smokeSpawnedSane && smokeEffectSpawned != 0) ||
                 (smokeVolumeSane && smokeVolumeDataReceived != 0));
            const bool decoyTicksAligned = decoyTickValid && decoyClientTickValid && std::abs(decoyTick - decoyClientTick) <= 8;
            const bool decoyTimingValid = decoyTickValid || decoyClientTickValid;
            const bool suspiciousSharedTicks =
                smokeTickValid &&
                infernoTickValid &&
                decoyTickValid &&
                (smokeTick == infernoTick) &&
                (smokeTick == decoyTick);
            if (suspiciousSharedTicks &&
                ((smokeTick <= 64) || !smokeActiveSane || (!isSmokeGrenade && !knownSmokeSubclass && !infernoStrongState && !isDecoyGrenade && !knownDecoySubclass))) {
                smokeTickValid = false;
                infernoTickValid = false;
                decoyTickValid = false;
                explodeTickValid = false;
                decoyClientTickValid = false;
            }
            if (smokeTickValid && !isSmokeGrenade)
                smokeTickValid = smokeSubclassKnownNow || unknownUtilityProbeCandidate;
            if (decoyTickValid && !isDecoyGrenade)
                decoyTickValid = decoySubclassKnownNow || unknownUtilityProbeCandidate;
            if (explodeTickValid && !isHeGrenade)
                explodeTickValid = heSubclassKnownNow || unknownUtilityProbeCandidate;
            if (infernoTickValid && !isInfernoGrenade && !infernoSubclassKnownNow && !infernoStrongState && !unknownUtilityProbeCandidate)
                infernoTickValid = false;

            const float infernoDurationSec = kMolotovDurationSec;
            const uint64_t infernoFallbackMs = kMolotovFallbackMs;
            const float smokeRemaining = calcRemainingFromTick(smokeTick, kSmokeDurationSec);
            const float infernoRemaining = calcRemainingFromTick(infernoTick, infernoDurationSec);
            const int decoyBestTick = decoyTickValid ? decoyTick : (decoyClientTickValid ? decoyClientTick : 0);
            const float decoyRemaining = calcRemainingFromTick(decoyBestTick, kDecoyDurationSec);
            const float explodeRemaining = calcRemainingFromTick(explodeTick, kExplosiveDurationSec);

            const bool rawSmokeEvidence =
                (isSmokeGrenade || smokeSubclassKnownNow || unknownUtilityProbeCandidate) &&
                smokeActiveSane &&
                ((smokeTickValid && (smokeRemaining > 0.0f || smokeEffectState)) ||
                 (!smokeTickValid && smokeEffectState));
            const bool infernoIdentityEvidence =
                (infernoTickValid || infernoStrongState) &&
                (isInfernoGrenade || infernoSubclassKnownNow || unknownUtilityProbeCandidate);
            bumpEvidenceCounter(rawSmokeEvidence, s_worldSmokeEvidenceCount[idx]);
            bumpEvidenceCounter(infernoIdentityEvidence, s_worldInfernoEvidenceCount[idx]);
            const bool infernoTypeConfirmed =
                isInfernoGrenade ||
                infernoSubclassKnownNow ||
                s_worldInfernoEvidenceCount[idx] >= 1u;
            const bool rawInfernoEvidence =
                infernoTypeConfirmed &&
                (infernoTickValid || infernoStrongState) &&
                (infernoRemaining > 0.0f || infernoStrongState);
            const bool rawDecoyEvidence =
                (isDecoyGrenade || decoySubclassKnownNow || unknownUtilityProbeCandidate) &&
                decoyTimingValid &&
                (decoyRemaining > 0.0f || decoyTicksAligned || decoyClientTickValid);
            const bool rawExplosiveEvidence =
                (isHeGrenade || heSubclassKnownNow || unknownUtilityProbeCandidate) &&
                explodeTickValid &&
                (explodeRemaining > 0.0f);
            const bool rawEvidence =
                rawSmokeEvidence ||
                rawInfernoEvidence ||
                rawDecoyEvidence ||
                rawExplosiveEvidence;

            if (!smokeSubclassKnownNow && rawSmokeEvidence) {
                rememberTrackedSubclass(s_worldSmokeSubclassIds, subclassId);
                smokeSubclassKnownNow = true;
            }
            if (!decoySubclassKnownNow && rawDecoyEvidence) {
                rememberTrackedSubclass(s_worldDecoySubclassIds, subclassId);
                decoySubclassKnownNow = true;
            }
            if (!heSubclassKnownNow && rawExplosiveEvidence) {
                rememberTrackedSubclass(s_worldHeSubclassIds, subclassId);
                heSubclassKnownNow = true;
            }
            if (!infernoSubclassKnownNow && rawInfernoEvidence) {
                rememberTrackedSubclass(s_worldInfernoSubclassIds, subclassId);
                infernoSubclassKnownNow = true;
            }

            bumpEvidenceCounter(rawDecoyEvidence, s_worldDecoyEvidenceCount[idx]);
            bumpEvidenceCounter(rawExplosiveEvidence, s_worldExplosiveEvidenceCount[idx]);

            const bool deferPersistentUtility =
                firstHistory ||
                (warmupWorldScan && unknownUtilityProbeCandidate);
            if (deferPersistentUtility) {
                if (wantsWorldProjectiles) {
                    const Vector3 velocity = reuseCachedWorldUtilityDetails ? s_worldPrevVelocity[idx] : worldVelocities[idx];
                    const bool finiteVelocity = isFiniteVec(velocity);
                    const float speedSq =
                        finiteVelocity
                        ? (velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z)
                        : 0.0f;
                    const bool movingProjectile = speedSq > (18.0f * 18.0f);
                    const bool likelyProjectile = !noOwner || movingProjectile;

                    if (isSmokeGrenade && !rawSmokeEvidence && likelyProjectile)
                        pushWorldMarker(WorldMarkerType::SmokeProjectile, pos, itemId, 0.0f, nowUs + 300000);
                    else if (isInfernoGrenade && !rawInfernoEvidence && likelyProjectile)
                        pushWorldMarker(WorldMarkerType::MolotovProjectile, pos, itemId, 0.0f, nowUs + 300000);
                    else if (isDecoyGrenade && !rawDecoyEvidence && likelyProjectile)
                        pushWorldMarker(WorldMarkerType::DecoyProjectile, pos, itemId, 0.0f, nowUs + 300000);

                    if (subclassId != 0u && isInfernoGrenade && likelyProjectile)
                        rememberTrackedSubclass(s_worldMolotovSubclassIds, subclassId);
                }

                s_worldPrevPos[idx] = pos;
                s_worldPrevSmokeTick[idx] = smokeTick;
                s_worldPrevSmokeActive[idx] = smokeActive;
                s_worldPrevSmokeVolumeDataReceived[idx] = smokeVolumeDataReceived;
                s_worldPrevSmokeEffectSpawned[idx] = smokeEffectSpawned;
                s_worldPrevInfernoTick[idx] = infernoTick;
                s_worldPrevInfernoLife[idx] = infernoLife;
                s_worldPrevInfernoFireCount[idx] = infernoFireCount;
                s_worldPrevInfernoInPostEffect[idx] = infernoInPostEffect;
                s_worldPrevDecoyTick[idx] = decoyTick;
                s_worldPrevDecoyClientTick[idx] = decoyClientTick;
                s_worldPrevExplodeTick[idx] = explodeTick;
                s_worldPrevVelocity[idx] = reuseCachedWorldUtilityDetails ? s_worldPrevVelocity[idx] : worldVelocities[idx];
                continue;
            }

            const bool smokeSignal = wantsWorldUtilityMarkers && rawSmokeEvidence && ((smokeRemaining > 0.0f) || s_worldSmokeEvidenceCount[idx] >= 1u);
            const bool infernoSignal = wantsWorldUtilityMarkers && rawInfernoEvidence && ((infernoRemaining > 0.0f) || s_worldInfernoEvidenceCount[idx] >= 1u);
            const bool decoySignal = wantsWorldUtilityMarkers && rawDecoyEvidence && ((decoyRemaining > 0.0f) || s_worldDecoyEvidenceCount[idx] >= 1u);
            const bool explosiveSignal = wantsWorldUtilityMarkers && rawExplosiveEvidence && ((explodeRemaining > 0.0f) || s_worldExplosiveEvidenceCount[idx] >= 1u);

            if (subclassId != 0u) {
                if (isSmokeGrenade && (smokeSignal || smokeTickValid))
                    rememberTrackedSubclass(s_worldSmokeSubclassIds, subclassId);
                else if (isDecoyGrenade && (decoySignal || decoyTickValid || decoyTicksAligned))
                    rememberTrackedSubclass(s_worldDecoySubclassIds, subclassId);
                else if (isHeGrenade && (explosiveSignal || explodeTickValid))
                    rememberTrackedSubclass(s_worldHeSubclassIds, subclassId);
                else if (infernoSignal && !isInfernoGrenade)
                    rememberTrackedSubclass(s_worldInfernoSubclassIds, subclassId);
            }

            if (kDebugWorldUtility) {
                if (smokeTickValid) ++dbgRawSmoke;
                if (infernoTickValid || infernoStrongState) ++dbgRawInferno;
                if (decoyTickValid) ++dbgRawDecoy;
                if (explodeTickValid) ++dbgRawExplosive;
                if (rawEvidence) ++dbgEvidenceEntities;
                if (smokeSignal) ++dbgSignalSmoke;
                if (infernoSignal) ++dbgSignalInferno;
                if (decoySignal) ++dbgSignalDecoy;
                if (explosiveSignal) ++dbgSignalExplosive;
                if (rawEvidence && !smokeSignal && !infernoSignal && !decoySignal && !explosiveSignal && dbgNoSignalSamples < 3) {
                    DebugSample& s = dbgSamples[dbgNoSignalSamples++];
                    s.idx = idx;
                    s.itemId = itemId;
                    s.owner = owner;
                    s.smokeTick = smokeTick;
                    s.infernoTick = infernoTick;
                    s.decoyTick = decoyTick;
                    s.decoyClientTick = decoyClientTick;
                    s.explodeTick = explodeTick;
                    s.infernoFireCount = infernoFireCount;
                    s.smokeActive = static_cast<int>(smokeActive);
                    s.smokeRemaining = smokeRemaining;
                    s.infernoRemaining = infernoRemaining;
                    s.decoyRemaining = decoyRemaining;
                    s.explodeRemaining = explodeRemaining;
                }
            }

            pushUtility(smokeSignal, smokeRemaining, s_worldSmokeLatched[idx], s_worldSmokeStartUs[idx], kSmokeFallbackMs, WorldMarkerType::Smoke, 45, pos, 0.0f);
            pushUtility(infernoSignal, infernoRemaining, s_worldInfernoLatched[idx], s_worldInfernoStartUs[idx], infernoFallbackMs, WorldMarkerType::Inferno, isIncendiaryGrenade ? 48 : (isMolotovGrenade ? 46 : 0), pos, infernoLife);
            pushUtility(decoySignal, decoyRemaining, s_worldDecoyLatched[idx], s_worldDecoyStartUs[idx], kDecoyFallbackMs, WorldMarkerType::Decoy, 47, pos, 0.0f);
            pushUtility(explosiveSignal, explodeRemaining, s_worldExplosiveLatched[idx], s_worldExplosiveStartUs[idx], kExplosiveFallbackMs, WorldMarkerType::Explosive, 44, pos, 0.0f);

            if (wantsWorldProjectiles) {
                const Vector3 velocity = reuseCachedWorldUtilityDetails ? s_worldPrevVelocity[idx] : worldVelocities[idx];
                const bool finiteVelocity = isFiniteVec(velocity);
                const float speedSq =
                    finiteVelocity
                    ? (velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z)
                    : 0.0f;
                const bool movingProjectile = speedSq > (18.0f * 18.0f);
                const bool likelyProjectile = !noOwner || movingProjectile;

                if (isSmokeGrenade && !smokeSignal && likelyProjectile)
                    pushWorldMarker(WorldMarkerType::SmokeProjectile, pos, itemId, 0.0f, nowUs + 300000);
                else if (isInfernoGrenade && !infernoSignal && likelyProjectile)
                    pushWorldMarker(WorldMarkerType::MolotovProjectile, pos, itemId, 0.0f, nowUs + 300000);
                else if (isDecoyGrenade && !decoySignal && likelyProjectile)
                    pushWorldMarker(WorldMarkerType::DecoyProjectile, pos, itemId, 0.0f, nowUs + 300000);

                if (subclassId != 0u && isInfernoGrenade && likelyProjectile)
                    rememberTrackedSubclass(s_worldMolotovSubclassIds, subclassId);
            }

            s_worldPrevPos[idx] = pos;
            s_worldPrevSmokeTick[idx] = smokeTick;
            s_worldPrevSmokeActive[idx] = smokeActive;
            s_worldPrevSmokeVolumeDataReceived[idx] = smokeVolumeDataReceived;
            s_worldPrevSmokeEffectSpawned[idx] = smokeEffectSpawned;
            s_worldPrevInfernoTick[idx] = infernoTick;
            s_worldPrevInfernoLife[idx] = infernoLife;
            s_worldPrevInfernoFireCount[idx] = infernoFireCount;
            s_worldPrevInfernoInPostEffect[idx] = infernoInPostEffect;
            s_worldPrevDecoyTick[idx] = decoyTick;
            s_worldPrevDecoyClientTick[idx] = decoyClientTick;
            s_worldPrevExplodeTick[idx] = explodeTick;
            s_worldPrevVelocity[idx] = reuseCachedWorldUtilityDetails ? s_worldPrevVelocity[idx] : worldVelocities[idx];
