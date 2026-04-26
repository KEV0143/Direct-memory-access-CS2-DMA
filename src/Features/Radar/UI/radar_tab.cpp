#include "Features/Radar/UI/radar_tab.h"

#include "Features/Radar/UI/radar_sections.h"
#include "app/Core/globals.h"
#include "app/UI/MenuShell/menu_state.h"
#include "app/UI/MenuShell/tab_page.h"
#include "app/UI/MenuShell/ui_widgets.h"

#include <imgui.h>

const char* ui::tabs::RadarTab::Label() const
{
    return "Radar";
}

void ui::tabs::RadarTab::Render(MenuState& state, IStatusSink& statusSink)
{
    (void)state;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 8.0f));
    ImGui::BeginChild("##radarchild", ImVec2(0, 0), ImGuiChildFlags_Borders);

    ui::widgets::ToggleRow("enable_radar", "Enable Radar", &g::radarEnabled);

    if (g::radarEnabled) {
        radar_sections::RenderDisplaySection();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        radar_sections::RenderColorsSection();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        radar_sections::RenderCalibrationSection(statusSink);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}
