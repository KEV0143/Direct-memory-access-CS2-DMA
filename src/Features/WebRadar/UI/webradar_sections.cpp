#include "Features/WebRadar/UI/webradar_sections.h"

#include "app/Core/globals.h"
#include "app/Config/config.h"
#include "app/UI/MenuShell/menu_utils.h"
#include "app/UI/MenuShell/ui_widgets.h"
#include "Features/WebRadar/webradar.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <thread>
#include <utility>

#include <imgui.h>

namespace
{
    constexpr float kControlWidth = 320.0f;
    constexpr int kRemoteSaveDebounceMs = 500;
    std::atomic_bool s_remoteTaskRunning{ false };
    bool s_localSettingsOpen = false;
    std::mutex s_remoteTaskStatusMutex;
    std::string s_remoteTaskStatus;
    bool s_remoteSaveDirty = false;
    std::chrono::steady_clock::time_point s_remoteLastChange{};

    constexpr const char* kMapOverrideItems[] = {
        "Auto (Detect)",
        "de_mirage",
        "de_inferno",
        "de_dust2",
        "de_nuke",
        "de_overpass",
        "de_train",
        "ar_baggage",
        "ar_shoots",
        "ar_shoots_night",
        "de_ancient",
        "de_ancient_night",
        "de_anubis",
        "de_vertigo",
        "cs_office",
        "cs_italy",
        "aim_custom"
    };

    void OpenRadarLink(const std::string& link, ui::IStatusSink& statusSink, const char* okStatus, const char* failStatus)
    {
        if (ui::menu_utils::OpenExternal(link))
            statusSink.SetStatus(okStatus);
        else
            statusSink.SetStatus(failStatus);
    }

    void SyncRemoteBuffers(ui::MenuState& state)
    {
        if (state.webRemoteInit)
            return;
        strncpy_s(state.webRemoteHost, sizeof(state.webRemoteHost), g::webRadarRemoteHost.c_str(), _TRUNCATE);
        strncpy_s(state.webRemoteLogin, sizeof(state.webRemoteLogin), g::webRadarRemoteLogin.c_str(), _TRUNCATE);
        strncpy_s(state.webRemotePassword, sizeof(state.webRemotePassword), g::webRadarRemotePassword.c_str(), _TRUNCATE);
        strncpy_s(state.webRemotePath, sizeof(state.webRemotePath), g::webRadarRemotePath.c_str(), _TRUNCATE);
        state.webRemoteInit = true;
    }

    bool InputTextRow(const char* label, const char* id, char* buffer, size_t bufferSize, ImGuiInputTextFlags flags = 0)
    {
        ImGui::PushID(id);
        ImGui::TextDisabled("%s", label);
        ImGui::SetNextItemWidth(kControlWidth);
        const bool changed = ImGui::InputText("##value", buffer, bufferSize, flags);
        ImGui::PopID();
        return changed;
    }

    void SaveRemoteSettings()
    {
        webradar::remote::SaveSettings(config::GetActiveProfile());
        webradar::remote::Configure(webradar::remote::CaptureSettingsFromGlobals());
        s_remoteSaveDirty = false;
    }

    void MarkRemoteSettingsDirty()
    {
        s_remoteSaveDirty = true;
        s_remoteLastChange = std::chrono::steady_clock::now();
    }

