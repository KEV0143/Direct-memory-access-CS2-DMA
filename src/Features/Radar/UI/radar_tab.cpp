#include "Features/Radar/UI/radar_tab.h"

#include "Features/Radar/UI/radar_sections.h"
#include "app/Core/globals.h"
#include "app/UI/MenuShell/menu_state.h"
#include "app/UI/MenuShell/tab_page.h"

#include <imgui.h>

const char* ui::tabs::RadarTab::Label() const
{
    return "Radar";
}

void ui::tabs::RadarTab::Render(MenuState& state, IStatusSink& statusSink)
{
    (void)state;

    ImGui::BeginChild("##radarchild", ImVec2(0, 0), ImGuiChildFlags_Borders);

    const float indent = 8.0f;
    ImGui::Indent(indent);

    ImGui::Checkbox("Enable Radar", &g::radarEnabled);
    ImGui::Spacing();

    if (g::radarEnabled) {
        radar_sections::RenderCalibrationSection(statusSink);

        ImGui::Spacing();
        radar_sections::RenderDisplaySection();
        ImGui::Spacing();
        radar_sections::RenderColorsSection();
    }

    ImGui::Unindent(indent);
    ImGui::EndChild();
}
