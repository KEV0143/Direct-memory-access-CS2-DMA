        auto invalidatePlayerSlot = [&](int idx) {
            const bool hadTrackedPlayer = s_players[idx].valid || s_players[idx].pawn != 0;
            s_playerInvalidReadStreak[idx] = 0;
            refreshed[idx] = false;
            s_players[idx] = {};
            s_prevRawPlayerPosReady[idx] = false;
            if (hadTrackedPlayer)
                publishCoreImmediately = true;
        };

        
        
        
        constexpr int kGracePeriodFrames = 24;
        constexpr int kZeroPawnGraceFrames = 3;
        constexpr int kDeathConfirmFrames = 3;
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
            const bool hadTrackedPlayer =
                s_players[i].valid &&
                s_players[i].pawn != 0;
            const bool pawnChangedThisFrame =
                hadTrackedPlayer &&
                s_players[i].pawn != pawns[i];
            const bool coreLooksInvalid =
                !teamValid ||
                healths[i] <= 0 ||
                lifeStates[i] != 0 ||
                !positionValid;

            if (isLocalSlot || isLocalControllerSlot || isLocalPawn || isLocalHandle) {
                invalidatePlayerSlot(i);
                s_deathConfirmCount[i] = 0;
                continue;
            }
            if (!pawns[i]) {
                if (s_players[i].valid && s_players[i].pawn != 0 &&
                    s_playerInvalidReadStreak[i] < kZeroPawnGraceFrames) {
                    if (!sceneSettling || recentStructuralReset)
                        ++s_playerInvalidReadStreak[i];
                    refreshed[i] = true;
                    continue;
                }
                invalidatePlayerSlot(i);
                s_deathConfirmCount[i] = 0;
                continue;
            }
            if (localTeamLikelySwitched && !teamLiveResolved) {
                invalidatePlayerSlot(i);
                s_deathConfirmCount[i] = 0;
                continue;
            }
            if (coreLooksInvalid) {
                if (localTeamLikelySwitched && !teamValid) {
                    invalidatePlayerSlot(i);
                    s_deathConfirmCount[i] = 0;
                    continue;
                }
                
                
                const bool looksDeadThisFrame =
                    teamValid && (healths[i] <= 0 || lifeStates[i] != 0);
                if (looksDeadThisFrame) {
                    ++s_deathConfirmCount[i];
                } else {
                    s_deathConfirmCount[i] = 0;
                }
                const bool confirmedDead = looksDeadThisFrame && s_deathConfirmCount[i] >= kDeathConfirmFrames;
                const int invalidGraceFrames = kGracePeriodFrames;
                
                
                
                
                
                
                
                
                
                const bool canGraceBadRead =
                    !confirmedDead &&
                    !pawnChangedThisFrame &&
                    s_playerInvalidReadStreak[i] < invalidGraceFrames;
                if (canGraceBadRead) {
                    if (!sceneSettling || recentStructuralReset)
                        ++s_playerInvalidReadStreak[i];
                    refreshed[i] = true;
                    continue;
                }
                invalidatePlayerSlot(i);
                s_deathConfirmCount[i] = 0;
                continue;
            }
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
