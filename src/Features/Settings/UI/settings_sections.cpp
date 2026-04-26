#include "Features/Settings/UI/settings_sections.h"

#include "app/Core/globals.h"
#include "app/UI/MenuShell/menu_utils.h"
#include "app/UI/MenuShell/ui_widgets.h"
#include "app/Config/config.h"
#include "Features/ESP/esp.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <imgui.h>

namespace
{
    void SaveProfile(const char* profileName, ui::IStatusSink& statusSink)
    {
        ::config::SaveNamed(profileName ? profileName : "default");
        statusSink.SetStatus("Profile saved.");
    }

    void LoadProfile(const char* profileName, ui::IStatusSink& statusSink)
    {
        if (::config::LoadNamed(profileName ? profileName : "default"))
            statusSink.SetStatus("Profile loaded.");
        else
            statusSink.SetStatus("Profile not found.");
    }

    void StartMenuKeyCapture(ui::MenuState& state, ui::IStatusSink& statusSink)
    {
        state.waitingForMenuKey = true;
        statusSink.SetStatus("Press any key to bind menu toggle.");
    }

    void CancelMenuKeyCapture(ui::MenuState& state, ui::IStatusSink& statusSink)
    {
        state.waitingForMenuKey = false;
        statusSink.SetStatus("Key capture canceled.");
    }

    void ResetMenuKeyToInsert(ui::MenuState& state, ui::IStatusSink& statusSink)
    {
        g::menuToggleKey = 'P';
        state.waitingForMenuKey = false;
        statusSink.SetStatus("Menu key reset to P.");
    }
}

