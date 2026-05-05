    std::string stickyMapName;
    uint64_t stickyMapStampMs = 0;
    uint64_t frameSeq = 0;
    int nextWaitMs = cfg::kMinRealtimeIntervalMs;
    uint64_t lastBroadcastPayloadVersion = 0;
    uint64_t lastPublishedSnapshotVersion = 0;
    uint64_t lastSeenSettingsVersion = 0;
    uint64_t lastPublishMs = 0;
    uint64_t earliestPublishMs = 0;
    uint64_t lastEntityPayloadMs = 0;
    std::string lastEntityPayloadMap;
    int lastEntityTeamT = 0;
    int lastEntityTeamCT = 0;
    constexpr uint32_t kPlayerFlagDead = 1u << 0;
    constexpr uint32_t kPlayerFlagScoped = 1u << 1;
    constexpr uint32_t kPlayerFlagFlashed = 1u << 2;
    constexpr uint32_t kPlayerFlagDefusing = 1u << 3;
    constexpr uint32_t kPlayerFlagHasDefuser = 1u << 4;
    constexpr uint32_t kPlayerFlagHasBomb = 1u << 5;
    constexpr uint32_t kPlayerFlagLocal = 1u << 6;
    constexpr uint32_t kBombFlagPlanted = 1u << 0;
    constexpr uint32_t kBombFlagDropped = 1u << 1;
    constexpr uint32_t kBombFlagTicking = 1u << 2;
    constexpr uint32_t kBombFlagDefusing = 1u << 3;
    constexpr uint32_t kBombFlagDefused = 1u << 4;
    auto appendCompactPlayer = [](
        std::string& out,
        bool& first,
        int idx,
        const std::string& name,
        int team,
        uint32_t flags,
        float yaw,
        const Vector3& position,
        int health,
        int armor,
        int money,
        int ping,
        uint16_t weaponId,
        int ammoClip,
        const uint16_t* grenadeIds,
        int grenadeCount,
        float velocityX,
        float velocityY) {
        if (!first)
            out.push_back(',');
        first = false;

        out.push_back('[');
        AppendJsonInt(out, idx);
        out.push_back(',');
        AppendJsonString(out, name);
        out.push_back(',');
        AppendJsonInt(out, team);
        out.push_back(',');
        AppendJsonInt(out, flags);
        out.push_back(',');
        AppendJsonFloat(out, yaw);
        out.push_back(',');
        AppendJsonVec3(out, position);
        out.push_back(',');
        AppendJsonInt(out, health);
        out.push_back(',');
        AppendJsonInt(out, armor);
        out.push_back(',');
        AppendJsonInt(out, money);
        out.push_back(',');
        AppendJsonInt(out, ping);
        out.push_back(',');
        AppendJsonInt(out, weaponId);
        out.push_back(',');
        AppendJsonInt(out, ammoClip);
        out.push_back(',');
        out.push_back('[');
        const int safeGrenadeCount = std::clamp(grenadeCount, 0, esp::PlayerData::kMaxGrenades);
        for (int i = 0; i < safeGrenadeCount; ++i) {
            if (i > 0)
                out.push_back(',');
            AppendJsonInt(out, grenadeIds[i]);
        }
        out.push_back(']');
        out.push_back(',');
        AppendJsonVec2(out, velocityX, velocityY);
        out.push_back(']');
    };

    while (running_.load(std::memory_order_relaxed)) {
        SettingsSnapshot settings = {};
        esp::WebRadarSnapshot snapshot = {};
        bool hasSnapshot = false;
        uint64_t snapshotVersion = 0;
        uint64_t settingsVersion = 0;
        uint64_t legacyDemandUntilMs = 0;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            const uint64_t waitNowMs = UnixNowMs();
            const bool publishThrottleActive = earliestPublishMs > waitNowMs;
            const int waitMs = publishThrottleActive
                ? std::clamp(static_cast<int>(earliestPublishMs - waitNowMs), 1, cfg::kMaxRealtimeIntervalMs)
                : std::clamp(nextWaitMs, 1, cfg::kMaxRealtimeIntervalMs);
            cv_.wait_for(lock, std::chrono::milliseconds(waitMs), [this, lastSeenSettingsVersion, lastPublishedSnapshotVersion, publishThrottleActive] {
                return !running_.load(std::memory_order_relaxed) ||
                    settingsVersion_ != lastSeenSettingsVersion ||
                    (!publishThrottleActive && hasSnapshot_ && snapshotVersion_ != lastPublishedSnapshotVersion);
            });

            if (!running_.load(std::memory_order_relaxed))
                break;

            settings = settings_;
            settingsVersion = settingsVersion_;
            stats_.enabled = settings.enabled;
            stats_.listenPort = settings.listenPort;
            stats_.lastAttemptUnixMs = UnixNowMs();
            legacyDemandUntilMs = legacyPayloadDemandUntilMs_;
            if (hasSnapshot_) {
                snapshot = latestSnapshot_;
                snapshotVersion = snapshotVersion_;
                hasSnapshot = true;
            }
        }
        const bool settingsChanged = settingsVersion != lastSeenSettingsVersion;
        lastSeenSettingsVersion = settingsVersion;

        const uint64_t nowMs = UnixNowMs();
        nextWaitMs = std::clamp(settings.intervalMs, cfg::kMinRealtimeIntervalMs, cfg::kMaxRealtimeIntervalMs);
        const bool publishEnabled = settings.enabled || remote::HasActiveConsumerDemand();

        bool snapshotHasLiveEntities = false;
        if (hasSnapshot) {
            snapshotHasLiveEntities =
                IsValidWorldVec(snapshot.localPos) ||
                ((snapshot.bomb.planted || snapshot.bomb.dropped) && IsValidWorldVec(snapshot.bomb.position)) ||
                snapshot.worldMarkerCount > 0;
            if (!snapshotHasLiveEntities) {
                for (const auto& player : snapshot.players) {
                    if (player.valid && IsFiniteVec(player.position)) {
                        snapshotHasLiveEntities = true;
                        break;
                    }
                }
            }
        }

        std::string mapName = settings.mapOverride;
        if (mapName.empty() && hasSnapshot)
            mapName = ResolveMapName(snapshot);
        if (!mapName.empty() && mapName != "unknown") {
            stickyMapName = mapName;
            stickyMapStampMs = nowMs;
        }
        const bool stickyMapEligible =
            hasSnapshot &&
            (snapshot.hasMinimapBounds || snapshotHasLiveEntities);
        if ((mapName.empty() || mapName == "unknown") &&
            stickyMapEligible &&
            !stickyMapName.empty() &&
            (nowMs - stickyMapStampMs) <= 5000) {
            mapName = stickyMapName;
        }
        if (mapName.empty())
            mapName = "unknown";

        bool hasLegacyConsumers = legacyDemandUntilMs > nowMs;
        if (!hasLegacyConsumers) {
            {
                std::lock_guard<std::mutex> streamLock(streamMutex_);
                for (const auto& client : streamClients_) {
                    if (client.protocolVersion < 2) {
                        hasLegacyConsumers = true;
                        break;
                    }
                }
            }
            if (!hasLegacyConsumers) {
                std::lock_guard<std::mutex> wsLock(wsMutex_);
                for (const auto& client : wsClients_) {
                    if (client &&
                        client->active.load(std::memory_order_relaxed) &&
                        client->protocolVersion < 2) {
                        hasLegacyConsumers = true;
                        break;
                    }
                }
            }
        }

        if (!publishEnabled) {
            std::lock_guard<std::mutex> lock(mutex_);
            stats_.activeMap = mapName;
            stats_.lastPayloadRows = 0;
            stats_.targetHz = 0.0;
            stats_.statusText = "WEBRadar disabled";
            nextWaitMs = cfg::kFallbackPublishIntervalMs;
            continue;
        }

        if (!hasSnapshot) {
            const bool holdRecentLivePayload =
                lastEntityPayloadMs > 0 &&
                nowMs >= lastEntityPayloadMs &&
                (nowMs - lastEntityPayloadMs) <= 3500 &&
                (mapName == "unknown" || mapName == lastEntityPayloadMap);
            if (!settingsChanged && holdRecentLivePayload) {
                nextWaitMs = cfg::kMinRealtimeIntervalMs;
                continue;
            }
            const uint64_t elapsedSincePublish = lastPublishMs > 0 && nowMs >= lastPublishMs ? (nowMs - lastPublishMs) : 0;
            if (!settingsChanged && lastPublishMs > 0 && elapsedSincePublish < cfg::kFallbackPublishIntervalMs) {
                nextWaitMs = cfg::kMinRealtimeIntervalMs;
                continue;
            }
            const std::string payloadJson = hasLegacyConsumers ? BuildFallbackLiveJson(settings, mapName, nowMs) : std::string();
            const std::string payloadJsonV2 = BuildFallbackLiveJsonV2(settings, mapName, nowMs);
            const size_t legacyPayloadBytes = payloadJson.size();
            const size_t compactPayloadBytes = payloadJsonV2.size();
            const size_t payloadBytes = compactPayloadBytes;
            uint64_t currentPayloadVersion = 0;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stats_.activeMap = mapName;
                stats_.lastPayloadRows = 0;
                stats_.lastPayloadBytes = payloadBytes;
                stats_.lastLegacyPayloadBytes = legacyPayloadBytes;
                stats_.lastCompactPayloadBytes = compactPayloadBytes;
                stats_.avgPayloadBytes = Ema(stats_.avgPayloadBytes, static_cast<double>(payloadBytes), 0.08);
                stats_.maxPayloadBytes = std::max(stats_.maxPayloadBytes, payloadBytes);
                stats_.targetHz = 1000.0 / static_cast<double>(cfg::kFallbackPublishIntervalMs);
                if (hasLegacyConsumers)
                    latestPayloadJson_ = payloadJson;
                latestPayloadJsonV2_ = payloadJsonV2;
                currentPayloadVersion = ++latestPayloadVersion_;
                stats_.statusText = stats_.serverListening
                    ? (settings.enabled ? "Server listening, waiting for ESP snapshot" : "Server listening, WEBRadar disabled")
                    : (settings.enabled ? "Waiting for HTTP listener" : "WEBRadar disabled");
            }
            lastPublishMs = nowMs;
            cv_.notify_all();
            if (currentPayloadVersion != lastBroadcastPayloadVersion) {
                const auto broadcastStart = std::chrono::steady_clock::now();
                BroadcastLivePayload(payloadJson, payloadJsonV2);
                const auto broadcastEnd = std::chrono::steady_clock::now();
                const uint64_t broadcastUs = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(broadcastEnd - broadcastStart).count());
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    stats_.broadcastUsAvg = Ema(stats_.broadcastUsAvg, static_cast<double>(broadcastUs), 0.10);
                }
                lastBroadcastPayloadVersion = currentPayloadVersion;
            }
            continue;
        }

        if (snapshotVersion == lastPublishedSnapshotVersion && !settingsChanged)
            continue;

        if (!settingsChanged && lastPublishMs > 0) {
            if (nowMs < earliestPublishMs) {
                nextWaitMs = std::max(1, static_cast<int>(earliestPublishMs - nowMs));
                continue;
            }
        }

        ++frameSeq;
        size_t entityCount = 0;

        const auto serializeStart = std::chrono::steady_clock::now();
        const bool buildLegacyPayload = hasLegacyConsumers;
        nlohmann::json live;
        const uint64_t captureTimeMs = snapshot.captureTickMs > 0 ? snapshot.captureTickMs : nowMs;

        if (buildLegacyPayload) {
            const double serverTimeMs = static_cast<double>(captureTimeMs);
            live["m_seq"] = frameSeq;
            live["m_ts"] = captureTimeMs;
            live["m_server_time"] = serverTimeMs;
            live["m_capture_time"] = captureTimeMs;
            live["m_map"] = mapName;
            live["m_local_team"] = snapshot.localTeam;
            live["m_updated_at"] = nowMs;
            live["m_players"] = nlohmann::json::array();
            live["m_bomb"] = {
                {"m_is_planted", false},
                {"m_is_dropped", false},
                {"m_is_ticking", false},
                {"m_is_defusing", false},
                {"m_is_defused", false},
                {"m_blow_time", 0.0f},
                {"m_timer_length", 40.0f},
                {"m_defuse_time", 0.0f},
                {"m_defuse_length", 10.0f},
                {"m_seq", frameSeq},
                {"m_ts", captureTimeMs},
                {"m_position", {{"x", 0.0f}, {"y", 0.0f}, {"z", 0.0f}}}
            };
        }
        std::string compactPlayers;
        compactPlayers.reserve(512 + snapshot.players.size() * 128);
        bool firstCompactPlayer = true;
        int entityTeamT = 0;
        int entityTeamCT = 0;
        std::string compactBomb = "[0,[0,0,0],0,40,0,10]";
        std::string compactWorld;
        compactWorld.reserve(128);
        compactWorld.push_back('[');
        bool firstCompactWorld = true;

        if (snapshot.bomb.planted && IsValidWorldVec(snapshot.bomb.position)) {
            const float blowLeft = std::max(0.0f, snapshot.bomb.blowTime - snapshot.bomb.currentGameTime);
            const float defuseLeft = std::max(0.0f, snapshot.bomb.defuseEndTime - snapshot.bomb.currentGameTime);
            const bool isDefused = !snapshot.bomb.ticking && snapshot.bomb.planted &&
                snapshot.bomb.blowTime > 0.0f && blowLeft <= 0.01f;
            uint32_t bombFlags = kBombFlagPlanted;
            if (snapshot.bomb.ticking)
                bombFlags |= kBombFlagTicking;
            if (snapshot.bomb.beingDefused)
                bombFlags |= kBombFlagDefusing;
            if (isDefused)
                bombFlags |= kBombFlagDefused;

            if (buildLegacyPayload) {
                live["m_bomb"] = {
                    {"m_is_planted", snapshot.bomb.planted},
                    {"m_is_dropped", false},
                    {"m_is_ticking", snapshot.bomb.ticking},
                    {"m_is_defusing", snapshot.bomb.beingDefused},
                    {"m_is_defused", isDefused},
                    {"m_blow_time", blowLeft},
                    {"m_timer_length", snapshot.bomb.timerLength > 1.0f ? snapshot.bomb.timerLength : 40.0f},
                    {"m_defuse_time", defuseLeft},
                    {"m_defuse_length", snapshot.bomb.defuseLength > 1.0f ? snapshot.bomb.defuseLength : 10.0f},
                    {"m_seq", frameSeq},
                    {"m_ts", captureTimeMs},
                    {"m_position", {
                        {"x", snapshot.bomb.position.x},
                        {"y", snapshot.bomb.position.y},
                        {"z", snapshot.bomb.position.z}
                    }}
                };
            }
            compactBomb.clear();
            compactBomb.push_back('[');
            AppendJsonInt(compactBomb, bombFlags);
            compactBomb.push_back(',');
            AppendJsonVec3(compactBomb, snapshot.bomb.position);
            compactBomb.push_back(',');
            AppendJsonFloat(compactBomb, blowLeft);
            compactBomb.push_back(',');
            AppendJsonFloat(compactBomb, snapshot.bomb.timerLength > 1.0f ? snapshot.bomb.timerLength : 40.0f);
            compactBomb.push_back(',');
            AppendJsonFloat(compactBomb, defuseLeft);
            compactBomb.push_back(',');
            AppendJsonFloat(compactBomb, snapshot.bomb.defuseLength > 1.0f ? snapshot.bomb.defuseLength : 10.0f);
            compactBomb.push_back(']');
            ++entityCount;
        } else if (snapshot.bomb.dropped && IsValidWorldVec(snapshot.bomb.position)) {
            if (buildLegacyPayload) {
                live["m_bomb"] = {
                    {"m_is_planted", false},
                    {"m_is_dropped", true},
                    {"m_is_ticking", false},
                    {"m_is_defusing", false},
                    {"m_is_defused", false},
                    {"m_blow_time", 0.0f},
                    {"m_timer_length", 40.0f},
                    {"m_defuse_time", 0.0f},
                    {"m_defuse_length", 10.0f},
                    {"m_seq", frameSeq},
                    {"m_ts", captureTimeMs},
                    {"m_position", {
                        {"x", snapshot.bomb.position.x},
                        {"y", snapshot.bomb.position.y},
                        {"z", snapshot.bomb.position.z}
                    }}
                };
            }
            compactBomb.clear();
            compactBomb.push_back('[');
            AppendJsonInt(compactBomb, kBombFlagDropped);
            compactBomb.push_back(',');
            AppendJsonVec3(compactBomb, snapshot.bomb.position);
            compactBomb += ",0,40,0,10]";
            ++entityCount;
        }

        {
            auto worldGrenades = nlohmann::json::array();
            for (int i = 0; i < snapshot.worldMarkerCount; ++i) {
                const auto& wm = snapshot.worldMarkers[i];
                if (!IsValidWorldVec(wm.position))
                    continue;
                const char* typeStr = "unknown";
                switch (wm.type) {
                case 1: typeStr = "smoke"; break;
                case 2: typeStr = "inferno"; break;
                case 3: typeStr = "decoy"; break;
                case 4: typeStr = "explosive"; break;
                default: continue;
                }
                if (buildLegacyPayload) {
                    worldGrenades.push_back({
                        {"type", typeStr},
                        {"position", {{"x", wm.position.x}, {"y", wm.position.y}, {"z", wm.position.z}}},
                        {"life", wm.lifeRemainingSec}
                    });
                }
                if (!firstCompactWorld)
                    compactWorld.push_back(',');
                firstCompactWorld = false;
                compactWorld.push_back('[');
                AppendJsonInt(compactWorld, wm.type);
                compactWorld.push_back(',');
                AppendJsonVec3(compactWorld, wm.position);
                compactWorld.push_back(',');
                AppendJsonFloat(compactWorld, wm.lifeRemainingSec);
                compactWorld.push_back(']');
            }
            if (buildLegacyPayload)
                live["m_world_grenades"] = worldGrenades;
        }
        compactWorld.push_back(']');

        if (IsFiniteVec(snapshot.localPos)) {
            const std::string localName = SafePlayerName(snapshot.localName, sizeof(snapshot.localName));
            const std::string localDisplayName = localName.empty() ? "Local Player" : localName;
            if (buildLegacyPayload) {
                nlohmann::json me;
                me["m_idx"] = 0;
                me["m_name"] = localDisplayName;
                me["m_team"] = snapshot.localTeam;
                me["m_color"] = 0;
                me["m_is_dead"] = snapshot.localIsDead;
                me["m_eye_angle"] = snapshot.localYaw;
                me["m_position"] = {
                    {"x", snapshot.localPos.x},
                    {"y", snapshot.localPos.y},
                    {"z", snapshot.localPos.z}
                };
                me["m_health"] = snapshot.localHealth;
                me["m_armor"] = snapshot.localArmor;
                me["m_money"] = snapshot.localMoney;
                me["m_ping"] = 0;
                me["m_scoped"] = false;
                me["m_flashed"] = false;
                me["m_defusing"] = false;
                me["m_has_defuser"] = snapshot.localHasDefuser;
                me["m_has_bomb"] = snapshot.localHasBomb;
                me["m_weapon_id"] = snapshot.localWeaponId;
                me["m_weapon"] = WeaponNameFromItemId(snapshot.localWeaponId);
                me["m_ammo_clip"] = snapshot.localAmmoClip;
                {
                    auto grenades = nlohmann::json::array();
                    for (int g = 0; g < snapshot.localGrenadeCount; ++g)
                        grenades.push_back(WeaponNameFromItemId(snapshot.localGrenadeIds[g]));
                    me["m_grenades"] = grenades;
                }
                me["m_model_name"] = TeamDefaultModelName(snapshot.localTeam);
                me["m_seq"] = frameSeq;
                me["m_ts"] = captureTimeMs;
                me["m_steam_id"] = "local";
                me["steamid"] = "local";
                me["m_is_local"] = true;
                live["m_players"].push_back(me);
            }
            uint32_t playerFlags = kPlayerFlagLocal;
            if (snapshot.localIsDead)
                playerFlags |= kPlayerFlagDead;
            if (snapshot.localHasDefuser)
                playerFlags |= kPlayerFlagHasDefuser;
            if (snapshot.localHasBomb)
                playerFlags |= kPlayerFlagHasBomb;
            appendCompactPlayer(
                compactPlayers,
                firstCompactPlayer,
                0,
                localDisplayName,
                snapshot.localTeam,
                playerFlags,
                snapshot.localYaw,
                snapshot.localPos,
                snapshot.localHealth,
                snapshot.localArmor,
                snapshot.localMoney,
                0,
                snapshot.localWeaponId,
                snapshot.localAmmoClip,
                snapshot.localGrenadeIds,
                snapshot.localGrenadeCount,
                0.0f,
                0.0f);
            if (snapshot.localTeam == 2)
                ++entityTeamT;
            else if (snapshot.localTeam == 3)
                ++entityTeamCT;
            ++entityCount;
        }

        for (size_t slot = 0; slot < snapshot.players.size(); ++slot) {
            const auto& player = snapshot.players[slot];
            if (!player.valid || !IsFiniteVec(player.position))
                continue;

            const std::string playerName = SafePlayerName(player.name, sizeof(player.name));
            if (buildLegacyPayload) {
                nlohmann::json row;
                row["m_idx"] = static_cast<int>(slot) + 1;
                row["m_name"] = playerName;
                row["m_team"] = player.team;
                row["m_color"] = static_cast<int>(slot % 6);
                row["m_is_dead"] = (player.health <= 0);
                row["m_eye_angle"] = player.eyeYaw;
                row["m_position"] = {
                    {"x", player.position.x},
                    {"y", player.position.y},
                    {"z", player.position.z}
                };
                row["m_health"] = player.health;
                row["m_armor"] = player.armor;
                row["m_money"] = player.money;
                row["m_ping"] = player.ping;
                row["m_scoped"] = player.scoped;
                row["m_flashed"] = player.flashed;
                row["m_defusing"] = player.defusing;
                row["m_has_defuser"] = player.hasDefuser;
                row["m_has_bomb"] = player.hasBomb;
                row["m_weapon_id"] = player.weaponId;
                row["m_weapon"] = WeaponNameFromItemId(player.weaponId);
                row["m_ammo_clip"] = player.ammoClip;
                {
                    auto grenades = nlohmann::json::array();
                    for (int g = 0; g < player.grenadeCount; ++g)
                        grenades.push_back(WeaponNameFromItemId(player.grenadeIds[g]));
                    row["m_grenades"] = grenades;
                }
                row["m_velocity"] = {
                    {"x", player.velocity.x},
                    {"y", player.velocity.y}
                };
                row["m_model_name"] = TeamDefaultModelName(player.team);
                row["m_seq"] = frameSeq;
                row["m_ts"] = captureTimeMs;
                row["m_steam_id"] = BuildPlayerSteamId(static_cast<int>(slot));
                row["steamid"] = row["m_steam_id"];
                row["m_is_local"] = false;
                live["m_players"].push_back(row);
            }
            uint32_t playerFlags = 0;
            if (player.health <= 0)
                playerFlags |= kPlayerFlagDead;
            if (player.scoped)
                playerFlags |= kPlayerFlagScoped;
            if (player.flashed)
                playerFlags |= kPlayerFlagFlashed;
            if (player.defusing)
                playerFlags |= kPlayerFlagDefusing;
            if (player.hasDefuser)
                playerFlags |= kPlayerFlagHasDefuser;
            if (player.hasBomb)
                playerFlags |= kPlayerFlagHasBomb;
            appendCompactPlayer(
                compactPlayers,
                firstCompactPlayer,
                static_cast<int>(slot) + 1,
                playerName,
                player.team,
                playerFlags,
                player.eyeYaw,
                player.position,
                player.health,
                player.armor,
                player.money,
                player.ping,
                player.weaponId,
                player.ammoClip,
                player.grenadeIds,
                player.grenadeCount,
                player.velocity.x,
                player.velocity.y);
            if (player.team == 2)
                ++entityTeamT;
            else if (player.team == 3)
                ++entityTeamCT;
            ++entityCount;
        }

        const std::string payloadJson = buildLegacyPayload ? live.dump() : std::string();
        std::string payloadJsonV2;
        payloadJsonV2.reserve(128 + compactPlayers.size() + compactBomb.size() + compactWorld.size());
        payloadJsonV2 += "{\"v\":2,\"seq\":";
        AppendJsonInt(payloadJsonV2, frameSeq);
        payloadJsonV2 += ",\"ts\":";
        AppendJsonInt(payloadJsonV2, captureTimeMs);
        payloadJsonV2 += ",\"map\":";
        AppendJsonString(payloadJsonV2, mapName);
        payloadJsonV2 += ",\"lt\":";
        AppendJsonInt(payloadJsonV2, snapshot.localTeam);
        payloadJsonV2 += ",\"p\":[";
        payloadJsonV2 += compactPlayers;
        payloadJsonV2 += "],\"b\":";
        payloadJsonV2 += compactBomb;
        payloadJsonV2 += ",\"w\":";
        payloadJsonV2 += compactWorld;
        payloadJsonV2.push_back('}');
        const auto serializeEnd = std::chrono::steady_clock::now();
        const uint64_t serializeUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(serializeEnd - serializeStart).count());
        const size_t legacyPayloadBytes = payloadJson.size();
        const size_t compactPayloadBytes = payloadJsonV2.size();
        const size_t payloadBytes = compactPayloadBytes;
        const uint64_t previousPublishedSnapshotVersion = lastPublishedSnapshotVersion;
        const uint64_t previousPublishMs = lastPublishMs;
        const bool holdRecentLivePayload =
            entityCount == 0 &&
            lastEntityPayloadMs > 0 &&
            nowMs >= lastEntityPayloadMs &&
            (nowMs - lastEntityPayloadMs) <= 3500 &&
            (mapName == "unknown" || mapName == lastEntityPayloadMap);
        const bool holdTransientTeamCollapse =
            lastEntityTeamT > 0 &&
            lastEntityTeamCT > 0 &&
            ((entityTeamT == 0 && entityTeamCT > 0) || (entityTeamCT == 0 && entityTeamT > 0)) &&
            lastEntityPayloadMs > 0 &&
            nowMs >= lastEntityPayloadMs &&
            (nowMs - lastEntityPayloadMs) <= 3500 &&
            (mapName == "unknown" || mapName == lastEntityPayloadMap);
        if (!settingsChanged && (holdRecentLivePayload || holdTransientTeamCollapse)) {
            nextWaitMs = cfg::kMinRealtimeIntervalMs;
            continue;
        }
        const uint64_t coalescedFrames =
            previousPublishedSnapshotVersion > 0 && snapshotVersion > previousPublishedSnapshotVersion + 1
                ? (snapshotVersion - previousPublishedSnapshotVersion - 1)
                : 0;
        const double sendHzSample =
            previousPublishMs > 0 && nowMs > previousPublishMs
                ? (1000.0 / static_cast<double>(nowMs - previousPublishMs))
                : (1000.0 / static_cast<double>(std::max(1, nextWaitMs)));
        uint64_t publishedPayloadVersion = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (buildLegacyPayload)
                latestPayloadJson_ = payloadJson;
            latestPayloadJsonV2_ = payloadJsonV2;
            publishedPayloadVersion = ++latestPayloadVersion_;
            stats_.lastUpdateUnixMs = nowMs;
            stats_.lastPayloadRows = entityCount;
            stats_.lastPayloadBytes = payloadBytes;
            stats_.lastLegacyPayloadBytes = legacyPayloadBytes;
            stats_.lastCompactPayloadBytes = compactPayloadBytes;
            stats_.avgPayloadBytes = Ema(stats_.avgPayloadBytes, static_cast<double>(payloadBytes), 0.08);
            stats_.maxPayloadBytes = std::max(stats_.maxPayloadBytes, payloadBytes);
            stats_.targetHz = 1000.0 / static_cast<double>(std::max(1, nextWaitMs));
            stats_.sendHz = Ema(stats_.sendHz, sendHzSample, 0.12);
            stats_.serializeUsAvg = Ema(stats_.serializeUsAvg, static_cast<double>(serializeUs), 0.12);
            stats_.serializeUsPeak = std::max(stats_.serializeUsPeak, serializeUs);
            stats_.coalescedFrames += coalescedFrames;
            stats_.activeMap = mapName;
            stats_.statusText = stats_.serverListening
                ? (entityCount > 0 ? ("Serving " + mapName) : "Server ready, waiting for entities")
                : "Waiting for HTTP listener";
            ++stats_.sentPackets;
        }
        lastPublishedSnapshotVersion = snapshotVersion;
        lastPublishMs = nowMs;
        earliestPublishMs = nowMs + static_cast<uint64_t>(std::max(1, nextWaitMs));
        if (entityCount > 0) {
            lastEntityPayloadMs = nowMs;
            lastEntityPayloadMap = mapName;
            lastEntityTeamT = entityTeamT;
            lastEntityTeamCT = entityTeamCT;
        }
        cv_.notify_all();

        if (publishedPayloadVersion != lastBroadcastPayloadVersion) {
            const auto broadcastStart = std::chrono::steady_clock::now();
            BroadcastLivePayload(payloadJson, payloadJsonV2);
            const auto broadcastEnd = std::chrono::steady_clock::now();
            const uint64_t broadcastUs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(broadcastEnd - broadcastStart).count());
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stats_.broadcastUsAvg = Ema(stats_.broadcastUsAvg, static_cast<double>(broadcastUs), 0.10);
            }
            lastBroadcastPayloadVersion = publishedPayloadVersion;
        }
    }