    void FlushRemoteSettingsIfIdle()
    {
        if (!s_remoteSaveDirty)
            return;
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_remoteLastChange);
        if (elapsed.count() >= kRemoteSaveDebounceMs)
            SaveRemoteSettings();
    }

    void ApplyRemoteBuffersToGlobals(const ui::MenuState& state)
    {
        g::webRadarRemoteHost = state.webRemoteHost;
        g::webRadarRemoteLogin = state.webRemoteLogin;
        g::webRadarRemotePassword = state.webRemotePassword;
        g::webRadarRemotePath = state.webRemotePath;
    }

    void QueueRemoteTaskStatus(std::string status)
    {
        std::lock_guard<std::mutex> lock(s_remoteTaskStatusMutex);
        s_remoteTaskStatus = std::move(status);
    }

    void FlushRemoteTaskStatus(ui::IStatusSink& statusSink)
    {
        std::string status;
        {
            std::lock_guard<std::mutex> lock(s_remoteTaskStatusMutex);
            status.swap(s_remoteTaskStatus);
        }
        if (!status.empty())
            statusSink.SetStatus(status);
    }

    std::jthread s_remoteTaskThread;

    template <typename Fn>
    void StartRemoteTask(ui::IStatusSink& statusSink, const char* startedStatus, Fn&& fn)
    {
        bool expected = false;
        if (!s_remoteTaskRunning.compare_exchange_strong(expected, true)) {
            statusSink.SetStatus("Remote task is already running.");
            return;
        }

        if (s_remoteTaskThread.joinable()) {
            s_remoteTaskThread.request_stop();
            s_remoteTaskThread.join();
        }

        statusSink.SetStatus(startedStatus);
        s_remoteTaskThread = std::jthread(
            [worker = std::forward<Fn>(fn)](std::stop_token stopToken) mutable {
                if (stopToken.stop_requested()) {
                    s_remoteTaskRunning.store(false, std::memory_order_relaxed);
                    return;
                }
                QueueRemoteTaskStatus(worker());
                s_remoteTaskRunning.store(false, std::memory_order_relaxed);
            });
    }
}