void ui::tabs::settings_sections::RenderProfilesSection(MenuState& state, IStatusSink& statusSink)
{
    const float innerGap = 10.0f;
    const float contentWidth = ImGui::GetContentRegionAvail().x;
    const float fieldWidth = contentWidth;
    const float dualButtonWidth = (contentWidth - innerGap) * 0.5f;

    ui::widgets::SectionTitle("Profiles");
    ImGui::TextDisabled("Active Profile");
    ImGui::TextUnformatted(config::GetActiveProfile().c_str());
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    ImGui::TextDisabled("Profile Name");
    ImGui::SetNextItemWidth(fieldWidth);
    ImGui::InputText("##profile_name", state.profileName, sizeof(state.profileName));

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::TextDisabled("Saved Profiles");
    ImGui::SetNextItemWidth(fieldWidth);
    if (ImGui::BeginCombo("##saved_profiles", config::GetActiveProfile().c_str())) {
        const std::vector<std::string> profiles = config::ListProfiles();
        for (const std::string& profile : profiles) {
            const bool selected = (profile == config::GetActiveProfile());
            if (ImGui::Selectable(profile.c_str(), selected)) {
                ui::menu_utils::CopyToBuffer(state.profileName, sizeof(state.profileName), profile);
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    if (ImGui::Button("Save Profile", ImVec2(dualButtonWidth, 32.0f))) {
        SaveProfile(state.profileName, statusSink);
    }
    ImGui::SameLine(0.0f, innerGap);
    if (ImGui::Button("Load Profile", ImVec2(dualButtonWidth, 32.0f))) {
        LoadProfile(state.profileName, statusSink);
    }
}

void ui::tabs::settings_sections::RenderControlsSection(MenuState& state, IStatusSink& statusSink)
{
    const float contentWidth = ImGui::GetContentRegionAvail().x;

    ui::widgets::SectionTitle("Controls");
    ImGui::TextDisabled("Menu Toggle Key");
    ImGui::TextUnformatted(key_names::ToDisplayName(g::menuToggleKey).c_str());
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    if (!state.waitingForMenuKey) {
        if (ImGui::Button("Change Menu Key", ImVec2(contentWidth, 32.0f))) {
            StartMenuKeyCapture(state, statusSink);
        }
    }
    else {
        if (ImGui::Button("Cancel Key Capture", ImVec2(contentWidth, 32.0f))) {
            CancelMenuKeyCapture(state, statusSink);
        }
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextDisabled("Waiting for key press...");
    }

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    if (ImGui::Button("Reset to P", ImVec2(contentWidth, 32.0f))) {
        ResetMenuKeyToInsert(state, statusSink);
    }
}

void ui::tabs::settings_sections::RenderScreenSection(IStatusSink& statusSink)
{
    (void)statusSink;
    ui::widgets::SectionTitle("Screen");
    ImGui::TextDisabled("Current Overlay Size");
    ImGui::Text("%d x %d", g::screenWidth, g::screenHeight);
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::TextDisabled("Overlay size is detected automatically from the game window.");

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ui::widgets::ToggleRow("vsync", "VSync", &g::vsyncEnabled);
    if (!g::vsyncEnabled) {
        ui::widgets::SliderIntRow("fps_limit", "FPS Limit", &g::fpsLimit, 0, 500, g::fpsLimit == 0 ? "Unlimited" : "%d");
    }
}

void ui::tabs::settings_sections::RenderDebugWindow(bool* open)
{
    ImGui::SetNextWindowSize(ImVec2(430.0f, 500.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug Statistics", open)) {
        ImGui::End();
        return;
    }

    const auto health = esp::GetDmaHealthStats();
    const auto debug = esp::GetDebugStats();
    const auto& st = debug.stages;
    static double s_lastCopyTime = 0.0;

    auto msFromUs = [](uint64_t valueUs) -> float {
        return static_cast<float>(valueUs) / 1000.0f;
    };
    auto statusBadge = [](bool value, ImVec4 okColor, ImVec4 badColor) -> ImVec4 {
        return value ? okColor : badColor;
    };
    auto subsystemStateLabel = [](esp::SubsystemHealthState state) -> const char* {
        switch (state) {
        case esp::SubsystemHealthState::Healthy: return "healthy";
        case esp::SubsystemHealthState::Degraded: return "degraded";
        case esp::SubsystemHealthState::Failed: return "failed";
        default: return "unknown";
        }
    };
    auto subsystemStateColor = [](esp::SubsystemHealthState state) -> ImVec4 {
        switch (state) {
        case esp::SubsystemHealthState::Healthy: return ImVec4(0.30f, 0.86f, 0.30f, 1.0f);
        case esp::SubsystemHealthState::Degraded: return ImVec4(0.95f, 0.76f, 0.24f, 1.0f);
        case esp::SubsystemHealthState::Failed: return ImVec4(1.0f, 0.40f, 0.30f, 1.0f);
        default: return ImVec4(0.62f, 0.62f, 0.66f, 1.0f);
        }
    };
    auto stageColorForUs = [](uint64_t valueUs) -> ImVec4 {
        if (valueUs == 0)
            return ImVec4(0.32f, 0.32f, 0.34f, 0.0f);
        if (valueUs <= 500)
            return ImVec4(0.30f, 0.80f, 0.34f, 0.92f);
        if (valueUs <= 1500)
            return ImVec4(0.86f, 0.76f, 0.24f, 0.92f);
        if (valueUs <= 3333)
            return ImVec4(0.95f, 0.62f, 0.24f, 0.92f);
        return ImVec4(0.98f, 0.32f, 0.24f, 0.92f);
    };
    auto renderMetricBar = [&](const char* label, float displayUs, const char* detail) {
        constexpr uint64_t kMetricScaleUs = 3333;
        const float windowRight = ImGui::GetWindowContentRegionMax().x;
        const float barStartX = 168.0f;
        const float barMargin = 12.0f;
        const float barMaxWidth = std::max(windowRight - barStartX - barMargin, 20.0f);
        const float displayMs = displayUs / 1000.0f;
        const uint64_t clampedDisplayUs = displayUs > 0.0f ? static_cast<uint64_t>(displayUs) : 0u;

        ImGui::Text("%-11s", label);
        ImGui::SameLine(88.0f);
        ImGui::Text("%6.2f ms", displayMs);
        ImGui::SameLine(barStartX);

        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float barHeight = ImGui::GetTextLineHeight() - 2.0f;
        dl->AddRectFilled(
            cursor,
            ImVec2(cursor.x + barMaxWidth, cursor.y + barHeight),
            ImGui::GetColorU32(ImVec4(0.12f, 0.12f, 0.14f, 0.9f)),
            2.0f);
        if (clampedDisplayUs > 0) {
            const float fillWidth =
                (std::min<uint64_t>(clampedDisplayUs, kMetricScaleUs) * barMaxWidth / static_cast<float>(kMetricScaleUs));
            if (fillWidth > 0.0f) {
                dl->AddRectFilled(
                    cursor,
                    ImVec2(cursor.x + fillWidth, cursor.y + barHeight),
                    ImGui::ColorConvertFloat4ToU32(stageColorForUs(clampedDisplayUs)),
                    2.0f);
            }
            if (clampedDisplayUs > kMetricScaleUs) {
                dl->AddRectFilled(
                    ImVec2(cursor.x + barMaxWidth - 6.0f, cursor.y),
                    ImVec2(cursor.x + barMaxWidth, cursor.y + barHeight),
                    ImGui::GetColorU32(ImVec4(0.98f, 0.24f, 0.20f, 0.95f)),
                    2.0f);
            }
        }
        ImGui::Dummy(ImVec2(barMaxWidth, barHeight));
        if (detail && detail[0] != '\0') {
            if (std::strcmp(label, "Bomb") == 0) {
                ImGui::Text("Status:");
                ImGui::SameLine();
                if (std::strstr(detail, "Hidden") != nullptr) {
                    ImGui::TextDisabled("%s", detail);
                } else {
                    ImGui::TextColored(ImVec4(0.3f, 0.86f, 0.3f, 1.0f), "%s", detail);
                }
            } else {
                ImGui::TextDisabled("%s", detail);
            }
        }
    };
    const uint64_t sessionUptimeUs = debug.sessionUptimeUs > 0 ? debug.sessionUptimeUs : debug.uptimeUs;
    const char* statusLabel = "Unknown";
    ImVec4 statusColor(0.7f, 0.7f, 0.7f, 1.0f);

    
    {
        if (health.gameStatus == esp::GameStatus::Ok) {
            statusLabel = "OK";
            statusColor = ImVec4(0.3f, 0.86f, 0.3f, 1.0f);
        } else if (health.gameStatus == esp::GameStatus::WaitCs2) {
            statusLabel = "Waiting for CS2";
            statusColor = ImVec4(0.86f, 0.7f, 0.24f, 1.0f);
        }
        ImGui::Text("Status:");
        ImGui::SameLine();
        ImGui::TextColored(statusColor, "%s", statusLabel);

        ImGui::SameLine(110.0f);
        ImGui::Text("Data:");
        ImGui::SameLine();
        ImGui::TextColored(
            statusBadge(health.workerRunning, ImVec4(0.3f, 0.86f, 0.3f, 1.0f), ImVec4(1.0f, 0.4f, 0.3f, 1.0f)),
            "%s", health.workerRunning ? "Running" : "Stopped");

        ImGui::SameLine(205.0f);
        ImGui::Text("Camera:");
        ImGui::SameLine();
        ImGui::TextColored(
            statusBadge(health.cameraWorkerRunning, ImVec4(0.3f, 0.86f, 0.3f, 1.0f), ImVec4(1.0f, 0.4f, 0.3f, 1.0f)),
            "%s", health.cameraWorkerRunning ? "Running" : "Stopped");
    }

    ImGui::Separator();

    
    const float rawSnapshotAgeMs =
        (debug.lastPublishUs > 0 && debug.uptimeUs > debug.lastPublishUs)
        ? msFromUs(debug.uptimeUs - debug.lastPublishUs)
        : 0.0f;
    const float rawCameraViewAgeMs = msFromUs(debug.cameraViewAgeUs);
    const float rawCameraLocalAgeMs = msFromUs(debug.cameraLocalPosAgeUs);
    const float rawWorldAgeMs = msFromUs(debug.worldScanAgeUs);
    const float worldTargetMs = msFromUs(debug.worldScanTargetIntervalUs);
    const float worldTickMs = msFromUs(st.worldScanUs);
    const float worldLastMs = msFromUs(st.worldScanLastUs);
    const float workerLoopAgeMs = static_cast<float>(health.dataWorkerLoopAgeMs);
    const float workerInFlightAgeMs = static_cast<float>(health.dataWorkerInFlightAgeMs);
    const float uiDeltaSeconds = std::clamp(ImGui::GetIO().DeltaTime, 1.0f / 240.0f, 0.25f);
    auto smoothToward = [&](float current, float target, float riseTauSeconds, float fallTauSeconds) -> float {
        const float tauSeconds = target >= current ? riseTauSeconds : fallTauSeconds;
        if (tauSeconds <= 0.0f)
            return target;
        const float alpha = 1.0f - std::exp(-uiDeltaSeconds / tauSeconds);
        return current + (target - current) * alpha;
    };
    constexpr float kHealthRiseTauSeconds = 2.50f;
    constexpr float kHealthFallTauSeconds = 4.00f;
    static float s_smoothedSnapshotAgeMs = 0.0f;
    static float s_smoothedCameraViewAgeMs = 0.0f;
    static float s_smoothedCameraLocalAgeMs = 0.0f;
    static float s_smoothedWorldAgeMs = 0.0f;
    static bool s_smoothedHealthInit = false;
    if (!s_smoothedHealthInit) {
        s_smoothedSnapshotAgeMs = rawSnapshotAgeMs;
        s_smoothedCameraViewAgeMs = rawCameraViewAgeMs;
        s_smoothedCameraLocalAgeMs = rawCameraLocalAgeMs;
        s_smoothedWorldAgeMs = rawWorldAgeMs;
        s_smoothedHealthInit = true;
    } else {
        s_smoothedSnapshotAgeMs = smoothToward(
            s_smoothedSnapshotAgeMs,
            rawSnapshotAgeMs,
            kHealthRiseTauSeconds,
            kHealthFallTauSeconds);
        s_smoothedCameraViewAgeMs = smoothToward(
            s_smoothedCameraViewAgeMs,
            rawCameraViewAgeMs,
            kHealthRiseTauSeconds,
            kHealthFallTauSeconds);
        s_smoothedCameraLocalAgeMs = smoothToward(
            s_smoothedCameraLocalAgeMs,
            rawCameraLocalAgeMs,
            kHealthRiseTauSeconds,
            kHealthFallTauSeconds);
    s_smoothedWorldAgeMs = smoothToward(
            s_smoothedWorldAgeMs,
            rawWorldAgeMs,
            0.55f,
            0.85f);
    }
    const float snapshotAgeMs = s_smoothedSnapshotAgeMs;
    const float cameraViewAgeMs = s_smoothedCameraViewAgeMs;
    const float worldAgeMs = s_smoothedWorldAgeMs;

    static float s_smoothedCycleTopUs = 0.0f;
    constexpr float kPrimaryRiseTauSeconds = 0.55f;
    constexpr float kPrimaryFallTauSeconds = 0.50f;
    s_smoothedCycleTopUs = smoothToward(
        s_smoothedCycleTopUs,
        static_cast<float>(debug.cycleUs),
        kPrimaryRiseTauSeconds,
        kPrimaryFallTauSeconds);
    const float cycleMsSmoothed = s_smoothedCycleTopUs / 1000.0f;
    constexpr float budgetMs = 1000.0f / 300.0f;

    ImGui::Text("Pipeline: %.2f / %.2f ms", cycleMsSmoothed, budgetMs);
    ImGui::SameLine();
    if (cycleMsSmoothed > budgetMs)
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "(over)");
    else
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "(ok)");

    ImGui::TextDisabled("Freshness: Snap %.1f | Cam %.1f | World %.1f ms", snapshotAgeMs, cameraViewAgeMs, worldAgeMs);
    ImGui::Text("Subsystems:");
    ImGui::SameLine();
    ImGui::TextDisabled("P");
    ImGui::SameLine();
    ImGui::TextColored(subsystemStateColor(debug.playersCore.state), "%s", subsystemStateLabel(debug.playersCore.state));
    ImGui::SameLine();
    ImGui::TextDisabled("C");
    ImGui::SameLine();
    ImGui::TextColored(subsystemStateColor(debug.cameraView.state), "%s", subsystemStateLabel(debug.cameraView.state));
    ImGui::SameLine();
    ImGui::TextDisabled("G");
    ImGui::SameLine();
    ImGui::TextColored(subsystemStateColor(debug.gamerulesMap.state), "%s", subsystemStateLabel(debug.gamerulesMap.state));
    ImGui::SameLine();
    ImGui::TextDisabled("B");
    ImGui::SameLine();
    ImGui::TextColored(subsystemStateColor(debug.bones.state), "%s", subsystemStateLabel(debug.bones.state));
    ImGui::SameLine();
    ImGui::TextDisabled("W");
    ImGui::SameLine();
    ImGui::TextColored(subsystemStateColor(debug.world.state), "%s", subsystemStateLabel(debug.world.state));

    ImGui::Separator();

    constexpr float kSecondaryRiseTauSeconds = 0.70f;
    constexpr float kSecondaryFallTauSeconds = 1.10f;
    static float s_smoothedHeldAuxUs = 0.0f;
    static float s_smoothedHeldInvUs = 0.0f;
    static float s_smoothedHeldBonesUs = 0.0f;
    static float s_smoothedAuxUs = 0.0f;
    static float s_smoothedInvUs = 0.0f;
    static float s_smoothedBonesUs = 0.0f;
    static float s_smoothedCameraCycleUs = 0.0f;
    static float s_smoothedOverlayFrameUs = 0.0f;
    static float s_smoothedOverlayPacingUs = 0.0f;
    static float s_smoothedCoreUs = 0.0f;
    static float s_smoothedDeferredCurrentUs = 0.0f;
    static float s_smoothedDeferredHeldUs = 0.0f;
    static float s_smoothedBombUs = 0.0f;
    static float s_smoothedWorldCurrentUs = 0.0f;
    static float s_smoothedWorldHeldUs = 0.0f;
    static float s_smoothedBaseUs = 0.0f;
    static float s_smoothedPlayerReadsUs = 0.0f;
    static float s_smoothedEngineUs = 0.0f;
    static float s_smoothedCommitUs = 0.0f;
    static float s_smoothedWorldTickUs = 0.0f;
    static float s_smoothedWorldLastUs = 0.0f;
    s_smoothedHeldAuxUs = smoothToward(
        s_smoothedHeldAuxUs,
        static_cast<float>(st.playerAuxLastUs),
        kSecondaryRiseTauSeconds,
        kSecondaryFallTauSeconds);
    s_smoothedHeldInvUs = smoothToward(
        s_smoothedHeldInvUs,
        static_cast<float>(st.inventoryLastUs),
        kSecondaryRiseTauSeconds,
        kSecondaryFallTauSeconds);
    s_smoothedHeldBonesUs = smoothToward(
        s_smoothedHeldBonesUs,
        static_cast<float>(st.boneReadsLastUs),
        kSecondaryRiseTauSeconds,
        kSecondaryFallTauSeconds);
    s_smoothedCameraCycleUs = smoothToward(
        s_smoothedCameraCycleUs,
        static_cast<float>(debug.camera.cycleUs),
        kSecondaryRiseTauSeconds,
        kSecondaryFallTauSeconds);
    s_smoothedOverlayFrameUs = smoothToward(
        s_smoothedOverlayFrameUs,
        static_cast<float>(debug.overlay.frameUs),
        kSecondaryRiseTauSeconds,
        kSecondaryFallTauSeconds);
    s_smoothedOverlayPacingUs = smoothToward(
        s_smoothedOverlayPacingUs,
        static_cast<float>(debug.overlay.pacingWaitUs),
        kSecondaryRiseTauSeconds,
        kSecondaryFallTauSeconds);
    uint64_t heldWorldUs = st.worldScanLastUs;
    if (debug.worldScanAgeUs > 0) {
        const uint64_t staleWorldThresholdUs =
            std::max<uint64_t>(debug.worldScanTargetIntervalUs * 2u, 120000u);
        if (debug.worldScanAgeUs > staleWorldThresholdUs)
            heldWorldUs = 0;
    }
    const uint64_t coreUs =
        st.engineUs + st.baseReadsUs + st.playerReadsUs + st.commitStateUs + st.commitEnrichUs;
    const uint64_t commitUs = st.commitStateUs + st.commitEnrichUs;
    const uint64_t deferredCurrentUs =
        st.playerAuxUs +
        st.inventoryUs +
        st.boneReadsUs;
    const uint64_t deferredHeldUs =
        std::max(st.playerAuxUs, st.playerAuxLastUs) +
        std::max(st.inventoryUs, st.inventoryLastUs) +
        std::max(st.boneReadsUs, st.boneReadsLastUs);
    const uint64_t worldCurrentUs = st.worldScanUs;
    const uint64_t worldHeldUs = heldWorldUs;
    s_smoothedCoreUs = smoothToward(
        s_smoothedCoreUs,
        static_cast<float>(coreUs),
        kPrimaryRiseTauSeconds,
        kPrimaryFallTauSeconds);
    s_smoothedBaseUs = smoothToward(
        s_smoothedBaseUs,
        static_cast<float>(st.baseReadsUs),
        kSecondaryRiseTauSeconds,
        kSecondaryFallTauSeconds);
    s_smoothedPlayerReadsUs = smoothToward(
        s_smoothedPlayerReadsUs,
        static_cast<float>(st.playerReadsUs),
        kSecondaryRiseTauSeconds,
        kSecondaryFallTauSeconds);
    s_smoothedEngineUs = smoothToward(
        s_smoothedEngineUs,
        static_cast<float>(st.engineUs),
        kSecondaryRiseTauSeconds,
        kSecondaryFallTauSeconds);
    s_smoothedCommitUs = smoothToward(
        s_smoothedCommitUs,
        static_cast<float>(commitUs),
        kSecondaryRiseTauSeconds,
        kSecondaryFallTauSeconds);
    s_smoothedDeferredCurrentUs = smoothToward(
        s_smoothedDeferredCurrentUs,
        static_cast<float>(deferredCurrentUs),
        kPrimaryRiseTauSeconds,
        kPrimaryFallTauSeconds);
    s_smoothedDeferredHeldUs = smoothToward(
        s_smoothedDeferredHeldUs,
        static_cast<float>(deferredHeldUs),
        kPrimaryRiseTauSeconds,
        kPrimaryFallTauSeconds);
    s_smoothedBombUs = smoothToward(
        s_smoothedBombUs,
        static_cast<float>(st.bombScanUs),
        kPrimaryRiseTauSeconds,
        kPrimaryFallTauSeconds);
    s_smoothedWorldCurrentUs = smoothToward(
        s_smoothedWorldCurrentUs,
        static_cast<float>(worldCurrentUs),
        0.65f,
        1.00f);
    s_smoothedWorldHeldUs = smoothToward(
        s_smoothedWorldHeldUs,
        static_cast<float>(worldHeldUs),
        0.65f,
        1.00f);
    s_smoothedAuxUs = smoothToward(
        s_smoothedAuxUs,
        static_cast<float>(st.playerAuxUs),
        kSecondaryRiseTauSeconds,
        kSecondaryFallTauSeconds);
    s_smoothedInvUs = smoothToward(
        s_smoothedInvUs,
        static_cast<float>(st.inventoryUs),
        kSecondaryRiseTauSeconds,
        kSecondaryFallTauSeconds);
    s_smoothedBonesUs = smoothToward(
        s_smoothedBonesUs,
        static_cast<float>(st.boneReadsUs),
        kSecondaryRiseTauSeconds,
        kSecondaryFallTauSeconds);
    s_smoothedWorldTickUs = smoothToward(
        s_smoothedWorldTickUs,
        static_cast<float>(st.worldScanUs),
        0.45f,
        0.70f);
    s_smoothedWorldLastUs = smoothToward(
        s_smoothedWorldLastUs,
        static_cast<float>(st.worldScanLastUs),
        0.50f,
        0.80f);
    const char* bombStateLabel = "Hidden";
    if (debug.bombPlanted)
        bombStateLabel = debug.bombTicking ? "Ticking" : "Planted";
    else if (debug.bombDropped)
        bombStateLabel = "Dropped";

    auto warmupStateLabel = [](esp::SceneWarmupState state) -> const char* {
        switch (state) {
        case esp::SceneWarmupState::ColdAttach: return "cold_attach";
        case esp::SceneWarmupState::SceneTransition: return "scene_transition";
        case esp::SceneWarmupState::HierarchyWarming: return "hierarchy_warming";
        case esp::SceneWarmupState::Stable: return "stable";
        case esp::SceneWarmupState::Recovery: return "recovery";
        default: return "unknown";
        }
    };

    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 2.0f));

    char detail[192] = {};
    std::snprintf(
        detail, sizeof(detail),
        "Engine %.2f | Base %.2f | Player %.2f | Commit %.2f",
        s_smoothedEngineUs / 1000.0f,
        s_smoothedBaseUs / 1000.0f,
        s_smoothedPlayerReadsUs / 1000.0f,
        s_smoothedCommitUs / 1000.0f);
    renderMetricBar("Core Data", s_smoothedCoreUs, detail);

    std::snprintf(
        detail, sizeof(detail),
        "Aux %.2f | Inv %.2f | Bones %.2f",
        s_smoothedAuxUs / 1000.0f,
        s_smoothedInvUs / 1000.0f,
        s_smoothedBonesUs / 1000.0f);
    renderMetricBar("Deferred", s_smoothedDeferredCurrentUs, detail);

    std::snprintf(
        detail, sizeof(detail),
        "[ %s%s | src 0x%X | q %u ]",
        bombStateLabel,
        debug.bombBeingDefused ? " | Defusing" : "",
        debug.bombSourceFlags,
        static_cast<unsigned>(debug.bombConfidence));
    renderMetricBar("Bomb", s_smoothedBombUs, detail);

    std::snprintf(
        detail, sizeof(detail),
        "Tick %.2f | Age %.0f ms | Markers %d",
        s_smoothedWorldCurrentUs / 1000.0f,
        worldAgeMs,
        debug.worldMarkerCount);
    renderMetricBar("World", s_smoothedWorldCurrentUs, detail);

    std::snprintf(
        detail, sizeof(detail),
        "Cycle %.2f",
        s_smoothedCameraCycleUs / 1000.0f);
    renderMetricBar("Camera", s_smoothedCameraCycleUs, detail);

    ImGui::Separator();

    ImGui::Text("Players: %d | Budget: %d | Entities: %d",
        debug.activePlayers, debug.playerSlotBudget, debug.highestEntityIdx);

    if (health.recovering || health.recoveryRequested || health.dataWorkerStalled) {
        ImGui::Separator();
        if (health.dataWorkerStalled)
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.25f, 1.0f), "Worker stalled");
        else if (health.recovering)
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "DMA recovering");
        else if (health.recoveryRequested)
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "DMA recovery requested");
    }
    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    if (ImGui::Button("Copy Diagnostics", ImVec2(ImGui::GetContentRegionAvail().x, 30.0f))) {
        char line[512] = {};
        std::string diagnostics;
        diagnostics.reserve(2048);

        std::snprintf(line, sizeof(line),
            "status=%s data_worker=%s camera_worker=%s game_status=%d\n",
            statusLabel,
            health.workerRunning ? "running" : "stopped",
            health.cameraWorkerRunning ? "running" : "stopped",
            static_cast<int>(health.gameStatus));
        diagnostics += line;

        std::snprintf(line, sizeof(line),
            "worker loop_age_ms=%.1f in_flight=%d in_flight_age_ms=%.1f stalled=%d\n",
            workerLoopAgeMs,
            health.dataWorkerInFlight ? 1 : 0,
            workerInFlightAgeMs,
            health.dataWorkerStalled ? 1 : 0);
        diagnostics += line;

        std::snprintf(line, sizeof(line),
            "publishes=%llu snapshot_age_ms=%.1f camera_view=%s(%.1f) camera_local=%s(%.1f)\n",
            static_cast<unsigned long long>(debug.publishCount),
            rawSnapshotAgeMs,
            debug.liveViewValid ? (debug.liveViewFresh ? "fresh" : "stale") : "invalid",
            rawCameraViewAgeMs,
            debug.liveLocalPosValid ? (debug.liveLocalPosFresh ? "fresh" : "stale") : "invalid",
            rawCameraLocalAgeMs);
        diagnostics += line;

        std::snprintf(line, sizeof(line),
            "engine menu=%d ingame=%d max_clients=%d slot_budget=%d\n",
            debug.engineMenu ? 1 : 0,
            debug.engineInGame ? 1 : 0,
            debug.engineMaxClients,
            debug.playerSlotBudget);
        diagnostics += line;

        std::snprintf(line, sizeof(line),
            "epochs scene=%llu map=%llu bomb=%llu warmup=%s warmup_age_ms=%.1f\n",
            static_cast<unsigned long long>(debug.sceneEpoch),
            static_cast<unsigned long long>(debug.mapEpoch),
            static_cast<unsigned long long>(debug.bombEpoch),
            warmupStateLabel(debug.warmupState),
            msFromUs(debug.warmupAgeUs));
        diagnostics += line;

        std::snprintf(line, sizeof(line),
            "subsystems players=%s@%.1f/%u camera=%s@%.1f/%u gamerules=%s@%.1f/%u bones=%s@%.1f/%u world=%s@%.1f/%u\n",
            subsystemStateLabel(debug.playersCore.state),
            msFromUs(debug.playersCore.lastGoodAgeUs),
            debug.playersCore.failureStreak,
            subsystemStateLabel(debug.cameraView.state),
            msFromUs(debug.cameraView.lastGoodAgeUs),
            debug.cameraView.failureStreak,
            subsystemStateLabel(debug.gamerulesMap.state),
            msFromUs(debug.gamerulesMap.lastGoodAgeUs),
            debug.gamerulesMap.failureStreak,
            subsystemStateLabel(debug.bones.state),
            msFromUs(debug.bones.lastGoodAgeUs),
            debug.bones.failureStreak,
            subsystemStateLabel(debug.world.state),
            msFromUs(debug.world.lastGoodAgeUs),
            debug.world.failureStreak);
        diagnostics += line;

        std::snprintf(line, sizeof(line),
            "cycle_ms=%.2f peak_ms=%.2f total_current_ms=%.2f total_held_ms=%.2f budget_ms=%.2f\n",
            msFromUs(debug.cycleUs),
            msFromUs(debug.maxCycleUs),
            msFromUs(st.totalUs),
            msFromUs(st.totalHeldUs),
            budgetMs);
        diagnostics += line;

        std::snprintf(line, sizeof(line),
            "stages_ms engine=%.2f base=%.2f player_hierarchy=%.2f player_core=%.2f player_repair=%.2f player_total=%.2f commit=%.2f player_aux=%.2f inventory=%.2f bones=%.2f bomb=%.2f world_tick=%.2f world_last=%.2f enrich=%.2f\n",
            msFromUs(st.engineUs),
            msFromUs(st.baseReadsUs),
            msFromUs(st.playerHierarchyUs),
            msFromUs(st.playerCoreUs),
            msFromUs(st.playerRepairUs),
            msFromUs(st.playerReadsUs),
            msFromUs(st.commitStateUs),
            msFromUs(st.playerAuxUs),
            msFromUs(st.inventoryUs),
            msFromUs(st.boneReadsUs),
            msFromUs(st.bombScanUs),
            worldTickMs,
            worldLastMs,
            msFromUs(st.commitEnrichUs));
        diagnostics += line;

        std::snprintf(line, sizeof(line),
            "held_ms player_aux=%.2f@%.1f inventory=%.2f@%.1f bones=%.2f@%.1f\n",
            msFromUs(st.playerAuxLastUs), msFromUs(st.playerAuxAgeUs),
            msFromUs(st.inventoryLastUs), msFromUs(st.inventoryAgeUs),
            msFromUs(st.boneReadsLastUs), msFromUs(st.boneReadsAgeUs));
        diagnostics += line;

        std::snprintf(line, sizeof(line),
            "world_scan age_ms=%.1f target_ms=%.1f markers=%d active_players=%d entity_range=%d\n",
            rawWorldAgeMs,
            worldTargetMs,
            debug.worldMarkerCount,
            debug.activePlayers,
            debug.highestEntityIdx);
        diagnostics += line;

        std::snprintf(line, sizeof(line),
            "camera_ms cycle=%.2f peak=%.2f overlay_ms frame=%.2f peak=%.2f sync=%.2f draw=%.2f present=%.2f pacing_wait=%.2f\n",
            msFromUs(debug.camera.cycleUs),
            msFromUs(debug.camera.maxCycleUs),
            msFromUs(debug.overlay.frameUs),
            msFromUs(debug.overlay.maxFrameUs),
            msFromUs(debug.overlay.syncUs),
            msFromUs(debug.overlay.drawUs),
            msFromUs(debug.overlay.presentUs),
            msFromUs(debug.overlay.pacingWaitUs));
        diagnostics += line;

        std::snprintf(line, sizeof(line),
            "bomb state=%s planted=%d ticking=%d dropped=%d defusing=%d bounds=%d source=0x%X confidence=%u\n",
            bombStateLabel,
            debug.bombPlanted ? 1 : 0,
            debug.bombTicking ? 1 : 0,
            debug.bombDropped ? 1 : 0,
            debug.bombBeingDefused ? 1 : 0,
            debug.bombBoundsValid ? 1 : 0,
            debug.bombSourceFlags,
            static_cast<unsigned>(debug.bombConfidence));
        diagnostics += line;

        std::snprintf(line, sizeof(line),
            "dma ok=%llu fail=%llu consec=%u recoveries=%llu recovering=%d recovery_requested=%d recovery_age_ms=%llu last_success_age_ms=%llu\n",
            static_cast<unsigned long long>(health.totalSuccesses),
            static_cast<unsigned long long>(health.totalFailures),
            health.consecutiveFailures,
            static_cast<unsigned long long>(health.totalRecoveries),
            health.recovering ? 1 : 0,
            health.recoveryRequested ? 1 : 0,
            static_cast<unsigned long long>(health.recoveryRequestAgeMs),
            static_cast<unsigned long long>(health.lastSuccessAgeMs));
        diagnostics += line;

        if (health.eventCount > 0) {
            diagnostics += "dma_events";
            for (int i = 0; i < health.eventCount; ++i) {
                const auto& event = health.events[i];
                std::snprintf(line, sizeof(line),
                    " [%d]=%s:%s@%llums",
                    i,
                    event.action,
                    event.reason,
                    static_cast<unsigned long long>(event.ageMs));
                diagnostics += line;
            }
            diagnostics += "\n";
        }

        const uint64_t uptimeSec = sessionUptimeUs / 1000000u;
        std::snprintf(line, sizeof(line),
            "uptime=%02d:%02d:%02d",
            static_cast<int>(uptimeSec / 3600),
            static_cast<int>((uptimeSec % 3600) / 60),
            static_cast<int>(uptimeSec % 60));
        diagnostics += line;
        ImGui::SetClipboardText(diagnostics.c_str());
        s_lastCopyTime = ImGui::GetTime();
    }
    if (s_lastCopyTime > 0.0 && (ImGui::GetTime() - s_lastCopyTime) < 1.5)
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "Diagnostics copied to clipboard.");

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    if (ImGui::Button("Refresh Cache Data", ImVec2(ImGui::GetContentRegionAvail().x, 30.0f))) {
        esp::RequestCacheRefresh();
    }

    ImGui::End();
}
