    const SOCKET socketHandle = static_cast<SOCKET>(socketHandleValue);
    if (keepOpen)
        *keepOpen = false;

    std::string rawRequest;
    if (!ReadHttpRequest(socketHandle, &rawRequest)) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++stats_.failedPackets;
        return false;
    }

    const HttpRequest request = ParseHttpRequest(rawRequest);
    if (_stricmp(request.method.c_str(), "GET") != 0) {
        SendHttpResponse(socketHandle, 405, "text/plain; charset=utf-8", "Method not allowed");
        return false;
    }

    if (request.path == "/favicon.ico")
        return SendHttpResponse(socketHandle, 200, "image/x-icon", "");

    if (request.path == "/api/ws") {
        const bool isUpgrade = HeaderContainsToken(request.headers, "connection", "upgrade") &&
            HeaderContainsToken(request.headers, "upgrade", "websocket");
        const auto keyIt = request.headers.find("sec-websocket-key");
        const auto versionIt = request.headers.find("sec-websocket-version");
        const bool versionOk = (versionIt != request.headers.end() && Trim(versionIt->second) == "13");

        if (!isUpgrade || keyIt == request.headers.end() || !versionOk) {
            SendHttpResponse(socketHandle, 400, "text/plain; charset=utf-8", "Invalid WebSocket upgrade");
            return false;
        }

        const std::string acceptKey = BuildWebSocketAcceptKey(Trim(keyIt->second));
        if (!SendWebSocketHandshakeResponse(socketHandle, acceptKey)) {
            std::lock_guard<std::mutex> lock(mutex_);
            ++stats_.failedPackets;
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++stats_.servedRequests;
            stats_.lastRequestUnixMs = UnixNowMs();
        }

        if (!RegisterWebSocketClient(socketHandleValue)) {
            std::lock_guard<std::mutex> lock(mutex_);
            ++stats_.failedPackets;
            return false;
        }

        if (keepOpen)
            *keepOpen = true;
        return true;
    }

    if (request.path == "/api/live") {
        std::string body;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            body = latestPayloadJson_;
            ++stats_.servedRequests;
            stats_.lastRequestUnixMs = UnixNowMs();
        }
        return SendHttpResponseEx(socketHandle, 200, "application/json; charset=utf-8", body, "public, max-age=60, stale-while-revalidate=30", false);
    }

    if (request.path == "/api/stream") {
        std::string body;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            body = latestPayloadJson_;
            ++stats_.servedRequests;
            stats_.lastRequestUnixMs = UnixNowMs();
        }

        const DWORD streamSendTimeoutMs = 600;
        setsockopt(socketHandle, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&streamSendTimeoutMs), sizeof(streamSendTimeoutMs));

        const BOOL noDelay = TRUE;
        setsockopt(socketHandle, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));
        const int sseSndbuf = 8192;
        setsockopt(socketHandle, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&sseSndbuf), sizeof(sseSndbuf));

        if (!SendSseHeaders(socketHandle) || !SendSseEvent(socketHandle, "snapshot", body)) {
            std::lock_guard<std::mutex> lock(mutex_);
            ++stats_.failedPackets;
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(streamMutex_);
            streamClients_.push_back(socketHandleValue);
        }

        if (keepOpen)
            *keepOpen = true;
        return true;
    }

    if (request.path == "/api/status") {
        const std::string body = BuildStatusJson();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++stats_.servedRequests;
            stats_.lastRequestUnixMs = UnixNowMs();
        }
        return SendHttpResponse(socketHandle, 200, "application/json; charset=utf-8", body);
    }

    if (request.path == "/api/ping") {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++stats_.servedRequests;
            stats_.lastRequestUnixMs = UnixNowMs();
        }

        return SendHttpResponseEx(socketHandle, 200, "text/plain; charset=utf-8", "ok", "no-store", false);
    }

    if (request.path == "/maps.json") {
        std::string body;
        webradar::EmbeddedAsset asset{};
        if (webradar::FindEmbeddedAsset("/maps.json", &asset)) {
            body.assign(static_cast<const char*>(asset.data), asset.size);
        } else {
            const auto path = app::paths::ResolveWebRadarAssetPath("maps.json");
            if (path.empty() || !ReadEntireFile(path, &body))
                body = BuildFallbackMapsJson();
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++stats_.servedRequests;
            stats_.lastRequestUnixMs = UnixNowMs();
        }

        return SendHttpResponse(socketHandle, 200, "application/json; charset=utf-8", body);
    }

    std::string servePath = request.path;
    if (servePath.empty() || servePath == "/")
        servePath = "/index.html";

    webradar::EmbeddedAsset embeddedAsset{};
    if (webradar::FindEmbeddedAsset(servePath, &embeddedAsset)) {
        std::string body(static_cast<const char*>(embeddedAsset.data), embeddedAsset.size);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++stats_.servedRequests;
            stats_.lastRequestUnixMs = UnixNowMs();
        }

        const std::string contentType = GuessContentTypeFromUrl(servePath);
        const char* cacheControl = (servePath == "/index.html" || servePath.rfind(".js") != std::string::npos || servePath.rfind(".css") != std::string::npos)
            ? "no-store"
            : "public, max-age=86400, stale-while-revalidate=43200";
        return SendHttpResponseEx(socketHandle, 200, contentType.c_str(), body, cacheControl, false);
    }

    const auto assetPath = ResolveStaticAssetPath(request.path);
    if (assetPath.empty()) {
        SendHttpResponse(socketHandle, 404, "text/plain; charset=utf-8", "Not found");
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(assetPath, ec) || !std::filesystem::is_regular_file(assetPath, ec)) {
        SendHttpResponse(socketHandle, 404, "text/plain; charset=utf-8", "Not found");
        return false;
    }

    std::string body;
    if (!ReadEntireFile(assetPath, &body)) {
        SendHttpResponse(socketHandle, 500, "text/plain; charset=utf-8", "Failed to read asset");
        std::lock_guard<std::mutex> lock(mutex_);
        ++stats_.failedPackets;
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++stats_.servedRequests;
        stats_.lastRequestUnixMs = UnixNowMs();
    }

    const std::string contentType = GuessContentType(assetPath);
    return SendHttpResponseEx(socketHandle, 200, contentType.c_str(), body, CacheControlForRequestPath(request.path, assetPath), false);
