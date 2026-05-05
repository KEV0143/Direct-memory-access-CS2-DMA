#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

namespace webradar::remote {

struct Settings {
    bool enabled = false;
    std::string host;
    int webPort = 8080;
    int sshPort = 22;
    std::string login = "root";
    std::string password;
    std::string remotePath = "/opt/kevqdma-webradar";
};

struct Stats {
    bool enabled = false;
    bool configured = false;
    bool connected = false;
    uint64_t sentPackets = 0;
    uint64_t sentBytes = 0;
    uint64_t queueDrops = 0;
    uint64_t reconnects = 0;
    uint64_t lastSendUnixMs = 0;
    int lastPingMs = -1;
    std::string lastError;
};

void Start();
void Stop();
void Configure(const Settings& settings);
void Publish(const std::string& compactPayload);
Stats GetStats();
bool HasActiveConsumerDemand();

bool LoadSettings(std::string_view profileName);
bool SaveSettings(std::string_view profileName);

bool TestPing(const Settings& settings, int* outMs, std::string* outError);
bool TestHttpStatus(const Settings& settings, std::string* outError);
bool TestSshConnection(const Settings& settings, std::string* outError);
bool CheckServerReady(const Settings& settings, std::string* outError);
bool ExportDeployPackage(const Settings& settings, std::filesystem::path* outPath, std::string* outError);
using DeployProgressCallback = std::function<void(std::string_view)>;
bool DeployToServer(const Settings& settings, std::filesystem::path* outPath, std::string* outError, DeployProgressCallback progress = {});
Settings CaptureSettingsFromGlobals();
void ApplySettingsToGlobals(const Settings& settings);

}
