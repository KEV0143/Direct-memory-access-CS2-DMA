#include "Features/Settings/UI/settings_tab.h"

#include "Features/Settings/UI/settings_sections.h"
#include "app/UI/MenuShell/menu_state.h"
#include "app/UI/MenuShell/tab_page.h"

#include <imgui.h>

const char* ui::tabs::SettingsTab::Label() const
{
    return "Settings";
}

void ui::tabs::SettingsTab::Render(MenuState& state, IStatusSink& statusSink)
{
    ImGui::BeginChild("##settingschild", ImVec2(0, 0), ImGuiChildFlags_Borders);

    const float panelGap = 14.0f;
    const ImVec2 available = ImGui::GetContentRegionAvail();
    
    
    
    const float safeWidth = (available.x > 100.0f) ? available.x : 680.0f;
    const float leftWidth = (safeWidth - panelGap) * 0.42f;

    ImGui::BeginChild("##settings_left_panel", ImVec2(leftWidth, 0.0f), ImGuiChildFlags_Borders);
    settings_sections::RenderProfilesSection(state, statusSink);
    ImGui::Dummy(ImVec2(0.0f, 12.0f));
    settings_sections::RenderControlsSection(state, statusSink);
    ImGui::EndChild();

    ImGui::SameLine(0.0f, panelGap);

    ImGui::BeginChild("##settings_right_panel", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    settings_sections::RenderScreenSection(statusSink);
    ImGui::Dummy(ImVec2(0.0f, 12.0f));
    static bool s_debugWindowOpen = false;
    if (ImGui::Button(s_debugWindowOpen ? "Close Debug / Telemetry" : "Open Debug / Telemetry",
                       ImVec2(ImGui::GetContentRegionAvail().x, 30.0f)))
        s_debugWindowOpen = !s_debugWindowOpen;
    ImGui::EndChild();

    if (s_debugWindowOpen)
        settings_sections::RenderDebugWindow(&s_debugWindowOpen);

    ImGui::EndChild();
}
