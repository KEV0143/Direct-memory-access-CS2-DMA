    std::string stickyMapName;
    uint64_t stickyMapStampMs = 0;
    uint64_t frameSeq = 0;
    int nextWaitMs = 16;
    std::string lastBroadcastPayload;
    uint64_t lastSeenSnapshotVersion = 0;
    uint64_t lastSeenSettingsVersion = 0;

    while (running_.load(std::memory_order_relaxed)) {
        SettingsSnapshot settings = {};
        esp::WebRadarSnapshot snapshot = {};
        bool hasSnapshot = false;
        uint64_t snapshotVersion = 0;
        uint64_t settingsVersion = 0;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            const int waitMs = std::clamp(nextWaitMs, 4, 2000);
            cv_.wait_for(lock, std::chrono::milliseconds(waitMs), [this, lastSeenSnapshotVersion, lastSeenSettingsVersion] {
                return !running_.load(std::memory_order_relaxed) ||
                    snapshotVersion_ != lastSeenSnapshotVersion ||
                    settingsVersion_ != lastSeenSettingsVersion;
            });

            if (!running_.load(std::memory_order_relaxed))
                break;

            settings = settings_;
            settingsVersion = settingsVersion_;
            stats_.enabled = settings.enabled;
            stats_.listenPort = settings.listenPort;
            stats_.lastAttemptUnixMs = UnixNowMs();
            if (hasSnapshot_) {
                snapshot = latestSnapshot_;
                snapshotVersion = snapshotVersion_;
                hasSnapshot = true;
            }
        }
        lastSeenSettingsVersion = settingsVersion;
        if (hasSnapshot)
            lastSeenSnapshotVersion = snapshotVersion;

        const uint64_t nowMs = UnixNowMs();
        nextWaitMs = settings.intervalMs;

        std::string mapName = settings.mapOverride;
        if (mapName.empty() && hasSnapshot)
            mapName = ResolveMapName(snapshot);
        if (!mapName.empty() && mapName != "unknown") {
            stickyMapName = mapName;
            stickyMapStampMs = nowMs;
        }
        const bool stickyMapEligible =
            hasSnapshot &&
            (snapshot.hasMinimapBounds || IsValidWorldVec(snapshot.localPos));
        if ((mapName.empty() || mapName == "unknown") &&
            stickyMapEligible &&
            !stickyMapName.empty() &&
            (nowMs - stickyMapStampMs) <= 2500) {
            mapName = stickyMapName;
        }
        if (mapName.empty())
            mapName = "unknown";

        if (!hasSnapshot) {
            const std::string payloadJson = BuildFallbackLiveJson(settings, mapName, nowMs);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stats_.activeMap = mapName;
                stats_.lastPayloadRows = 0;
                latestPayloadJson_ = payloadJson;
                ++latestPayloadVersion_;
                stats_.statusText = stats_.serverListening
                    ? (settings.enabled ? "Server listening, waiting for ESP snapshot" : "Server listening, WEBRadar disabled")
                    : (settings.enabled ? "Waiting for HTTP listener" : "WEBRadar disabled");
            }
            cv_.notify_all();
            if (lastBroadcastPayload != payloadJson) {
                BroadcastLivePayload(payloadJson);
                lastBroadcastPayload = payloadJson;
            }
            continue;
        }

        if (!settings.enabled) {
            const std::string payloadJson = BuildFallbackLiveJson(settings, mapName, nowMs);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stats_.activeMap = mapName;
                stats_.lastPayloadRows = 0;
                latestPayloadJson_ = payloadJson;
                ++latestPayloadVersion_;
                stats_.statusText = stats_.serverListening
                    ? "Server listening, WEBRadar disabled"
                    : "WEBRadar disabled";
            }
            cv_.notify_all();
            if (lastBroadcastPayload != payloadJson) {
                BroadcastLivePayload(payloadJson);
                lastBroadcastPayload = payloadJson;
            }
            continue;
        }

        ++frameSeq;
        size_t entityCount = 0;

        nlohmann::json live;
        const uint64_t captureTimeMs = snapshot.captureTickMs > 0 ? snapshot.captureTickMs : nowMs;
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

        if (snapshot.bomb.planted && IsValidWorldVec(snapshot.bomb.position)) {
            const float blowLeft = std::max(0.0f, snapshot.bomb.blowTime - snapshot.bomb.currentGameTime);
            const float defuseLeft = std::max(0.0f, snapshot.bomb.defuseEndTime - snapshot.bomb.currentGameTime);
            const bool isDefused = !snapshot.bomb.ticking && snapshot.bomb.planted &&
                snapshot.bomb.blowTime > 0.0f && blowLeft <= 0.01f;

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
            ++entityCount;
        } else if (snapshot.bomb.dropped && IsValidWorldVec(snapshot.bomb.position)) {
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
                worldGrenades.push_back({
                    {"type", typeStr},
                    {"position", {{"x", wm.position.x}, {"y", wm.position.y}, {"z", wm.position.z}}},
                    {"life", wm.lifeRemainingSec}
                });
            }
            live["m_world_grenades"] = worldGrenades;
        }

        if (IsFiniteVec(snapshot.localPos)) {
            nlohmann::json me;
            me["m_idx"] = 0;
            const std::string localName = SafePlayerName(snapshot.localName, sizeof(snapshot.localName));
            me["m_name"] = localName.empty() ? "Local Player" : localName;
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
            ++entityCount;
        }

        for (size_t slot = 0; slot < snapshot.players.size(); ++slot) {
            const auto& player = snapshot.players[slot];
            if (!player.valid || !IsFiniteVec(player.position))
                continue;

            nlohmann::json row;
            row["m_idx"] = static_cast<int>(slot) + 1;
            row["m_name"] = SafePlayerName(player.name, sizeof(player.name));
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
            ++entityCount;
        }

        const std::string payloadJson = live.dump();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            latestPayloadJson_ = payloadJson;
            ++latestPayloadVersion_;
            stats_.lastUpdateUnixMs = nowMs;
            stats_.lastPayloadRows = entityCount;
            stats_.activeMap = mapName;
            stats_.statusText = stats_.serverListening
                ? (entityCount > 0 ? ("Serving " + mapName) : "Server ready, waiting for entities")
                : "Waiting for HTTP listener";
            ++stats_.sentPackets;
        }
        cv_.notify_all();

        if (lastBroadcastPayload != payloadJson) {
            BroadcastLivePayload(payloadJson);
            lastBroadcastPayload = payloadJson;
        }
    }
