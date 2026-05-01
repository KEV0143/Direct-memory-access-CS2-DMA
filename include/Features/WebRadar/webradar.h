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
inline constexpr int kMinRealtimeIntervalMs = 15;
inline constexpr int kMaxRealtimeIntervalMs = 2000;
inline constexpr int kFallbackPublishIntervalMs = 1000;
inline constexpr int kPollingFallbackIntervalMs = 67;
}

struct RuntimeStats {
    bool enabled = false;
    bool serverListening = false;
    uint16_t listenPort = 0;
    uint64_t sentPackets = 0;
    uint64_t failedPackets = 0;
    uint64_t servedRequests = 0;
    uint64_t livePollRequests = 0;
    uint64_t lastAttemptUnixMs = 0;
    uint64_t lastUpdateUnixMs = 0;
    uint64_t lastRequestUnixMs = 0;
    size_t lastPayloadRows = 0;
    size_t lastPayloadBytes = 0;
    size_t lastLegacyPayloadBytes = 0;
    size_t lastCompactPayloadBytes = 0;
    double avgPayloadBytes = 0.0;
    size_t maxPayloadBytes = 0;
    double sendHz = 0.0;
    double targetHz = 0.0;
    double bytesOutPerSec = 0.0;
    uint64_t totalBytesOut = 0;
    double serializeUsAvg = 0.0;
    uint64_t serializeUsPeak = 0;
    double broadcastUsAvg = 0.0;
    uint64_t coalescedFrames = 0;
    uint64_t droppedFramesSlowClient = 0;
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
    struct StreamClient;
    struct WebSocketClient;

    struct SettingsSnapshot {
        bool enabled = false;
        int intervalMs = cfg::kMinRealtimeIntervalMs;
        uint16_t listenPort = 0;
        std::string mapOverride;
    };

    void WorkerLoop();
    void ServerLoop();
    bool HandleHttpClient(uintptr_t socketHandle, bool* keepOpen);
    void BroadcastLivePayload(const std::string& payloadJsonV1, const std::string& payloadJsonV2);
    void CloseStreamClients();
    void CloseWebSocketClients();
    void PruneWebSocketClients();
    bool RegisterWebSocketClient(uintptr_t socketHandleValue, int protocolVersion);
    void WebSocketClientLoop(WebSocketClient* client, uint64_t lastSeenVersion);
    void RecordBytesOut(size_t bytes);
    std::string BuildStatusJson() const;
    std::string BuildFallbackLiveJson(const SettingsSnapshot& settings, const std::string& mapName, uint64_t nowMs) const;
    std::string BuildFallbackLiveJsonV2(const SettingsSnapshot& settings, const std::string& mapName, uint64_t nowMs) const;

    static std::string Trim(const std::string& text);
    static uint64_t UnixNowMs();
    static std::string BuildPlayerSteamId(int slot);
    static std::string ResolveMapName(const esp::WebRadarSnapshot& snapshot);
    static std::string NormalizeMapName(const std::string& rawName);

private:
    struct WebSocketClient {
        std::atomic<uintptr_t> socketHandle{ 0 };
        std::atomic<bool> active{ true };
        int protocolVersion = 1;
        std::thread thread;
    };

    struct StreamClient {
        uintptr_t socketHandle = 0;
        int protocolVersion = 1;
    };

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    std::thread serverThread_;
    std::atomic<bool> running_{ false };
    std::atomic<uintptr_t> listenSocket_{ 0 };
    std::atomic<uint16_t> requestedPort_{ 0 };
    mutable std::mutex streamMutex_;
    std::vector<StreamClient> streamClients_;
    std::atomic<uint64_t> streamGeneration_{ 0 };
    mutable std::mutex wsMutex_;
    std::vector<std::unique_ptr<WebSocketClient>> wsClients_;

    SettingsSnapshot settings_;
    uint64_t settingsVersion_ = 1;
    esp::WebRadarSnapshot latestSnapshot_ = {};
    bool hasSnapshot_ = false;
    uint64_t snapshotVersion_ = 0;
    std::string latestPayloadJson_;
    std::string latestPayloadJsonV2_;
    uint64_t latestPayloadVersion_ = 0;
    uint64_t legacyPayloadDemandUntilMs_ = 0;
    uint64_t bytesOutWindowUnixMs_ = 0;
    uint64_t bytesOutWindowBytes_ = 0;

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
