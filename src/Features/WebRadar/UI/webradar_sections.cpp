#include "Features/WebRadar/UI/webradar_sections.h"

#include "app/Core/globals.h"
#include "app/UI/MenuShell/menu_utils.h"

#include <algorithm>
#include <cstring>

#include <imgui.h>

namespace
{
    constexpr const char* kMapOverrideItems[] = {
        "Auto (Detect)",
        "de_mirage",
        "de_inferno",
        "de_dust2",
        "de_nuke",
        "de_overpass",
        "de_ancient",
        "de_anubis",
        "de_vertigo",
        "aim_custom"
    };

    void OpenLocalRadar(const std::string& localLink, ui::IStatusSink& statusSink)
    {
        if (ui::menu_utils::OpenExternal(localLink))
            statusSink.SetStatus("WEBRadar opened.");
        else
            statusSink.SetStatus("Cannot open WEBRadar.");
    }
}

void ui::tabs::webradar_sections::RenderConnectionSection(MenuState& state, const std::string& radarLink, IStatusSink& statusSink)
{
    ImGui::Checkbox("Enable WEBRadar", &g::webRadarEnabled);

    ImGui::InputInt("Port", &g::webRadarPort);
    g::webRadarPort = std::clamp(g::webRadarPort, 1025, 65535);

    int mapOverrideIndex = 0;
    for (int i = 1; i < IM_ARRAYSIZE(kMapOverrideItems); ++i) {
        if (_stricmp(state.webMapOverride, kMapOverrideItems[i]) == 0) {
            mapOverrideIndex = i;
            break;
        }
    }

    if (ImGui::BeginCombo("Map Override", kMapOverrideItems[mapOverrideIndex])) {
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

    ImGui::BeginDisabled(radarLink.empty());
    if (ImGui::Button("Open Radar", ImVec2(220, 32))) {
        OpenLocalRadar(radarLink, statusSink);
    }
    ImGui::EndDisabled();
}

void ui::tabs::webradar_sections::RenderQrSection(const std::string& radarLink)
{
    ImGui::SetNextItemOpen(g::webRadarQrOpen, ImGuiCond_Always);
    const bool qrOpen = ImGui::CollapsingHeader("QR Code", ImGuiTreeNodeFlags_DefaultOpen);
    g::webRadarQrOpen = qrOpen;
    if (!qrOpen)
        return;

    constexpr float kQrSize = 184.0f;
    constexpr float kQrPadding = 12.0f;
    const ImGuiStyle& style = ImGui::GetStyle();
    const ImVec4 childBg = style.Colors[ImGuiCol_ChildBg];
    const ImVec4 border = style.Colors[ImGuiCol_Border];
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(childBg.x, childBg.y, childBg.z, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(border.x, border.y, border.z, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kQrPadding, kQrPadding));
    ImGui::BeginChild("##webradar_qr_panel", ImVec2(0.0f, kQrSize + kQrPadding * 2.0f), ImGuiChildFlags_Borders);
    if (!radarLink.empty()) {
        ui::menu_utils::RenderQrCode(radarLink, kQrSize);
    } else {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12.0f);
        ImGui::TextDisabled("LAN address not detected.");
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

void ui::tabs::webradar_sections::RenderDebugSection(const webradar::RuntimeStats& runtimeStats, uint16_t effectivePort)
{
    ImGui::SetNextItemOpen(g::webRadarDebugOpen, ImGuiCond_Always);
    const bool debugOpen = ImGui::CollapsingHeader("Debug");
    g::webRadarDebugOpen = debugOpen;
    if (!debugOpen)
        return;

    ImGui::Text("Status: %s", runtimeStats.statusText.empty() ? "Idle" : runtimeStats.statusText.c_str());
    ImGui::Text("HTTP Server: %s", runtimeStats.serverListening ? "listening" : "offline");
    ImGui::Text("Listen Port: %u", static_cast<unsigned>(effectivePort));
    ImGui::Text("Active Map: %s", runtimeStats.activeMap.empty() ? "-" : runtimeStats.activeMap.c_str());
    ImGui::Text("Snapshots Published: %llu", static_cast<unsigned long long>(runtimeStats.sentPackets));
    ImGui::Text("HTTP Requests Served: %llu", static_cast<unsigned long long>(runtimeStats.servedRequests));
    ImGui::Text("Server Errors: %llu", static_cast<unsigned long long>(runtimeStats.failedPackets));
    ImGui::Text("Entities in Last Snapshot: %zu", runtimeStats.lastPayloadRows);
    ImGui::Text("Last Update: %s", ui::menu_utils::FormatUnixMs(runtimeStats.lastUpdateUnixMs).c_str());
    ImGui::Text("Last Request: %s", ui::menu_utils::FormatUnixMs(runtimeStats.lastRequestUnixMs).c_str());
}
