        static uint64_t s_commitPlayerResetSerial = 0;
        static uint64_t s_zeroPawnSinceUs[64] = {};
        static uint64_t s_coreInvalidSinceUs[64] = {};
        static uint64_t s_deadReadSinceUs[64] = {};
        static uintptr_t s_deadReadPawn[64] = {};
        static uint64_t s_lastAliveCoreReadUs[64] = {};
        const uint64_t commitPlayerResetSerial = s_sceneResetSerial.load(std::memory_order_relaxed);
        if (s_commitPlayerResetSerial != commitPlayerResetSerial) {
            s_commitPlayerResetSerial = commitPlayerResetSerial;
            memset(s_zeroPawnSinceUs, 0, sizeof(s_zeroPawnSinceUs));
            memset(s_coreInvalidSinceUs, 0, sizeof(s_coreInvalidSinceUs));
            memset(s_deadReadSinceUs, 0, sizeof(s_deadReadSinceUs));
            memset(s_deadReadPawn, 0, sizeof(s_deadReadPawn));
            memset(s_lastAliveCoreReadUs, 0, sizeof(s_lastAliveCoreReadUs));
        }

        auto clearPendingPlayerState = [&](int idx) {
            s_zeroPawnSinceUs[idx] = 0;
            s_coreInvalidSinceUs[idx] = 0;
            s_deadReadSinceUs[idx] = 0;
            s_deadReadPawn[idx] = 0;
        };

        auto invalidatePlayerSlot = [&](int idx) {
            const bool hadTrackedPlayer = s_players[idx].valid || s_players[idx].pawn != 0;
            s_playerInvalidReadStreak[idx] = 0;
            refreshed[idx] = false;
            s_players[idx] = {};
            s_prevRawPlayerPosReady[idx] = false;
            clearPendingPlayerState(idx);
            s_lastAliveCoreReadUs[idx] = 0;
            if (hadTrackedPlayer)
                publishCoreImmediately = true;
        };

        
        
        
        constexpr uint64_t kCoreStaleHoldUs = 1200000;
        constexpr uint64_t kCoreStaleHoldAfterResetUs = 1800000;
        constexpr uint64_t kZeroPawnGraceUs = 1400000;
        constexpr uint64_t kDeathConfirmUs = 80000;
        constexpr float kRespawnTeleportDistance2D = 512.0f;
        const uint64_t recentResetUs = s_lastSceneResetUs.load(std::memory_order_relaxed);
        const bool recentStructuralReset =
            recentResetUs > 0 &&
            nowUs > recentResetUs &&
            (nowUs - recentResetUs) <= 4000000u;
        auto& s_deathConfirmCount = s_playerDeathConfirmCount;

        for (int i = 0; i < 64; i++) {
            const bool isLocalSlot =
                localPlayerIndexValid &&
                localPlayerIndexHasLiveEvidence &&
                (i == localPlayerIndex);
            const bool isLocalControllerSlot =
                localControllerMaskBit > 0 &&
                localControllerMaskBit <= 64 &&
                i == (localControllerMaskBit - 1);
            const bool isLocalPawn =
                (localPawnResolved != 0) &&
                (pawns[i] == localPawnResolved);
            const bool localControllerPawnHandleValid =
                localControllerPawnHandle != 0u &&
                localControllerPawnHandle != 0xFFFFFFFFu;
            const bool isLocalHandle =
                localControllerPawnHandleValid &&
                ((pawnHandles[i] & kEntityHandleMask) ==
                 (localControllerPawnHandle & kEntityHandleMask));
            const bool teamLiveResolved = liveTeamReads[i];
            const bool teamValid =
                (teams[i] == 2 || teams[i] == 3);
            const bool positionValid =
                isValidWorldPos(positions[i]);
            const bool coreFreshThisFrame =
                coreReadFresh[i] &&
                coreReadPlausible[i] &&
                pawns[i] != 0;
            const bool coreAliveThisFrame =
                coreFreshThisFrame &&
                coreReadAlive[i] &&
                healths[i] > 0 &&
                lifeStates[i] == 0;
            const bool coreDeadThisFrame =
                coreFreshThisFrame &&
                !coreAliveThisFrame &&
                (healths[i] <= 0 || lifeStates[i] != 0);
            if (coreFreshThisFrame) {
                if (coreAliveThisFrame)
                    s_lastAliveCoreReadUs[i] = nowUs;
            }
            const bool hadTrackedPlayer =
                s_players[i].valid &&
                s_players[i].pawn != 0;
            const bool pawnChangedThisFrame =
                hadTrackedPlayer &&
                s_players[i].pawn != pawns[i];
            const bool coreLooksInvalid =
                !coreAliveThisFrame ||
                !teamValid ||
                !positionValid;
            const bool structuralLiveEvidence =
                pawns[i] != 0 &&
                coreFreshThisFrame &&
                teamValid &&
                positionValid;

            if (isLocalSlot || isLocalControllerSlot || isLocalPawn || isLocalHandle) {
                invalidatePlayerSlot(i);
                s_deathConfirmCount[i] = 0;
                continue;
            }
            if (!pawns[i]) {
                if (s_players[i].valid && s_players[i].pawn != 0) {
                    if (s_zeroPawnSinceUs[i] == 0)
                        s_zeroPawnSinceUs[i] = nowUs;
                    if ((nowUs - s_zeroPawnSinceUs[i]) < kZeroPawnGraceUs) {
                        if (!sceneSettling || recentStructuralReset)
                            ++s_playerInvalidReadStreak[i];
                        refreshed[i] = true;
                        continue;
                    }
                }
                clearPendingPlayerState(i);
                invalidatePlayerSlot(i);
                s_deathConfirmCount[i] = 0;
                continue;
            }
            s_zeroPawnSinceUs[i] = 0;
            if (localTeamLikelySwitched && !teamLiveResolved) {
                if (s_players[i].valid && s_players[i].pawn == pawns[i]) {
                    if (s_coreInvalidSinceUs[i] == 0)
                        s_coreInvalidSinceUs[i] = nowUs;
                    ++s_playerInvalidReadStreak[i];
                    refreshed[i] = true;
                    continue;
                }
            }
            if (coreLooksInvalid) {
                const bool missingFreshCore = !coreFreshThisFrame;
                const uint64_t coreStaleHoldUs =
                    (sceneSettling || recentStructuralReset)
                    ? kCoreStaleHoldAfterResetUs
                    : kCoreStaleHoldUs;
                const bool canTemporarilyHoldAliveCore =
                    missingFreshCore &&
                    !pawnChangedThisFrame &&
                    s_players[i].valid &&
                    s_players[i].pawn == pawns[i] &&
                    s_lastAliveCoreReadUs[i] > 0 &&
                    nowUs >= s_lastAliveCoreReadUs[i] &&
                    (nowUs - s_lastAliveCoreReadUs[i]) <= coreStaleHoldUs;
                if (canTemporarilyHoldAliveCore) {
                    if (s_coreInvalidSinceUs[i] == 0)
                        s_coreInvalidSinceUs[i] = nowUs;
                    ++s_playerInvalidReadStreak[i];
                    refreshed[i] = true;
                    continue;
                }

                const bool looksDeadThisFrame =
                    structuralLiveEvidence &&
                    coreDeadThisFrame &&
                    (healths[i] <= 0 || lifeStates[i] != 0);
                if (looksDeadThisFrame) {
                    if (s_deadReadPawn[i] != pawns[i]) {
                        s_deadReadPawn[i] = pawns[i];
                        s_deadReadSinceUs[i] = nowUs;
                    } else if (s_deadReadSinceUs[i] == 0) {
                        s_deadReadSinceUs[i] = nowUs;
                    }
                    ++s_deathConfirmCount[i];
                } else {
                    s_deathConfirmCount[i] = 0;
                    s_deadReadSinceUs[i] = 0;
                    s_deadReadPawn[i] = 0;
                }
                const bool confirmedDead =
                    looksDeadThisFrame &&
                    s_deathConfirmCount[i] >= 2 &&
                    s_deadReadSinceUs[i] > 0 &&
                    nowUs > s_deadReadSinceUs[i] &&
                    (nowUs - s_deadReadSinceUs[i]) >= kDeathConfirmUs;
                if (confirmedDead && s_players[i].valid && s_players[i].pawn == pawns[i]) {
                    s_players[i].health = 0;
                    s_players[i].hasBones = false;
                    memset(s_players[i].bones, 0, sizeof(s_players[i].bones));
                }
                if (!confirmedDead &&
                    !pawnChangedThisFrame &&
                    s_players[i].valid &&
                    s_players[i].pawn != 0) {
                    if (s_coreInvalidSinceUs[i] == 0)
                        s_coreInvalidSinceUs[i] = nowUs;
                    const uint64_t invalidGraceUs = looksDeadThisFrame
                        ? kDeathConfirmUs
                        : coreStaleHoldUs;
                    if ((nowUs - s_coreInvalidSinceUs[i]) < invalidGraceUs) {
                        if (!sceneSettling || recentStructuralReset)
                            ++s_playerInvalidReadStreak[i];
                        refreshed[i] = true;
                        continue;
                    }
                }
                clearPendingPlayerState(i);
                invalidatePlayerSlot(i);
                s_deathConfirmCount[i] = 0;
                continue;
            }
            clearPendingPlayerState(i);
            s_deathConfirmCount[i] = 0;
            s_playerInvalidReadStreak[i] = 0;
            refreshed[i] = true;
            esp::PlayerData& p = s_players[i];
            const bool hadTrackedPlayerNow = p.valid || p.pawn != 0;
            const uintptr_t previousPawn = p.pawn;
            const bool pawnChanged = !p.valid || p.pawn != pawns[i];
            const int previousHealth = p.health;
            const Vector3 previousPosition = p.position;
            if (!hadTrackedPlayerNow || previousPawn != pawns[i])
                publishCoreImmediately = true;
            if (pawnChanged) {
                s_prevRawPlayerPosReady[i] = false;
                p.money = 0;
                p.ping = 0;
                p.visible = false;
                p.scoped = false;
                p.defusing = false;
                p.hasDefuser = false;
                p.flashed = false;
                p.flashDuration = 0.0f;
                p.eyeYaw = 0.0f;
                p.spottedMask = 0;
                memset(p.name, 0, sizeof(p.name));
                p.weaponId = 0;
                p.ammoClip = -1;
                p.hasBomb = false;
                p.grenadeCount = 0;
                memset(p.grenadeIds, 0, sizeof(p.grenadeIds));
                p.hasBones = false;
                memset(p.bones, 0, sizeof(p.bones));
                p.soundUntilMs = 0;
            }
            const bool respawnedThisFrame =
                hadTrackedPlayerNow &&
                !pawnChanged &&
                previousHealth <= 0 &&
                healths[i] > 0 &&
                lifeStates[i] == 0;
            const float positionJump2D =
                (!pawnChanged && isValidWorldPos(previousPosition) && positionValid)
                ? static_cast<float>(std::hypot(
                    positions[i].x - previousPosition.x,
                    positions[i].y - previousPosition.y))
                : 0.0f;
            const bool likelyRoundRespawnTeleport =
                hadTrackedPlayerNow &&
                !pawnChanged &&
                previousHealth > 0 &&
                healths[i] > 0 &&
                lifeStates[i] == 0 &&
                positionJump2D >= kRespawnTeleportDistance2D;
            if (respawnedThisFrame || likelyRoundRespawnTeleport) {
                s_prevRawPlayerPosReady[i] = false;
                p.hasBones = false;
                memset(p.bones, 0, sizeof(p.bones));
                p.soundUntilMs = 0;
            }
            p.valid = true;
            p.pawn = pawns[i];
            p.staleFrames = 0;
            p.health = healths[i];
            p.armor = std::clamp(armors[i], 0, 100);
            p.team = teams[i];
            p.position = positions[i];
            const uint64_t previousSoundUntil = pawnChanged ? 0 : p.soundUntilMs;
            Vector3 derivedVelocity = {};
            if (!s_prevRawPlayerPosReady[i]) {
                s_prevRawPlayerPos[i] = positions[i];
                s_prevRawPlayerPosReady[i] = true;
            } else {
                const float dxMove = positions[i].x - s_prevRawPlayerPos[i].x;
                const float dyMove = positions[i].y - s_prevRawPlayerPos[i].y;
                const float dzMove = positions[i].z - s_prevRawPlayerPos[i].z;
                const float move2D = static_cast<float>(std::hypot(dxMove, dyMove));
                const uint64_t elapsedUs = (s_captureTimeUs > s_prevCaptureTimeUs && s_prevCaptureTimeUs > 0)
                    ? (s_captureTimeUs - s_prevCaptureTimeUs) : (1000000u / DATA_WORKER_HZ);
                const bool normalTick = (elapsedUs < 50000); 
                if (normalTick && elapsedUs > 0) {
                    const float dtSec = static_cast<float>(elapsedUs) / 1000000.0f;
                    if (dtSec > 0.0001f) {
                        derivedVelocity = Vector3{dxMove / dtSec, dyMove / dtSec, dzMove / dtSec};
                        const float velocity2D = static_cast<float>(std::hypot(derivedVelocity.x, derivedVelocity.y));
                        const float velocity3D = static_cast<float>(std::hypot(velocity2D, derivedVelocity.z));
                        if (!isFiniteVec(derivedVelocity) || velocity2D > 3000.0f || velocity3D > 4000.0f)
                            derivedVelocity = {};
                    }
                }
                if (move2D > 16.0f && normalTick)
                    p.soundUntilMs = nowMs + 420;
                else
                    p.soundUntilMs = previousSoundUntil;
                s_prevRawPlayerPos[i] = positions[i];
            }
            velocities[i] = derivedVelocity;
            p.velocity = derivedVelocity;
            if (p.soundUntilMs < nowMs)
                p.soundUntilMs = 0;
        }
