    WSADATA wsaData = {};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.serverListening = false;
        stats_.statusText = "WEBRadar socket init failed";
        ++stats_.failedPackets;
        return;
    }

    while (running_.load(std::memory_order_relaxed)) {
        const uint16_t targetPort = NormalizePort(requestedPort_.load(std::memory_order_relaxed));
        SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSocket == INVALID_SOCKET) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stats_.serverListening = false;
                stats_.listenPort = targetPort;
                stats_.statusText = "WEBRadar socket create failed";
                ++stats_.failedPackets;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
            continue;
        }

        const BOOL reuseAddr = TRUE;
        setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuseAddr), sizeof(reuseAddr));

        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(targetPort);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(listenSocket, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR ||
            listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stats_.serverListening = false;
                stats_.listenPort = targetPort;
                stats_.statusText = "WEBRadar port bind failed";
                ++stats_.failedPackets;
            }
            closesocket(listenSocket);
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
            continue;
        }

        const uintptr_t socketValue = static_cast<uintptr_t>(listenSocket);
        listenSocket_.store(socketValue, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stats_.serverListening = true;
            stats_.listenPort = targetPort;
            if (!stats_.enabled)
                stats_.statusText = "Server listening, WEBRadar disabled";
            else if (stats_.lastUpdateUnixMs == 0)
                stats_.statusText = "Server listening, waiting for snapshot";
        }

        bool restartRequested = false;
        while (running_.load(std::memory_order_relaxed)) {
            if (requestedPort_.load(std::memory_order_relaxed) != targetPort ||
                listenSocket_.load(std::memory_order_relaxed) != socketValue) {
                restartRequested = true;
                break;
            }

            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(listenSocket, &readSet);

            timeval timeout = {};
            timeout.tv_sec = 0;
            timeout.tv_usec = 250000;

            const int selectResult = select(0, &readSet, nullptr, nullptr, &timeout);
            if (!running_.load(std::memory_order_relaxed))
                break;
            if (selectResult == SOCKET_ERROR) {
                restartRequested = true;
                break;
            }
            if (selectResult == 0 || !FD_ISSET(listenSocket, &readSet))
                continue;

            SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
            if (clientSocket == INVALID_SOCKET) {
                restartRequested = running_.load(std::memory_order_relaxed);
                break;
            }

            const BOOL noDelay = TRUE;
            setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));

            const DWORD recvTimeoutMs = 500;
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&recvTimeoutMs), sizeof(recvTimeoutMs));

            const DWORD sendTimeoutMs = 1200;
            setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&sendTimeoutMs), sizeof(sendTimeoutMs));

            bool keepOpen = false;
            HandleHttpClient(static_cast<uintptr_t>(clientSocket), &keepOpen);
            if (!keepOpen) {
                shutdown(clientSocket, SD_BOTH);
                closesocket(clientSocket);
            }
        }

        const uintptr_t currentHandle = listenSocket_.exchange(0);
        if (currentHandle == socketValue) {
            shutdown(listenSocket, SD_BOTH);
            closesocket(listenSocket);
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            stats_.serverListening = false;
            if (!running_.load(std::memory_order_relaxed))
                stats_.statusText = "WEBRadar listener stopped";
            else if (restartRequested)
                stats_.statusText = "WEBRadar listener restarting";
        }

        if (!running_.load(std::memory_order_relaxed))
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }

    WSACleanup();
