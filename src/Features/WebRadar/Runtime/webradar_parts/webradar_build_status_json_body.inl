    std::lock_guard<std::mutex> lock(mutex_);
    size_t streamClientCount = 0;
    size_t wsClientCount = 0;
    {
        std::lock_guard<std::mutex> streamLock(streamMutex_);
        streamClientCount = streamClients_.size();
    }
    {
        std::lock_guard<std::mutex> wsLock(wsMutex_);
        for (const auto& client : wsClients_) {
            if (client && client->active.load(std::memory_order_relaxed))
                ++wsClientCount;
        }
    }
    nlohmann::json status = {
        {"enabled", stats_.enabled},
        {"server_listening", stats_.serverListening},
        {"listen_port", stats_.listenPort},
        {"stream_clients", streamClientCount + wsClientCount},
        {"sse_clients", streamClientCount},
        {"websocket_clients", wsClientCount},
        {"sent_packets", stats_.sentPackets},
        {"failed_packets", stats_.failedPackets},
        {"served_requests", stats_.servedRequests},
        {"live_poll_requests", stats_.livePollRequests},
        {"last_attempt_unix_ms", stats_.lastAttemptUnixMs},
        {"last_update_unix_ms", stats_.lastUpdateUnixMs},
        {"last_request_unix_ms", stats_.lastRequestUnixMs},
        {"last_payload_rows", stats_.lastPayloadRows},
        {"last_payload_bytes", stats_.lastPayloadBytes},
        {"last_legacy_payload_bytes", stats_.lastLegacyPayloadBytes},
        {"last_compact_payload_bytes", stats_.lastCompactPayloadBytes},
        {"avg_payload_bytes", stats_.avgPayloadBytes},
        {"max_payload_bytes", stats_.maxPayloadBytes},
        {"send_hz", stats_.sendHz},
        {"target_hz", stats_.targetHz},
        {"bytes_out_per_sec", stats_.bytesOutPerSec},
        {"total_bytes_out", stats_.totalBytesOut},
        {"serialize_us_avg", stats_.serializeUsAvg},
        {"serialize_us_peak", stats_.serializeUsPeak},
        {"broadcast_us_avg", stats_.broadcastUsAvg},
        {"coalesced_frames", stats_.coalescedFrames},
        {"dropped_frames_slow_client", stats_.droppedFramesSlowClient},
        {"active_map", stats_.activeMap},
        {"status_text", stats_.statusText},
    };
    return status.dump();
