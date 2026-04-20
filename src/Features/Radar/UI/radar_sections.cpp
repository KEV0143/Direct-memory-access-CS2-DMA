#include "Features/Radar/UI/radar_sections.h"

#include "app/Core/globals.h"

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

    ImGui::PushItemWidth(260.0f);
    ImGui::SliderFloat("Size", &g::radarSize, 100.0f, 400.0f, "%.0f");
    ImGui::SliderFloat("Dot Size", &g::radarDotSize, 2.0f, 8.0f, "%.1f");
    ImGui::SliderFloat("Map Rotation", &g::radarWorldRotationDeg, -180.0f, 180.0f, "%.1f deg");
    ImGui::SliderFloat("Map Scale", &g::radarWorldScale, 0.50f, 1.50f, "%.2fx");
    ImGui::SliderFloat("Map Offset X", &g::radarWorldOffsetX, -0.150f, 0.150f, "%.3f");
    ImGui::SliderFloat("Map Offset Y", &g::radarWorldOffsetY, -0.150f, 0.150f, "%.3f");
    ImGui::PopItemWidth();

    ImGui::Spacing();
    if (ImGui::Button("Reset Map Calibration", ImVec2(220, 30))) {
        ResetMapCalibration(statusSink);
    }
}

void ui::tabs::radar_sections::RenderDisplaySection()
{
    const char* modeLabel = (g::radarMode == 0) ? "Heading-Up" : "Static Map";

    if (ImGui::BeginCombo("Radar Mode", modeLabel)) {
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

    ImGui::Checkbox("Local Player Dot", &g::radarShowLocalDot);
    ImGui::Checkbox("View Direction", &g::radarShowAngles);
    ImGui::Checkbox("Show Bomb", &g::radarShowBomb);
    ImGui::Checkbox("Show Crosshair", &g::radarShowCrosshair);
}

void ui::tabs::radar_sections::RenderColorsSection()
{
    const ImGuiColorEditFlags pickerFlags =
        ImGuiColorEditFlags_AlphaBar |
        ImGuiColorEditFlags_AlphaPreviewHalf;
    const ImGuiColorEditFlags inlineClr =
        pickerFlags | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel;
    const float clrX = 130.0f;

    const auto colorRow = [&](const char* id, const char* label, float* color) {
        ImGui::PushID(id);
        ImGui::TextDisabled("%s", label);
        ImGui::SameLine(clrX);
        ImGui::SetNextItemWidth(26.0f);
        ImGui::ColorEdit4("##c", color, inlineClr);
        ImGui::PopID();
    };

    colorRow("dot", "Enemy Dot", g::radarDotColor);
    colorRow("angle", "Direction", g::radarAngleColor);
    colorRow("bomb", "Bomb", g::radarBombColor);
    colorRow("bg", "Background", g::radarBgColor);
    colorRow("border", "Border", g::radarBorderColor);
}
