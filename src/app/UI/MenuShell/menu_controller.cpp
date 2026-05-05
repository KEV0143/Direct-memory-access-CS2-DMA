#include "app/Core/build_info.h"
#include "app/UI/MenuShell/menu_controller.h"

#include "app/Config/config.h"
#include "app/Core/globals.h"
#include "app/UI/MenuShell/menu_utils.h"
#include "Features/Settings/UI/settings_tab.h"
#include "Features/ESP/UI/esp_tab.h"
#include "Features/Radar/UI/radar_tab.h"
#include "Features/WebRadar/UI/webradar_tab.h"
#include "Features/ESP/esp.h"

#include <Windows.h>

#include <algorithm>
#include <string>

#include <imgui.h>

namespace
{
    const char* GetStatusText(const esp::DmaHealthStats& stats)
    {
        if (stats.recovering)
            return "Recovering...";
        switch (stats.gameStatus) {
        case esp::GameStatus::Ok:       return "OK";
        case esp::GameStatus::WaitCs2:  return "Wait cs2.exe";
        default:                        return "Unknown";
        }
    }

    ImVec4 GetStatusColor(const esp::DmaHealthStats& stats)
    {
        if (stats.recovering)
            return ImVec4(0.88f, 0.72f, 0.38f, 1.0f);
        switch (stats.gameStatus) {
        case esp::GameStatus::Ok:       return ImVec4(0.52f, 0.82f, 0.56f, 1.0f);
        case esp::GameStatus::WaitCs2:  return ImVec4(0.88f, 0.38f, 0.38f, 1.0f);
        default:                        return ImVec4(0.88f, 0.38f, 0.38f, 1.0f);
        }
    }

    void RenderStatusToast(const ui::MenuState& state)
    {
        const float now = static_cast<float>(ImGui::GetTime());
        if (state.statusText.empty() || now > state.statusUntil)
            return;

        const float fadeIn = std::clamp((now - state.statusShownAt) / 0.16f, 0.0f, 1.0f);
        const float fadeOut = std::clamp((state.statusUntil - now) / 0.22f, 0.0f, 1.0f);
        const float alpha = (fadeIn < fadeOut) ? fadeIn : fadeOut;
        if (alpha <= 0.0f)
            return;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImGuiStyle& style = ImGui::GetStyle();
        const ImVec2 textSize = ImGui::CalcTextSize(state.statusText.c_str());
        const ImVec2 padding(12.0f, 9.0f);
        const float accentWidth = 4.0f;
        const ImVec2 boxSize(
            textSize.x + padding.x * 2.0f + accentWidth,
            textSize.y + padding.y * 2.0f);
        const ImVec2 windowPos = ImGui::GetWindowPos();
        const ImVec2 windowSize = ImGui::GetWindowSize();
        const float titleBarHeight = ImGui::GetFrameHeight() + style.WindowPadding.y + 10.0f;
        const ImVec2 boxMin(
            windowPos.x + windowSize.x - boxSize.x - 18.0f,
            windowPos.y + titleBarHeight);
        const ImVec2 boxMax(boxMin.x + boxSize.x, boxMin.y + boxSize.y);

        const ImU32 bgColor = ImGui::GetColorU32(ImVec4(0.08f, 0.10f, 0.12f, 0.92f * alpha));
        const ImU32 borderColor = ImGui::GetColorU32(ImVec4(0.24f, 0.31f, 0.29f, 0.95f * alpha));
        const ImU32 accentColor = ImGui::GetColorU32(ImVec4(0.38f, 0.84f, 0.64f, 0.95f * alpha));
        const ImU32 textColor = ImGui::GetColorU32(ImVec4(0.92f, 0.96f, 0.94f, 0.98f * alpha));
        const ImU32 shadowColor = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.18f * alpha));

        drawList->AddRectFilled(
            ImVec2(boxMin.x + 1.0f, boxMin.y + 2.0f),
            ImVec2(boxMax.x + 1.0f, boxMax.y + 2.0f),
            shadowColor,
            10.0f);
        drawList->AddRectFilled(boxMin, boxMax, bgColor, 10.0f);
        drawList->AddRect(boxMin, boxMax, borderColor, 10.0f, 0, 1.0f);
        drawList->AddRectFilled(
            boxMin,
            ImVec2(boxMin.x + accentWidth, boxMax.y),
            accentColor,
            10.0f,
            ImDrawFlags_RoundCornersLeft);
        drawList->AddText(
            ImVec2(boxMin.x + padding.x + accentWidth, boxMin.y + padding.y),
            textColor,
            state.statusText.c_str());
    }
}

ui::MenuController::MenuController()
{
    tabs_.push_back(std::make_unique<ui::tabs::EspTab>());
    tabs_.push_back(std::make_unique<ui::tabs::RadarTab>());
    tabs_.push_back(std::make_unique<ui::tabs::WebRadarTab>());
    tabs_.push_back(std::make_unique<ui::tabs::SettingsTab>());
}

