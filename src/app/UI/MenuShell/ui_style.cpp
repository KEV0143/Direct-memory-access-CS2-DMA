#include "app/UI/MenuShell/ui_style.h"

#include "app/UI/MenuShell/menu_controller.h"

#include <imgui.h>

void ui::ApplyStyle()
{
    ImGui::StyleColorsDark();

    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowPadding = ImVec2(8, 12);
    s.FramePadding = ImVec2(8, 6);
    s.ItemSpacing = ImVec2(10, 7);
    s.ItemInnerSpacing = ImVec2(7, 5);
    s.ScrollbarSize = 13.0f;
    s.GrabMinSize = 10.0f;

    s.WindowRounding = 8.0f;
    s.ChildRounding = 7.0f;
    s.FrameRounding = 5.0f;
    s.PopupRounding = 6.0f;
    s.ScrollbarRounding = 6.0f;
    s.GrabRounding = 5.0f;
    s.TabRounding = 5.0f;

    s.WindowBorderSize = 1.0f;
    s.ChildBorderSize = 1.0f;
    s.FrameBorderSize = 1.0f;
    s.PopupBorderSize = 1.0f;
    s.TabBorderSize = 0.0f;
    s.WindowTitleAlign = ImVec2(0.00f, 0.50f);

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.58f, 0.58f, 0.58f, 1.00f);
    c[ImGuiCol_WindowBg] = ImVec4(0.03f, 0.03f, 0.03f, 0.98f);
    c[ImGuiCol_ChildBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.96f);
    c[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.98f);
    c[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.20f, 0.75f);
    c[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    c[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);

    c[ImGuiCol_TitleBg] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.02f, 0.02f, 0.02f, 0.85f);

    c[ImGuiCol_MenuBarBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);

    c[ImGuiCol_ScrollbarBg] = ImVec4(0.03f, 0.03f, 0.03f, 0.70f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.18f, 0.18f, 0.18f, 0.95f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);

    c[ImGuiCol_CheckMark] = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.88f, 0.88f, 0.88f, 1.00f);

    c[ImGuiCol_Button] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);

    c[ImGuiCol_Header] = ImVec4(0.14f, 0.14f, 0.14f, 0.95f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.20f, 0.20f, 0.20f, 0.98f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);

    c[ImGuiCol_Separator] = ImVec4(0.22f, 0.22f, 0.22f, 0.65f);
    c[ImGuiCol_SeparatorHovered] = ImVec4(0.30f, 0.30f, 0.30f, 0.80f);
    c[ImGuiCol_SeparatorActive] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);

    c[ImGuiCol_Tab] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
    c[ImGuiCol_TabHovered] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    c[ImGuiCol_TabSelected] = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
    c[ImGuiCol_TabDimmed] = ImVec4(0.07f, 0.07f, 0.07f, 1.00f);
    c[ImGuiCol_TabDimmedSelected] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);

    c[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.28f, 0.28f, 0.35f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.70f);
    c[ImGuiCol_ResizeGripActive] = ImVec4(0.52f, 0.52f, 0.52f, 0.95f);
}

void ui::RenderMenu()
{
    static ui::MenuController controller;
    controller.Render();
}
