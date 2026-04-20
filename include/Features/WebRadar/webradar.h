#pragma once

#include "Features/ESP/esp.h"

#include <atomic>
#include <cstddef>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace webradar {

namespace cfg {
inline constexpr unsigned short kDefaultListenPort = 22006;
}

struct RuntimeStats {
    bool enabled = false;
    bool serverListening = false;
    uint16_t listenPort = 0;
    uint64_t sentPackets = 0;
    uint64_t failedPackets = 0;
    uint64_t servedRequests = 0;
    uint64_t lastAttemptUnixMs = 0;
    uint64_t lastUpdateUnixMs = 0;
    uint64_t lastRequestUnixMs = 0;
    size_t lastPayloadRows = 0;
    std::string activeMap;
    std::string statusText;
};

class WEBRadar {
public:
    WEBRadar();
    ~WEBRadar();

    WEBRadar(const WEBRadar&) = delete;
    WEBRadar& operator=(const WEBRadar&) = delete;

    void Start();
    void Stop();

    void Configure(bool enabled, int intervalMs, uint16_t listenPort, const std::string& mapOverride);
    void UpdateSnapshot(const esp::WebRadarSnapshot& snapshot);

    RuntimeStats GetStats() const;
    bool HasActiveConsumers() const;

private:
    struct WebSocketClient;

    struct SettingsSnapshot {
        bool enabled = false;
        int intervalMs = 4;
        uint16_t listenPort = 0;
        std::string mapOverride;
    };

    void WorkerLoop();
    void ServerLoop();
    bool HandleHttpClient(uintptr_t socketHandle, bool* keepOpen);
    void BroadcastLivePayload(const std::string& payloadJson);
    void CloseStreamClients();
    void CloseWebSocketClients();
    void PruneWebSocketClients();
    bool RegisterWebSocketClient(uintptr_t socketHandleValue);
    void WebSocketClientLoop(WebSocketClient* client, uint64_t lastSeenVersion);
    std::string BuildStatusJson() const;
    std::string BuildFallbackLiveJson(const SettingsSnapshot& settings, const std::string& mapName, uint64_t nowMs) const;

    static std::string Trim(const std::string& text);
    static uint64_t UnixNowMs();
    static std::string BuildPlayerSteamId(int slot);
    static std::string ResolveMapName(const esp::WebRadarSnapshot& snapshot);
    static std::string NormalizeMapName(const std::string& rawName);

private:
    struct WebSocketClient {
        std::atomic<uintptr_t> socketHandle{ 0 };
        std::atomic<bool> active{ true };
        std::thread thread;
    };

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    std::thread serverThread_;
    std::atomic<bool> running_{ false };
    std::atomic<uintptr_t> listenSocket_{ 0 };
    std::atomic<uint16_t> requestedPort_{ 0 };
    mutable std::mutex streamMutex_;
    std::vector<uintptr_t> streamClients_;
    mutable std::mutex wsMutex_;
    std::vector<std::unique_ptr<WebSocketClient>> wsClients_;

    SettingsSnapshot settings_;
    uint64_t settingsVersion_ = 1;
    esp::WebRadarSnapshot latestSnapshot_ = {};
    bool hasSnapshot_ = false;
    uint64_t snapshotVersion_ = 0;
    std::string latestPayloadJson_;
    uint64_t latestPayloadVersion_ = 0;

    RuntimeStats stats_;
};

WEBRadar& Instance();
void Initialize();
void Shutdown();
void ApplySettingsFromGlobals();
void CaptureFromEsp();
RuntimeStats GetRuntimeStats();
bool HasActiveConsumers();

} 
