    nlohmann::json payload = {
        {"m_seq", nowMs},
        {"m_ts", nowMs},
        {"m_server_time", nowMs},
        {"m_capture_time", nowMs},
        {"m_map", mapName.empty() ? "unknown" : mapName},
        {"m_local_team", 0},
        {"m_updated_at", nowMs},
        {"m_players", nlohmann::json::array()},
        {"m_bomb", {
            {"m_is_planted", false},
            {"m_is_dropped", false},
            {"m_is_ticking", false},
            {"m_is_defusing", false},
            {"m_is_defused", false},
            {"m_blow_time", 0.0f},
            {"m_timer_length", 40.0f},
            {"m_defuse_time", 0.0f},
            {"m_defuse_length", 10.0f},
            {"m_seq", nowMs},
            {"m_ts", nowMs},
            {"m_position", {{"x", 0.0f}, {"y", 0.0f}, {"z", 0.0f}}}
        }}
    };
    return payload.dump();
