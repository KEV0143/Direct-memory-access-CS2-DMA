#include "Features/Radar/UI/radar_sections.h"

#include "app/Core/globals.h"
#include "app/UI/MenuShell/ui_widgets.h"

#include <imgui.h>

namespace
{
    void ResetMapCalibration(ui::IStatusSink& statusSink)
    {
        g::radarWorldRotationDeg = 0.0f;
        g::radarWorldScale = 1.0f;
        g::radarWorldOffsetX = 0.0f;
        g::radarWorldOffsetY = 0.0f;
        statusSink.SetStatus("Radar map calibration reset.");
    }
}

void ui::tabs::radar_sections::RenderCalibrationSection(IStatusSink& statusSink)
{
    ImGui::SetNextItemOpen(g::radarCalibrationOpen, ImGuiCond_Always);
    const bool calibrationOpen = ImGui::CollapsingHeader("Calibration", ImGuiTreeNodeFlags_DefaultOpen);
    g::radarCalibrationOpen = calibrationOpen;
    if (!calibrationOpen)
        return;

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ui::widgets::SliderFloatRow("size", "Size", &g::radarSize, 100.0f, 400.0f, "%.0f");
    ui::widgets::SliderFloatRow("dot_size", "Dot Size", &g::radarDotSize, 2.0f, 8.0f, "%.1f");
    ui::widgets::SliderFloatRow("rotation", "Map Rotation", &g::radarWorldRotationDeg, -180.0f, 180.0f, "%.1f deg");
    ui::widgets::SliderFloatRow("scale", "Map Scale", &g::radarWorldScale, 0.50f, 1.50f, "%.2fx");
    ui::widgets::SliderFloatRow("offset_x", "Map Offset X", &g::radarWorldOffsetX, -0.150f, 0.150f, "%.3f");
    ui::widgets::SliderFloatRow("offset_y", "Map Offset Y", &g::radarWorldOffsetY, -0.150f, 0.150f, "%.3f");

    if (ui::widgets::FullButton("Reset Map Calibration", 34.0f))
        ResetMapCalibration(statusSink);
}

void ui::tabs::radar_sections::RenderDisplaySection()
{
    ui::widgets::SectionTitle("Display");
    const char* modeLabel = (g::radarMode == 0) ? "Heading-Up" : "Static Map";

    ImGui::TextDisabled("Radar Mode");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::BeginCombo("##radar_mode", modeLabel)) {
        const bool headingUpSelected = (g::radarMode == 0);
        if (ImGui::Selectable("Heading-Up", headingUpSelected))
            g::radarMode = 0;
        if (headingUpSelected)
            ImGui::SetItemDefaultFocus();

        const bool staticMapSelected = (g::radarMode == 1);
        if (ImGui::Selectable("Static Map", staticMapSelected))
            g::radarMode = 1;
        if (staticMapSelected)
            ImGui::SetItemDefaultFocus();

        ImGui::EndCombo();
    }
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    ui::widgets::TwoColumnRows("##radar_display_1",
        [] { ui::widgets::ToggleRow("local_dot", "Local Player Dot", &g::radarShowLocalDot); },
        [] { ui::widgets::ToggleRow("view_direction", "View Direction", &g::radarShowAngles); });
    ui::widgets::TwoColumnRows("##radar_display_2",
        [] { ui::widgets::ToggleRow("show_bomb", "Show Bomb", &g::radarShowBomb); },
        [] { ui::widgets::ToggleRow("crosshair", "Show Crosshair", &g::radarShowCrosshair); });
    ui::widgets::TwoColumnRows("##radar_display_3",
        [] { ui::widgets::ToggleRow("spectators", "Spectator List", &g::radarSpectatorList); },
        [] { ImGui::Dummy(ImVec2(1.0f, 1.0f)); });
}

void ui::tabs::radar_sections::RenderColorsSection()
{
    ui::widgets::SectionTitle("Colors");
    const ImGuiColorEditFlags pickerFlags =
        ImGuiColorEditFlags_AlphaBar |
        ImGuiColorEditFlags_AlphaPreviewHalf;
    const ImGuiColorEditFlags inlineClr =
        pickerFlags | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel;

    ui::widgets::TwoColumnRows("##radar_colors_1",
        [&] { ui::widgets::ColorRow("dot", "Enemy Dot", g::radarDotColor, inlineClr); },
        [&] { ui::widgets::ColorRow("angle", "Direction", g::radarAngleColor, inlineClr); });
    ui::widgets::TwoColumnRows("##radar_colors_2",
        [&] { ui::widgets::ColorRow("bomb", "Bomb", g::radarBombColor, inlineClr); },
        [&] { ui::widgets::ColorRow("bg", "Background", g::radarBgColor, inlineClr); });
    ui::widgets::TwoColumnRows("##radar_colors_3",
        [&] { ui::widgets::ColorRow("border", "Border", g::radarBorderColor, inlineClr); },
        [] { ImGui::Dummy(ImVec2(1.0f, 1.0f)); });
}