void ui::tabs::webradar_sections::RenderConnectionSection(MenuState& state, const std::string& localLink, const std::string& webLink, IStatusSink& statusSink)
{
    ui::widgets::SectionTitle("Connection");
    ui::widgets::ToggleRow("enable_local_webradar", "Enable LocalWebRadar", &g::webRadarEnabled);
    if (ui::widgets::ToggleRow("enable_web_remote", "Enable WebRadar", &g::webRadarRemoteEnabled))
        SaveRemoteSettings();

    if (ui::widgets::CompactButton("Settings LocalWebRadar", 190.0f, 32.0f))
        s_localSettingsOpen = !s_localSettingsOpen;
    ImGui::SameLine();
    if (ui::widgets::CompactButton("Settings WebRadar", 170.0f, 32.0f)) {
        g::webRadarRemoteSettingsOpen = !g::webRadarRemoteSettingsOpen;
        SaveRemoteSettings();
    }

    if (s_localSettingsOpen) {
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ui::widgets::CompactInputIntRowAt("local_web_port", "Local Port", &g::webRadarPort, kControlWidth, 100.0f);
        g::webRadarPort = std::clamp(g::webRadarPort, 1025, 65535);

        const int prevIntervalMs = g::webRadarIntervalMs;
        ui::widgets::SliderIntRow(
            "local_web_send_rate",
            "Send Interval (ms)",
            &g::webRadarIntervalMs,
            webradar::cfg::kMinRealtimeIntervalMs,
            webradar::cfg::kMaxRealtimeIntervalMs,
            "%d ms");
        g::webRadarIntervalMs = std::clamp(
            g::webRadarIntervalMs,
            webradar::cfg::kMinRealtimeIntervalMs,
            webradar::cfg::kMaxRealtimeIntervalMs);
        if (prevIntervalMs != g::webRadarIntervalMs)
            webradar::ApplySettingsFromGlobals();

        const bool prevBindLan = g::webRadarBindLan;
        ui::widgets::ToggleRow("local_web_bind_lan", "Allow LAN access (bind 0.0.0.0)", &g::webRadarBindLan);
        if (prevBindLan != g::webRadarBindLan)
            webradar::ApplySettingsFromGlobals();

        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::TextDisabled("Origin Allowlist");
        ImGui::TextWrapped(
            "Empty list with LAN off allows only same-origin browsers. "
            "Add explicit Origins (e.g. http://192.168.1.10) to permit cross-origin clients.");

        static char s_originBuffer[160] = {};
        ImGui::SetNextItemWidth(kControlWidth);
        ImGui::InputText("##origin_entry", s_originBuffer, sizeof(s_originBuffer));
        ImGui::SameLine();
        if (ui::widgets::CompactButton("Add##origin_add", 70.0f, 24.0f)) {
            std::string entry = s_originBuffer;
            while (!entry.empty() && (entry.front() == ' ' || entry.front() == '\t'))
                entry.erase(entry.begin());
            while (!entry.empty() && (entry.back() == ' ' || entry.back() == '\t'))
                entry.pop_back();
            if (!entry.empty()) {
                bool exists = false;
                for (const auto& origin : g::webRadarOriginAllowlist) {
                    if (origin == entry) { exists = true; break; }
                }
                if (!exists)
                    g::webRadarOriginAllowlist.push_back(std::move(entry));
                s_originBuffer[0] = '\0';
            }
        }
        for (size_t i = 0; i < g::webRadarOriginAllowlist.size(); ) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::TextUnformatted(g::webRadarOriginAllowlist[i].c_str());
            ImGui::SameLine();
            if (ui::widgets::CompactButton("Remove", 80.0f, 22.0f)) {
                g::webRadarOriginAllowlist.erase(g::webRadarOriginAllowlist.begin() + static_cast<std::ptrdiff_t>(i));
                ImGui::PopID();
                continue;
            }
            ImGui::PopID();
            ++i;
        }
    }

    int mapOverrideIndex = 0;
    for (int i = 1; i < IM_ARRAYSIZE(kMapOverrideItems); ++i) {
        if (_stricmp(state.webMapOverride, kMapOverrideItems[i]) == 0) {
            mapOverrideIndex = i;
            break;
        }
    }

    ImGui::TextDisabled("Map Override");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::BeginCombo("##map_override", kMapOverrideItems[mapOverrideIndex])) {
        for (int i = 0; i < IM_ARRAYSIZE(kMapOverrideItems); ++i) {
            const bool selected = (mapOverrideIndex == i);
            if (ImGui::Selectable(kMapOverrideItems[i], selected)) {
                mapOverrideIndex = i;
                const char* selectedValue = (i == 0) ? "" : kMapOverrideItems[i];
                strncpy_s(state.webMapOverride, sizeof(state.webMapOverride), selectedValue, _TRUNCATE);
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    g::webRadarMapOverride = state.webMapOverride;

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::BeginDisabled(localLink.empty());
    if (ui::widgets::CompactButton("Open LocalRadar", 170.0f, 34.0f))
        OpenRadarLink(localLink, statusSink, "Local WEBRadar opened.", "Cannot open Local WEBRadar.");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(webLink.empty());
    if (ui::widgets::CompactButton("Open WebRadar", 150.0f, 34.0f))
        OpenRadarLink(webLink, statusSink, "Remote WEBRadar opened.", "Cannot open remote WEBRadar.");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(webLink.empty());
    if (ui::widgets::CompactButton("Copy WebRadar link", 180.0f, 34.0f)) {
        ImGui::SetClipboardText(webLink.c_str());
        statusSink.SetStatus("WebRadar link copied.");
    }
    ImGui::EndDisabled();
}

void ui::tabs::webradar_sections::RenderRemoteSection(MenuState& state, IStatusSink& statusSink)
{
    SyncRemoteBuffers(state);
    FlushRemoteTaskStatus(statusSink);

    if (!g::webRadarRemoteSettingsOpen)
        return;

    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    bool changed = false;
    changed |= InputTextRow("Host / IP", "web_host", state.webRemoteHost, sizeof(state.webRemoteHost));
    changed |= ui::widgets::CompactInputIntRowAt("web_port", "Web Port", &g::webRadarRemoteWebPort, kControlWidth, 120.0f);
    changed |= ui::widgets::CompactInputIntRowAt("ssh_port", "SSH Port", &g::webRadarRemoteSshPort, kControlWidth, 120.0f);
    changed |= InputTextRow("Login", "web_login", state.webRemoteLogin, sizeof(state.webRemoteLogin));
    changed |= InputTextRow("Password", "web_password", state.webRemotePassword, sizeof(state.webRemotePassword), ImGuiInputTextFlags_Password);
    changed |= InputTextRow("Remote Path", "web_remote_path", state.webRemotePath, sizeof(state.webRemotePath));

    g::webRadarRemoteWebPort = std::clamp(g::webRadarRemoteWebPort, 1, 65535);
    g::webRadarRemoteSshPort = std::clamp(g::webRadarRemoteSshPort, 1, 65535);

    if (changed) {
        ApplyRemoteBuffersToGlobals(state);
        MarkRemoteSettingsDirty();
    }
    FlushRemoteSettingsIfIdle();

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    if (ui::widgets::CompactButton("Save", 100.0f, 32.0f)) {
        ApplyRemoteBuffersToGlobals(state);
        SaveRemoteSettings();
        statusSink.SetStatus("Web settings saved.");
    }
    ImGui::SameLine();
    if (ui::widgets::CompactButton("Test Ping", 140.0f, 32.0f)) {
        int pingMs = -1;
        std::string error;
        if (webradar::remote::TestPing(webradar::remote::CaptureSettingsFromGlobals(), &pingMs, &error))
            statusSink.SetStatus("SSH port ping: " + std::to_string(pingMs) + " ms.");
        else
            statusSink.SetStatus(error.empty() ? "SSH port ping failed." : error);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(s_remoteTaskRunning.load(std::memory_order_relaxed));
    if (ui::widgets::CompactButton("Test Connection", 170.0f, 32.0f)) {
        const webradar::remote::Settings settings = webradar::remote::CaptureSettingsFromGlobals();
        StartRemoteTask(statusSink, "Testing SSH connection...", [settings] {
            std::string error;
            if (webradar::remote::TestSshConnection(settings, &error))
                return std::string("SSH connection is OK.");
            return error.empty() ? std::string("SSH connection failed.") : error;
        });
    }
    ImGui::EndDisabled();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    ImGui::BeginDisabled(s_remoteTaskRunning.load(std::memory_order_relaxed));
    if (ui::widgets::CompactButton("Check Server Ready", 190.0f, 32.0f)) {
        const webradar::remote::Settings settings = webradar::remote::CaptureSettingsFromGlobals();
        StartRemoteTask(statusSink, "Checking WebRadar server...", [settings] {
            std::string error;
            if (webradar::remote::CheckServerReady(settings, &error))
                return std::string("WebRadar server is ready.");
            return error.empty() ? std::string("WebRadar server is not ready.") : error;
        });
    }
    ImGui::SameLine();
    if (ui::widgets::CompactButton("Deploy Web Files", 180.0f, 32.0f)) {
        const webradar::remote::Settings settings = webradar::remote::CaptureSettingsFromGlobals();
        StartRemoteTask(statusSink, "Deploying WebRadar to VDS...", [settings] {
            std::filesystem::path outPath;
            std::string error;
            if (webradar::remote::DeployToServer(settings, &outPath, &error, [](std::string_view progress) {
                QueueRemoteTaskStatus(std::string(progress));
            }))
                return std::string("WebRadar deployed and reachable.");
            return error.empty() ? std::string("WebRadar deploy failed.") : error;
        });
    }
    ImGui::EndDisabled();
    if (s_remoteTaskRunning.load(std::memory_order_relaxed))
        ImGui::TextDisabled("Remote task is running...");
}

void ui::tabs::webradar_sections::RenderQrSection(const std::string& localLink, const std::string& webLink)
{
    ui::widgets::ToggleRow("qr_code", "QR Code", &g::webRadarQrOpen);
    if (!g::webRadarQrOpen)
        return;

    constexpr float kQrSize = 184.0f;
    constexpr float kQrPadding = 12.0f;
    const ImGuiStyle& style = ImGui::GetStyle();
    const ImVec4 childBg = style.Colors[ImGuiCol_ChildBg];
    const ImVec4 border = style.Colors[ImGuiCol_Border];
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(childBg.x, childBg.y, childBg.z, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(border.x, border.y, border.z, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kQrPadding, kQrPadding));
    ImGui::BeginChild("##webradar_qr_panel", ImVec2(0.0f, kQrSize + kQrPadding * 2.0f + 28.0f), ImGuiChildFlags_Borders);
    if (!localLink.empty()) {
        ImGui::BeginGroup();
        ImGui::TextDisabled("Local WebRadar");
        ui::menu_utils::RenderQrCode(localLink, kQrSize);
        ImGui::EndGroup();
    } else {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12.0f);
        ImGui::TextDisabled("Local link not detected.");
    }

    if (!webLink.empty()) {
        ImGui::SameLine(0.0f, 28.0f);
        ImGui::BeginGroup();
        ImGui::TextDisabled("WebRadar");
        ui::menu_utils::RenderQrCode(webLink, kQrSize);
        ImGui::EndGroup();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

void ui::tabs::webradar_sections::RenderDebugSection(const webradar::RuntimeStats& runtimeStats, uint16_t effectivePort)
{
    ui::widgets::ToggleRow("debug", "Debug", &g::webRadarDebugOpen);
    if (!g::webRadarDebugOpen)
        return;

    ImGui::Text("Status: %s", runtimeStats.statusText.empty() ? "Idle" : runtimeStats.statusText.c_str());
    ImGui::Text("HTTP Server: %s", runtimeStats.serverListening ? "listening" : "offline");
    ImGui::Text("Listen Port: %u", static_cast<unsigned>(effectivePort));
    ImGui::Text("Active Map: %s", runtimeStats.activeMap.empty() ? "-" : runtimeStats.activeMap.c_str());
    ImGui::Text("Snapshots Published: %llu", static_cast<unsigned long long>(runtimeStats.sentPackets));
    ImGui::Text("HTTP Requests Served: %llu", static_cast<unsigned long long>(runtimeStats.servedRequests));
    ImGui::Text("Server Errors: %llu", static_cast<unsigned long long>(runtimeStats.failedPackets));
    ImGui::Text("Entities in Last Snapshot: %zu", runtimeStats.lastPayloadRows);
    ImGui::Separator();
    ImGui::Text("Payload: %zu B | legacy %zu B | avg %.0f B | max %zu B",
        runtimeStats.lastPayloadBytes,
        runtimeStats.lastLegacyPayloadBytes,
        runtimeStats.avgPayloadBytes,
        runtimeStats.maxPayloadBytes);
    ImGui::Text("Send: %.1f / %.1f Hz | Out %.1f KB/s",
        runtimeStats.sendHz,
        runtimeStats.targetHz,
        runtimeStats.bytesOutPerSec / 1024.0);
    ImGui::Text("Serialize: avg %.0f us | peak %llu us",
        runtimeStats.serializeUsAvg,
        static_cast<unsigned long long>(runtimeStats.serializeUsPeak));
    ImGui::Text("Coalesced: %llu | Poll: %llu",
        static_cast<unsigned long long>(runtimeStats.coalescedFrames),
        static_cast<unsigned long long>(runtimeStats.livePollRequests));
    ImGui::Text("Last Update: %s", ui::menu_utils::FormatUnixMs(runtimeStats.lastUpdateUnixMs).c_str());
    ImGui::Text("Last Request: %s", ui::menu_utils::FormatUnixMs(runtimeStats.lastRequestUnixMs).c_str());
}

void ui::tabs::webradar_sections::RenderRemoteDebugSection(const webradar::remote::Stats& runtimeStats)
{
    if (!g::webRadarDebugOpen)
        return;

    ImGui::Separator();
    ImGui::Text("Web Remote: %s", runtimeStats.connected ? "connected" : (runtimeStats.enabled ? "offline" : "disabled"));
    ImGui::Text("Remote Packets: %llu | Drops: %llu",
        static_cast<unsigned long long>(runtimeStats.sentPackets),
        static_cast<unsigned long long>(runtimeStats.queueDrops));
    ImGui::Text("Remote Out: %.1f KB | Last Ping: %d ms",
        static_cast<double>(runtimeStats.sentBytes) / 1024.0,
        runtimeStats.lastPingMs);
    if (!runtimeStats.lastError.empty())
        ImGui::TextDisabled("Remote Error: %s", runtimeStats.lastError.c_str());
}
