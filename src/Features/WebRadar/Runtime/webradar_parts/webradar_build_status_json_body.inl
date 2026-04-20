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
        {"last_attempt_unix_ms", stats_.lastAttemptUnixMs},
        {"last_update_unix_ms", stats_.lastUpdateUnixMs},
        {"last_request_unix_ms", stats_.lastRequestUnixMs},
        {"last_payload_rows", stats_.lastPayloadRows},
        {"active_map", stats_.activeMap},
        {"status_text", stats_.statusText},
    };
    return status.dump();