void ui::MenuController::Render()
{
    if (!g::menuOpen)
        return;

    EnsureInitialized();
    HandleMenuKeyCapture();

    static const std::string menuTitle = app::build_info::RuntimeTitle() + "###main_menu";
    static bool s_forceStartupCenter = true;

    ImGui::SetNextWindowSize(ImVec2(720, 660), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(640, 520), ImVec2(980, 900));
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    if (displaySize.x > 0.0f && displaySize.y > 0.0f) {
        const ImVec2 centeredPos(displaySize.x * 0.5f, displaySize.y * 0.5f);
        ImGui::SetNextWindowPos(
            centeredPos,
            s_forceStartupCenter ? ImGuiCond_Always : ImGuiCond_Appearing,
            ImVec2(0.5f, 0.5f));
    }
    if (!ImGui::Begin(menuTitle.c_str(), &g::menuOpen, ImGuiWindowFlags_NoCollapse)) {
        s_forceStartupCenter = false;
        ImGui::End();
        return;
    }
    s_forceStartupCenter = false;

    if (displaySize.x > 0.0f && displaySize.y > 0.0f) {
        const ImVec2 windowPos = ImGui::GetWindowPos();
        const ImVec2 windowSize = ImGui::GetWindowSize();
        const bool offscreenHorizontally =
            (windowPos.x + windowSize.x) < 24.0f || windowPos.x > (displaySize.x - 24.0f);
        const bool offscreenVertically =
            (windowPos.y + windowSize.y) < 24.0f || windowPos.y > (displaySize.y - 24.0f);
        if (offscreenHorizontally || offscreenVertically) {
            ImGui::SetWindowPos(ImVec2(
                (displaySize.x - windowSize.x) * 0.5f,
                (displaySize.y - windowSize.y) * 0.5f));
        }
    }

    const float contentIndent = 8.0f;
    ImGui::Indent(contentIndent);

    const esp::DmaHealthStats dmaStats = esp::GetDmaHealthStats();
    ImGui::TextUnformatted("KevqDMA");
    ImGui::SameLine(0, 4);
    ImGui::TextDisabled("| Status:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(GetStatusColor(dmaStats), "%s", GetStatusText(dmaStats));
    ImGui::SameLine(0, 4);
    ImGui::TextDisabled("| Fail: %u", static_cast<unsigned>(dmaStats.consecutiveFailures));

    if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_FittingPolicyResizeDown)) {
        for (const auto& tab : tabs_) {
            if (ImGui::BeginTabItem(tab->Label())) {
                tab->Render(state_, *this);
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    RenderStatusToast(state_);

    ImGui::Unindent(contentIndent);
    ImGui::End();
}

void ui::MenuController::SetStatus(const std::string& text)
{
    state_.statusText = text;
    state_.statusShownAt = static_cast<float>(ImGui::GetTime());
    state_.statusUntil = static_cast<float>(ImGui::GetTime()) + 2.5f;
}

void ui::MenuController::EnsureInitialized()
{
    if (state_.profileInit)
        return;

    menu_utils::CopyToBuffer(state_.profileName, sizeof(state_.profileName), config::GetActiveProfile());
    menu_utils::CopyToBuffer(state_.webMapOverride, sizeof(state_.webMapOverride), g::webRadarMapOverride);
    state_.profileInit = true;
}

namespace
{
    bool IsForbiddenCaptureVk(int vk)
    {
        switch (vk) {
        case VK_LBUTTON:
        case VK_RBUTTON:
        case VK_MBUTTON:
        case VK_XBUTTON1:
        case VK_XBUTTON2:
        case VK_LWIN:
        case VK_RWIN:
        case VK_SHIFT:
        case VK_CONTROL:
        case VK_MENU:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_LMENU:
        case VK_RMENU:
        case VK_END:
        case VK_CANCEL:
        case VK_PACKET:
        case VK_PROCESSKEY:
        case VK_KANA:
#if VK_HANGUL != VK_KANA
        case VK_HANGUL:
#endif
        case VK_JUNJA:
        case VK_FINAL:
        case VK_HANJA:
#if VK_KANJI != VK_HANJA
        case VK_KANJI:
#endif
        case VK_CONVERT:
        case VK_NONCONVERT:
        case VK_ACCEPT:
        case VK_MODECHANGE:
        case VK_APPS:
        case VK_SLEEP:
        case VK_NUMLOCK:
        case VK_SCROLL:
            return true;
        default:
            break;
        }
        if (vk >= 0xE0)
            return true;
        return false;
    }
}

void ui::MenuController::HandleMenuKeyCapture()
{
    if (!state_.waitingForMenuKey) {
        for (int vk = 0; vk < 256; ++vk)
            state_.menuKeyCaptureSnapshot[vk] = 0;
        return;
    }

    for (int vk = 0x08; vk <= 0xFE; ++vk) {
        const bool isDown = (GetAsyncKeyState(vk) & 0x8000) != 0;
        const bool wasDown = state_.menuKeyCaptureSnapshot[vk] != 0;
        state_.menuKeyCaptureSnapshot[vk] = isDown ? 1 : 0;
        if (!isDown || wasDown)
            continue;
        if (IsForbiddenCaptureVk(vk))
            continue;

        g::menuToggleKey = vk;
        state_.waitingForMenuKey = false;
        SetStatus("Menu key updated.");
        for (int reset = 0; reset < 256; ++reset)
            state_.menuKeyCaptureSnapshot[reset] = 0;
        break;
    }
}
