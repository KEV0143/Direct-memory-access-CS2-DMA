    const uint64_t nowUs = TickNowUs();
    const uint64_t nowMs = nowUs / 1000u;

    uint16_t previewLocalWeaponId = s_localWeaponId;
    int previewLocalAmmoClip = s_localAmmoClip;
    bool previewLocalHasBomb = s_localHasBomb;
    bool localHasDefuserResolved = s_localHasDefuser;
    bool localIsDeadResolved = s_localIsDead;
    int localHealthResolved = s_localHealth;
    int localArmorResolved = s_localArmor;
    int localMoneyResolved = s_localMoney;
    char localNameResolved[128] = {};
    memcpy(localNameResolved, s_localName, sizeof(localNameResolved));
    uint16_t previewLocalGrenadeIds[esp::PlayerData::kMaxGrenades] = {};
    memcpy(previewLocalGrenadeIds, s_localGrenadeIds, sizeof(previewLocalGrenadeIds));
    int previewLocalGrenadeCount = std::clamp(s_localGrenadeCount, 0, esp::PlayerData::kMaxGrenades);

    const LocalPlayerIndexSource localPlayerIndexSource =
        ResolveLocalPlayerIndexSource(localControllerMaskBit, localMaskBit);
    const int localPlayerIndex = ResolveLocalPlayerIndex(localControllerMaskBit, localMaskBit);
    const bool localPlayerIndexValid = IsValidLocalPlayerIndex(localPlayerIndex);
    const bool localPlayerIndexHasLiveEvidence =
        IsLiveLocalPlayerIndexSource(localPlayerIndexSource);

    auto isLikelyViewMatrix = [](const view_matrix_t& matrix) -> bool {
        float absSum = 0.0f;
        int nonZeroCount = 0;
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                const float value = matrix[row][col];
                if (!std::isfinite(value))
                    return false;
                absSum += std::fabs(value);
                if (std::fabs(value) > 0.0001f)
                    ++nonZeroCount;
            }
        }
        return nonZeroCount >= 6 && absSum > 1.0f;
    };

    uintptr_t localPawnResolved = localPawn;
    int localTeamResolved = localTeamLiveResolved ? localTeam : 0;
    Vector3 localPosResolved = localPos;
    ResolveSharedLocalIdentityFromSlot(
        localPlayerIndex,
        localPlayerIndexValid && localPlayerIndexHasLiveEvidence,
        names,
        healths,
        lifeStates,
        armors,
        moneys,
        hasDefuserFlags,
        localNameResolved,
        localIsDeadResolved,
        localHealthResolved,
        localArmorResolved,
        localMoneyResolved,
        localHasDefuserResolved);
    if (localPlayerIndexValid && localPlayerIndexHasLiveEvidence) {
        localPawnResolved = pawns[localPlayerIndex] ? pawns[localPlayerIndex] : localPawnResolved;
        if (liveTeamReads[localPlayerIndex] &&
            (teams[localPlayerIndex] == 2 || teams[localPlayerIndex] == 3))
            localTeamResolved = teams[localPlayerIndex];
        if (isValidWorldPos(positions[localPlayerIndex])) {
            localPosResolved = positions[localPlayerIndex];
        } else if (!localIsDeadResolved && isValidWorldPos(localPos)) {
            localPosResolved = localPos;
        }
    }

    const bool localControllerPawnHandleValid =
        localControllerPawnHandle != 0u &&
        localControllerPawnHandle != 0xFFFFFFFFu;
    const bool localControllerTeamValid =
        localControllerTeam == 2 ||
        localControllerTeam == 3;
    if (localControllerTeamValid)
        localTeamResolved = localControllerTeam;

    const bool localIdentityResolved =
        localPlayerIndexHasLiveEvidence ||
        localPawnResolved != 0 ||
        localControllerPawnHandleValid ||
        localControllerMaskBit > 0;
    const bool previousLocalTeamValid = (s_localTeam == 2 || s_localTeam == 3);
    const bool localTeamTransitionDetected =
        previousLocalTeamValid &&
        ((localControllerTeamValid && localControllerTeam != s_localTeam) ||
         localTeamLikelySwitched);
    bool localTeamResolvedValid =
        localTeamResolved == 2 ||
        localTeamResolved == 3;
    const bool localPawnPointerResolved = localPawnResolved != 0;
    const bool localPresenceLiveEvidence =
        localIdentityResolved ||
        localControllerTeamValid ||
        localPawnPointerResolved;
    if (!localTeamResolvedValid &&
        previousLocalTeamValid &&
        localPresenceLiveEvidence &&
        !localTeamTransitionDetected) {
        localTeamResolved = s_localTeam;
        localTeamResolvedValid = true;
    }

    promoteInGameFrame("commit_core_state");
    bool refreshed[64] = {};
    bool publishCoreImmediately = false;

    
    if (minimapBoundsValid) {
        auto hashMapBounds = [](const Vector3& mins, const Vector3& maxs) -> uint64_t {
            auto mix = [](uint64_t seed, uint64_t value) -> uint64_t {
                seed ^= value + 0x9E3779B97F4A7C15ull + (seed << 6) + (seed >> 2);
                return seed;
            };
            auto quantize = [](float value) -> uint64_t {
                if (!std::isfinite(value))
                    return 0ull;
                const long long scaled = static_cast<long long>(std::llround(static_cast<double>(value) * 4.0));
                return static_cast<uint64_t>(scaled);
            };

            uint64_t hash = 0x84222325CBF29CE4ull;
            hash = mix(hash, quantize(mins.x));
            hash = mix(hash, quantize(mins.y));
            hash = mix(hash, quantize(mins.z));
            hash = mix(hash, quantize(maxs.x));
            hash = mix(hash, quantize(maxs.y));
            hash = mix(hash, quantize(maxs.z));
            return hash;
        };

        static uint64_t s_pendingMapFingerprint = 0;
        static uint32_t s_pendingMapFingerprintCount = 0;
        const uint64_t currentMapFingerprint = hashMapBounds(minimapMins, minimapMaxs);
        const uint64_t sceneResetAgeUs = nowUs - s_lastSceneResetUs.load(std::memory_order_relaxed);
        const bool recentSceneReset = sceneResetAgeUs < 3000000u;

        if (s_mapFingerprint == 0) {
            s_mapFingerprint = currentMapFingerprint;
            s_pendingMapFingerprint = 0;
            s_pendingMapFingerprintCount = 0;
        } else if (currentMapFingerprint != s_mapFingerprint) {
            if (recentSceneReset) {
                s_mapFingerprint = currentMapFingerprint;
                s_pendingMapFingerprint = 0;
                s_pendingMapFingerprintCount = 0;
            } else {
                if (s_pendingMapFingerprint != currentMapFingerprint) {
                    s_pendingMapFingerprint = currentMapFingerprint;
                    s_pendingMapFingerprintCount = 1;
                } else if (s_pendingMapFingerprintCount < 0xFFFFFFFFu) {
                    ++s_pendingMapFingerprintCount;
                }

                const float boundsDiffX = std::fabs(minimapMins.x - s_minimapMins.x) +
                                          std::fabs(minimapMaxs.x - s_minimapMaxs.x);
                const float boundsDiffY = std::fabs(minimapMins.y - s_minimapMins.y) +
                                          std::fabs(minimapMaxs.y - s_minimapMaxs.y);
                if (s_pendingMapFingerprintCount >= 2u &&
                    (boundsDiffX > 100.0f || boundsDiffY > 100.0f)) {
                    DmaLogPrintf(
                        "[INFO] Map fingerprint changed (bounds shifted by %.0f/%.0f)",
                        static_cast<double>(boundsDiffX),
                        static_cast<double>(boundsDiffY));
                    handleSceneTransition("map_fingerprint_changed", true, true);
                    s_mapFingerprint = currentMapFingerprint;
                    s_pendingMapFingerprint = 0;
                    s_pendingMapFingerprintCount = 0;
                }
            }
        } else {
            s_pendingMapFingerprint = 0;
            s_pendingMapFingerprintCount = 0;
        }
    }

    memcpy(s_prevPlayers, s_players, sizeof(s_prevPlayers));
    s_prevLocalPos = s_localPos;
    s_prevCaptureTimeUs = s_captureTimeUs;
    s_captureTimeUs = nowUs;

    s_localPawn = localPawnResolved;
    s_localPlayerIndex =
        (localPlayerIndexValid && localPlayerIndexHasLiveEvidence)
        ? localPlayerIndex
        : -1;
    s_localTeam = localTeamResolved;
    s_localPos = localPosResolved;
    ApplySharedLocalIdentityState(
        localNameResolved,
        localIsDeadResolved,
        localHealthResolved,
        localArmorResolved,
        localMoneyResolved,
        localHasDefuserResolved);
    s_localWeaponId = previewLocalWeaponId;
    s_localAmmoClip = previewLocalAmmoClip;
    s_localHasBomb = previewLocalHasBomb;
    s_localGrenadeCount = previewLocalGrenadeCount;
    std::copy(std::begin(previewLocalGrenadeIds), std::end(previewLocalGrenadeIds), std::begin(s_localGrenadeIds));
    if (isLikelyViewMatrix(viewMatrix))
        memcpy(&s_viewMatrix, &viewMatrix, sizeof(view_matrix_t));
    if (std::isfinite(viewAngles.x) && std::isfinite(viewAngles.y) && std::isfinite(viewAngles.z))
        s_viewAngles = viewAngles;
    if (sensValue > 0.0f && sensValue < 100.0f) {
        std::lock_guard<std::mutex> lock(s_dataMutex);
        s_sensitivity = sensValue;
    }
    if (minimapBoundsValid) {
        s_minimapMins = minimapMins;
        s_minimapMaxs = minimapMaxs;
        s_hasMinimapBounds = true;
    }
    
    s_localMaskResolved = localMaskResolved;

#include "commit_players.inl"

    
    for (int i = 0; i < 64; ++i) {
        if (refreshed[i]) {
            s_playerLastSeenMs[i] = nowMs;
            s_players[i].staleFrames = 0;
        } else if (s_players[i].valid) {
            s_players[i].staleFrames++;
            const uint64_t elapsed = (nowMs > s_playerLastSeenMs[i])
                ? (nowMs - s_playerLastSeenMs[i]) : 0;
            if (elapsed > STALE_TIMEOUT_MS) {
                s_players[i] = {};
                s_prevRawPlayerPosReady[i] = false;
            }
        }
    }

    if (publishCoreImmediately) {
        PublishCurrentSnapshot(
            SnapshotPlayers |
            SnapshotLocalView |
            SnapshotTiming);
    }
