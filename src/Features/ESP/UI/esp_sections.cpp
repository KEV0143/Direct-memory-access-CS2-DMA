#include "Features/ESP/UI/esp_sections.h"

#include "app/Core/globals.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
    enum class ItemEspGroup {
        Pistols,
        SMGs,
        Rifles,
        Snipers,
        Heavy,
        Gear
    };

    struct ItemEspEntry {
        uint16_t id;
        const char* label;
        ItemEspGroup group;
    };

    enum class EspIcon {
        Enable,
        Preview,
        Box,
        Health,
        Armor,
        Visibility,
        Weapon,
        Skeleton,
        Snap,
        Arrows,
        Flags,
        Item,
        World,
        Bomb,
        Dot
    };

    constexpr ItemEspEntry kItemEspEntries[] = {
        { 1, "Deagle", ItemEspGroup::Pistols },
        { 2, "Dual Berettas", ItemEspGroup::Pistols },
        { 3, "Five-SeveN", ItemEspGroup::Pistols },
        { 4, "Glock", ItemEspGroup::Pistols },
        { 30, "Tec-9", ItemEspGroup::Pistols },
        { 32, "P2000", ItemEspGroup::Pistols },
        { 36, "P250", ItemEspGroup::Pistols },
        { 61, "USP-S", ItemEspGroup::Pistols },
        { 63, "CZ75", ItemEspGroup::Pistols },
        { 64, "R8", ItemEspGroup::Pistols },
        { 17, "MAC-10", ItemEspGroup::SMGs },
        { 19, "P90", ItemEspGroup::SMGs },
        { 23, "MP5", ItemEspGroup::SMGs },
        { 24, "UMP-45", ItemEspGroup::SMGs },
        { 26, "PP-Bizon", ItemEspGroup::SMGs },
        { 33, "MP7", ItemEspGroup::SMGs },
        { 34, "MP9", ItemEspGroup::SMGs },
        { 7, "AK-47", ItemEspGroup::Rifles },
        { 8, "AUG", ItemEspGroup::Rifles },
        { 10, "FAMAS", ItemEspGroup::Rifles },
        { 13, "Galil", ItemEspGroup::Rifles },
        { 16, "M4A4", ItemEspGroup::Rifles },
        { 39, "SG553", ItemEspGroup::Rifles },
        { 60, "M4A1-S", ItemEspGroup::Rifles },
        { 9, "AWP", ItemEspGroup::Snipers },
        { 11, "G3SG1", ItemEspGroup::Snipers },
        { 38, "SCAR-20", ItemEspGroup::Snipers },
        { 40, "SSG08", ItemEspGroup::Snipers },
        { 14, "M249", ItemEspGroup::Heavy },
        { 25, "XM1014", ItemEspGroup::Heavy },
        { 27, "MAG-7", ItemEspGroup::Heavy },
        { 28, "Negev", ItemEspGroup::Heavy },
        { 29, "Sawed-Off", ItemEspGroup::Heavy },
        { 35, "Nova", ItemEspGroup::Heavy },
        { 31, "Zeus", ItemEspGroup::Gear },
        { 57, "Healthshot", ItemEspGroup::Gear },
    };
    constexpr ImGuiColorEditFlags kPickerFlags =
        ImGuiColorEditFlags_AlphaBar |
        ImGuiColorEditFlags_AlphaPreviewHalf;

    constexpr ImGuiColorEditFlags kInlineColorFlags =
        kPickerFlags | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel;

    constexpr float kSliderWidth = 160.0f;
    constexpr float kEspRowHeight = 50.0f;
    constexpr float kEspRowGap = 16.0f;
    constexpr float kEspColumnGap = 18.0f;
    constexpr float kEspRowRounding = 8.0f;
    constexpr float kPi = 3.1415926535f;

    ImU32 ColorU32(int r, int g, int b, int a)
    {
        return IM_COL32(r, g, b, a);
    }

    ImVec2 Pixel(float x, float y)
    {
        return ImVec2(std::floor(x) + 0.5f, std::floor(y) + 0.5f);
    }

    ImVec2 Add(const ImVec2& p, float x, float y)
    {
        return Pixel(p.x + x, p.y + y);
    }

    void DrawGearIcon(ImDrawList* drawList, const ImVec2& center, ImU32 color)
    {
        ImVec2 teeth[16] = {};
        for (int i = 0; i < 16; ++i) {
            const float a = (static_cast<float>(i) * kPi) / 8.0f;
            const float r = (i % 2 == 0) ? 7.4f : 5.7f;
            teeth[i] = Pixel(center.x + std::cos(a) * r, center.y + std::sin(a) * r);
        }
        drawList->AddPolyline(teeth, 16, color, ImDrawFlags_Closed, 1.35f);
        drawList->AddCircle(center, 2.2f, color, 16, 1.25f);
    }

    void DrawSoftIconCircle(ImDrawList* drawList, const ImVec2& center, float radius, ImU32 color)
    {
        drawList->AddCircle(center, radius, color, 28, 1.35f);
    }

    void DrawCornerBoxIcon(ImDrawList* drawList, const ImVec2& p, ImU32 color)
    {
        constexpr float s = 17.0f;
        constexpr float l = 5.0f;
        const ImVec2 q = Add(p, 1.0f, 1.0f);
        drawList->AddLine(Add(q, 0.0f, 0.0f), Add(q, l, 0.0f), color, 1.6f);
        drawList->AddLine(Add(q, 0.0f, 0.0f), Add(q, 0.0f, l), color, 1.6f);
        drawList->AddLine(Add(q, s, 0.0f), Add(q, s - l, 0.0f), color, 1.6f);
        drawList->AddLine(Add(q, s, 0.0f), Add(q, s, l), color, 1.6f);
        drawList->AddLine(Add(q, 0.0f, s), Add(q, l, s), color, 1.6f);
        drawList->AddLine(Add(q, 0.0f, s), Add(q, 0.0f, s - l), color, 1.6f);
        drawList->AddLine(Add(q, s, s), Add(q, s - l, s), color, 1.6f);
        drawList->AddLine(Add(q, s, s), Add(q, s, s - l), color, 1.6f);
    }

    uintptr_t EspIconTexture(EspIcon icon)
    {
        switch (icon) {
        case EspIcon::Enable:
            return g::espUiIcons.enableEsp;
        case EspIcon::Preview:
            return g::espUiIcons.espPreview;
        case EspIcon::Box:
            return g::espUiIcons.cornerBox;
        case EspIcon::Health:
            return g::espUiIcons.healthBar;
        case EspIcon::Armor:
            return g::espUiIcons.armorBar;
        case EspIcon::Visibility:
            return g::espUiIcons.visibilityColors;
        case EspIcon::Weapon:
            return g::espUiIcons.weaponLabel;
        case EspIcon::Skeleton:
            return g::espUiIcons.skeleton;
        case EspIcon::Snap:
            return g::espUiIcons.snapLines;
        case EspIcon::Flags:
            return g::espUiIcons.playerFlags;
        case EspIcon::World:
            return g::espUiIcons.worldEsp;
        case EspIcon::Bomb:
            return g::espUiIcons.bombEsp;
        default:
            return 0;
        }
    }

    void DrawEspIcon(ImDrawList* drawList, EspIcon icon, const ImVec2& pos, float size, ImU32 color)
    {
        const uintptr_t textureId = EspIconTexture(icon);
        if (textureId != 0) {
            drawList->AddImage(
                ImTextureRef(static_cast<ImTextureID>(textureId)),
                Pixel(pos.x, pos.y),
                Pixel(pos.x + size, pos.y + size),
                ImVec2(0.0f, 0.0f),
                ImVec2(1.0f, 1.0f),
                color);
            return;
        }

        constexpr float stroke = 1.65f;
        const ImVec2 c = Add(pos, 10.0f, 10.0f);

        switch (icon) {
        case EspIcon::Enable:
            DrawSoftIconCircle(drawList, c, 7.5f, color);
            drawList->AddLine(Add(c, -3.8f, 0.0f), Add(c, -1.0f, 3.0f), color, 1.9f);
            drawList->AddLine(Add(c, -1.0f, 3.0f), Add(c, 5.0f, -4.2f), color, 1.9f);
            break;
        case EspIcon::Preview:
            drawList->AddRect(Add(pos, 2.0f, 3.0f), Add(pos, 18.0f, 17.0f), color, 3.5f, 0, stroke);
            drawList->AddCircleFilled(Add(pos, 14.0f, 7.0f), 1.6f, color);
            drawList->AddLine(Add(pos, 5.0f, 15.0f), Add(pos, 9.0f, 10.5f), color, stroke);
            drawList->AddLine(Add(pos, 9.0f, 10.5f), Add(pos, 15.0f, 15.0f), color, stroke);
            break;
        case EspIcon::Box:
            DrawCornerBoxIcon(drawList, pos, color);
            break;
        case EspIcon::Health:
            drawList->AddRect(Add(c, -6.8f, -6.8f), Add(c, 6.8f, 6.8f), color, 3.0f, 0, 1.35f);
            drawList->AddLine(Add(c, -3.8f, 0.0f), Add(c, 3.8f, 0.0f), color, 1.65f);
            drawList->AddLine(Add(c, 0.0f, -3.8f), Add(c, 0.0f, 3.8f), color, 1.65f);
            break;
        case EspIcon::Armor: {
            const ImVec2 pts[] = {
                Add(c, 0.0f, -8.2f),
                Add(c, 6.5f, -5.0f),
                Add(c, 5.2f, 5.0f),
                Add(c, 0.0f, 8.3f),
                Add(c, -5.2f, 5.0f),
                Add(c, -6.5f, -5.0f)
            };
            drawList->AddPolyline(pts, 6, color, ImDrawFlags_Closed, stroke);
            break;
        }
        case EspIcon::Visibility:
            drawList->AddCircle(c, 7.6f, color, 30, stroke);
            drawList->AddCircleFilled(Add(c, -3.3f, -2.8f), 1.35f, color, 12);
            drawList->AddCircleFilled(Add(c, 1.2f, -4.0f), 1.35f, color, 12);
            drawList->AddCircleFilled(Add(c, 4.2f, -0.7f), 1.35f, color, 12);
            drawList->AddCircle(Add(c, 1.9f, 3.5f), 2.1f, color, 16, 1.25f);
            break;
        case EspIcon::Weapon:
        {
            const ImVec2 pistol[] = {
                Add(pos, 2.5f, 7.0f),
                Add(pos, 14.5f, 7.0f),
                Add(pos, 17.0f, 9.2f),
                Add(pos, 16.0f, 11.0f),
                Add(pos, 10.0f, 11.0f),
                Add(pos, 8.3f, 16.5f),
                Add(pos, 5.2f, 16.5f),
                Add(pos, 6.0f, 11.0f),
                Add(pos, 2.5f, 11.0f)
            };
            drawList->AddPolyline(pistol, 9, color, ImDrawFlags_Closed, stroke);
            drawList->AddLine(Add(pos, 11.7f, 11.0f), Add(pos, 12.5f, 13.5f), color, 1.2f);
            break;
        }
        case EspIcon::Skeleton:
            drawList->AddCircle(Add(pos, 10.0f, 4.0f), 2.1f, color, 16, 1.3f);
            drawList->AddLine(Add(pos, 10.0f, 6.8f), Add(pos, 10.0f, 12.6f), color, 1.45f);
            drawList->AddLine(Add(pos, 6.4f, 9.4f), Add(pos, 13.6f, 9.4f), color, 1.45f);
            drawList->AddLine(Add(pos, 10.0f, 12.6f), Add(pos, 7.1f, 17.4f), color, 1.45f);
            drawList->AddLine(Add(pos, 10.0f, 12.6f), Add(pos, 12.9f, 17.4f), color, 1.45f);
            break;
        case EspIcon::Snap:
            drawList->AddLine(Add(c, -6.5f, 0.0f), Add(c, -2.7f, 0.0f), color, 1.35f);
            drawList->AddLine(Add(c, 2.7f, 0.0f), Add(c, 6.5f, 0.0f), color, 1.35f);
            drawList->AddLine(Add(c, 0.0f, -6.5f), Add(c, 0.0f, -2.7f), color, 1.35f);
            drawList->AddLine(Add(c, 0.0f, 2.7f), Add(c, 0.0f, 6.5f), color, 1.35f);
            drawList->AddCircle(c, 2.4f, color, 18, 1.25f);
            break;
        case EspIcon::Arrows:
            drawList->AddLine(Add(c, -6.8f, 0.0f), Add(c, 6.8f, 0.0f), color, stroke);
            drawList->AddLine(Add(c, 0.0f, -6.8f), Add(c, 0.0f, 6.8f), color, stroke);
            drawList->AddLine(Add(c, 6.8f, 0.0f), Add(c, 3.8f, -3.0f), color, stroke);
            drawList->AddLine(Add(c, 6.8f, 0.0f), Add(c, 3.8f, 3.0f), color, stroke);
            drawList->AddLine(Add(c, -6.8f, 0.0f), Add(c, -3.8f, -3.0f), color, stroke);
            drawList->AddLine(Add(c, -6.8f, 0.0f), Add(c, -3.8f, 3.0f), color, stroke);
            drawList->AddLine(Add(c, 0.0f, -6.8f), Add(c, -3.0f, -3.8f), color, stroke);
            drawList->AddLine(Add(c, 0.0f, -6.8f), Add(c, 3.0f, -3.8f), color, stroke);
            drawList->AddLine(Add(c, 0.0f, 6.8f), Add(c, -3.0f, 3.8f), color, stroke);
            drawList->AddLine(Add(c, 0.0f, 6.8f), Add(c, 3.0f, 3.8f), color, stroke);
            break;
        case EspIcon::Flags:
            drawList->AddLine(Add(pos, 5.0f, 3.0f), Add(pos, 5.0f, 18.0f), color, stroke);
            drawList->AddLine(Add(pos, 6.0f, 4.0f), Add(pos, 16.2f, 6.7f), color, stroke);
            drawList->AddLine(Add(pos, 16.2f, 6.7f), Add(pos, 6.0f, 10.5f), color, stroke);
            break;
        case EspIcon::Item:
            drawList->AddRect(Add(pos, 5.0f, 5.0f), Add(pos, 15.0f, 15.0f), color, 2.0f, 0, stroke);
            drawList->AddCircleFilled(Add(pos, 15.5f, 4.5f), 1.9f, color);
            break;
        case EspIcon::World:
            drawList->AddCircle(c, 7.8f, color, 30, stroke);
            drawList->AddLine(Add(c, -7.8f, 0.0f), Add(c, 7.8f, 0.0f), color, 1.25f);
            drawList->AddLine(Add(c, 0.0f, -7.8f), Add(c, 0.0f, 7.8f), color, 1.25f);
            drawList->AddEllipse(c, ImVec2(4.0f, 7.8f), color, 0.0f, 22, 1.2f);
            break;
        case EspIcon::Bomb:
            drawList->AddCircle(Add(pos, 9.6f, 11.6f), 5.8f, color, 28, 1.55f);
            drawList->AddRectFilled(Add(pos, 7.1f, 5.2f), Add(pos, 12.1f, 7.2f), color, 1.0f);
            drawList->AddLine(Add(pos, 12.0f, 6.2f), Add(pos, 16.0f, 3.4f), color, 1.35f);
            drawList->AddLine(Add(pos, 16.0f, 3.4f), Add(pos, 18.0f, 4.4f), color, 1.15f);
            drawList->AddCircleFilled(Add(pos, 9.6f, 11.6f), 1.25f, color);
            break;
        case EspIcon::Dot:
            drawList->AddCircleFilled(c, 4.0f, color);
            break;
        default:
            break;
        }
    }

    bool ToggleSwitch(const char* id, bool* value)
    {
        if (!value)
            return false;

        ImGui::PushID(id);
        const ImVec2 size(38.0f, 20.0f);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##toggle", size);
        const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        if (clicked)
            *value = !*value;

        const bool hovered = ImGui::IsItemHovered();
        const float t = *value ? 1.0f : 0.0f;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImU32 trackColor = *value
            ? ColorU32(37, 116, 255, hovered ? 255 : 242)
            : ColorU32(54, 62, 74, hovered ? 245 : 218);
        const ImVec2 min = Pixel(pos.x, pos.y);
        const ImVec2 max = Pixel(pos.x + size.x, pos.y + size.y);
        drawList->AddRectFilled(min, max, trackColor, size.y * 0.5f);
        if (*value) {
            drawList->AddRect(
                Pixel(pos.x - 0.5f, pos.y - 0.5f),
                Pixel(pos.x + size.x + 0.5f, pos.y + size.y + 0.5f),
                ColorU32(96, 158, 255, hovered ? 150 : 108),
                size.y * 0.5f,
                0,
                1.0f);
        }
        drawList->AddCircleFilled(
            Pixel(pos.x + 10.0f + t * 18.0f, pos.y + 10.8f),
            7.0f,
            ColorU32(0, 0, 0, *value ? 58 : 38));
        drawList->AddCircleFilled(
            Pixel(pos.x + 10.0f + t * 18.0f, pos.y + 10.0f),
            6.9f,
            ColorU32(240, 246, 255, 255));

        ImGui::PopID();
        return clicked;
    }

    bool GearButton(const char* id)
    {
        ImGui::PushID(id);
        const ImVec2 size(26.0f, 26.0f);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##gear", size);
        const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        const bool hovered = ImGui::IsItemHovered();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(
            Pixel(pos.x, pos.y),
            Pixel(pos.x + size.x, pos.y + size.y),
            hovered ? ColorU32(25, 36, 52, 248) : ColorU32(14, 22, 34, 238),
            7.0f);
        drawList->AddRect(
            Pixel(pos.x, pos.y),
            Pixel(pos.x + size.x, pos.y + size.y),
            ColorU32(60, 78, 105, hovered ? 245 : 170),
            7.0f);
        DrawGearIcon(drawList, Pixel(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f), ColorU32(170, 184, 205, hovered ? 255 : 220));

        ImGui::PopID();
        return clicked;
    }

    void ColorRow(const char* id, const char* label, float* color);

    template <typename ExtraFn>
    void DrawOptionRow(const char* id,
                       EspIcon icon,
                       const char* label,
                       const char* description,
                       bool* enabled,
                       float* color,
                       ExtraFn&& extraFn,
                       bool showSettings = true,
                       float forcedWidth = 0.0f)
    {
        ImGui::PushID(id);

        const float availableWidth = forcedWidth > 0.0f ? forcedWidth : ImGui::GetContentRegionAvail().x - 2.0f;
        const float rowWidth = std::max(260.0f, availableWidth);
        const ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        const ImVec2 rowPos = Pixel(cursorPos.x, cursorPos.y);
        const ImVec2 rowMax = Pixel(rowPos.x + rowWidth, rowPos.y + kEspRowHeight);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const bool isEnabled = enabled && *enabled;
        const bool rowHovered = ImGui::IsMouseHoveringRect(rowPos, rowMax);

        drawList->AddRectFilled(
            rowPos,
            rowMax,
            rowHovered ? ColorU32(12, 24, 39, 250) : ColorU32(8, 17, 29, 240),
            kEspRowRounding);
        drawList->AddRect(
            rowPos,
            rowMax,
            isEnabled ? ColorU32(38, 92, 162, rowHovered ? 135 : 92) : ColorU32(36, 48, 65, rowHovered ? 105 : 72),
            kEspRowRounding,
            0,
            0.65f);

        if (isEnabled) {
            drawList->AddRectFilled(
                ImVec2(rowPos.x + 1.0f, rowPos.y + 12.0f),
                ImVec2(rowPos.x + 2.0f, rowMax.y - 12.0f),
                ColorU32(37, 113, 255, 82),
                1.5f);
        }

        constexpr float iconSize = 20.0f;
        const ImVec2 iconPos(rowPos.x + 17.0f, rowPos.y + (kEspRowHeight - iconSize) * 0.5f);
        DrawEspIcon(drawList, icon, iconPos, iconSize, ColorU32(205, 216, 232, isEnabled ? 245 : 172));

        const ImVec2 labelPos(rowPos.x + 55.0f, rowPos.y + (kEspRowHeight - ImGui::GetFontSize()) * 0.5f);
        drawList->AddText(labelPos, ColorU32(226, 234, 246, isEnabled ? 255 : 205), label);
        (void)description;

        const float rightPad = 12.0f;
        const float gearWidth = showSettings ? 32.0f : 0.0f;
        const float toggleX = rowMax.x - rightPad - gearWidth - 38.0f - (showSettings ? 8.0f : 0.0f);
        ImGui::SetCursorScreenPos(ImVec2(toggleX, rowPos.y + (kEspRowHeight - 20.0f) * 0.5f));
        ToggleSwitch("toggle", enabled);

        if (showSettings) {
            ImGui::SetCursorScreenPos(ImVec2(rowMax.x - rightPad - 26.0f, rowPos.y + (kEspRowHeight - 26.0f) * 0.5f));
            if (GearButton("settings"))
                ImGui::OpenPopup("##cfg");
        }

        ImGui::SetCursorScreenPos(ImVec2(rowPos.x, rowMax.y + kEspRowGap));
        ImGui::Dummy(ImVec2(rowWidth, 1.0f));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.035f, 0.055f, 0.085f, 0.98f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.16f, 0.24f, 0.36f, 0.82f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.07f, 0.10f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.10f, 0.14f, 0.21f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.20f, 0.48f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.20f, 0.48f, 1.0f, 1.0f));
        ImGui::SetNextWindowSizeConstraints(ImVec2(380.0f, 0.0f), ImVec2(560.0f, FLT_MAX));
        if (ImGui::BeginPopup("##cfg")) {
            ImGui::TextColored(ImVec4(0.86f, 0.92f, 1.0f, 1.0f), "%s Settings", label);
            ImDrawList* popupDrawList = ImGui::GetWindowDrawList();
            const ImVec2 linePos = ImGui::GetCursorScreenPos();
            popupDrawList->AddRectFilled(
                linePos,
                ImVec2(linePos.x + ImGui::GetContentRegionAvail().x, linePos.y + 1.0f),
                ColorU32(52, 105, 186, 138),
                1.0f);
            ImGui::Spacing();
            ImGui::Spacing();

            if (color)
                ColorRow("accent", "Accent Color", color);

            extraFn();

            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(4);

        ImGui::PopID();
    }

    template <typename ExtraFn>
    void DrawFeature(const char* id, const char* label, bool* enabled, float* color, ExtraFn&& extraFn)
    {
        DrawOptionRow(id, EspIcon::Dot, label, "", enabled, color, std::forward<ExtraFn>(extraFn), true);
    }

    void ColorRow(const char* id, const char* label, float* color)
    {
        ImGui::PushID(id);
        const float rowWidth = std::max(300.0f, ImGui::GetContentRegionAvail().x);
        const float rowHeight = 34.0f;
        const ImVec2 rowPos = Pixel(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
        const ImVec2 rowMax = Pixel(rowPos.x + rowWidth, rowPos.y + rowHeight);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(rowPos, rowMax, ColorU32(8, 17, 29, 238), 7.0f);
        drawList->AddRect(rowPos, rowMax, ColorU32(41, 58, 82, 120), 7.0f, 0, 0.65f);
        drawList->AddText(
            ImVec2(rowPos.x + 12.0f, rowPos.y + (rowHeight - ImGui::GetFontSize()) * 0.5f),
            ColorU32(226, 234, 246, 235),
            label);
        ImGui::SetCursorScreenPos(ImVec2(rowMax.x - 40.0f, rowPos.y + 5.0f));
        ImGui::SetNextItemWidth(26.0f);
        ImGui::ColorEdit4("##c", color, kInlineColorFlags);
        ImGui::SetCursorScreenPos(ImVec2(rowPos.x, rowMax.y + 7.0f));
        ImGui::Dummy(ImVec2(rowWidth, 1.0f));
        ImGui::PopID();
    }

    bool ToggleSetting(const char* id, const char* label, bool* toggle)
    {
        ImGui::PushID(id);
        const float rowWidth = std::max(300.0f, ImGui::GetContentRegionAvail().x);
        const float rowHeight = 34.0f;
        const ImVec2 rowPos = Pixel(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
        const ImVec2 rowMax = Pixel(rowPos.x + rowWidth, rowPos.y + rowHeight);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const bool isOn = toggle && *toggle;
        drawList->AddRectFilled(rowPos, rowMax, ColorU32(8, 17, 29, 238), 7.0f);
        drawList->AddRect(
            rowPos,
            rowMax,
            isOn ? ColorU32(38, 92, 162, 102) : ColorU32(41, 58, 82, 116),
            7.0f,
            0,
            0.65f);
        drawList->AddText(
            ImVec2(rowPos.x + 12.0f, rowPos.y + (rowHeight - ImGui::GetFontSize()) * 0.5f),
            ColorU32(226, 234, 246, isOn ? 255 : 218),
            label);
        ImGui::SetCursorScreenPos(ImVec2(rowMax.x - 50.0f, rowPos.y + 7.0f));
        const bool changed = ToggleSwitch("toggle", toggle);
        ImGui::SetCursorScreenPos(ImVec2(rowPos.x, rowMax.y + 7.0f));
        ImGui::Dummy(ImVec2(rowWidth, 1.0f));
        ImGui::PopID();
        return changed;
    }

    void ToggleColorRow(const char* id, const char* label, bool* toggle, float* color)
    {
        ImGui::PushID(id);
        const float rowWidth = std::max(320.0f, ImGui::GetContentRegionAvail().x);
        const float rowHeight = 34.0f;
        const ImVec2 rowPos = Pixel(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
        const ImVec2 rowMax = Pixel(rowPos.x + rowWidth, rowPos.y + rowHeight);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const bool isOn = toggle && *toggle;
        drawList->AddRectFilled(rowPos, rowMax, ColorU32(8, 17, 29, 238), 7.0f);
        drawList->AddRect(
            rowPos,
            rowMax,
            isOn ? ColorU32(38, 92, 162, 102) : ColorU32(41, 58, 82, 116),
            7.0f,
            0,
            0.65f);
        drawList->AddText(
            ImVec2(rowPos.x + 12.0f, rowPos.y + (rowHeight - ImGui::GetFontSize()) * 0.5f),
            ColorU32(226, 234, 246, isOn ? 255 : 218),
            label);
        ImGui::SetCursorScreenPos(ImVec2(rowMax.x - 92.0f, rowPos.y + 7.0f));
        ToggleSwitch("toggle", toggle);
        ImGui::SetCursorScreenPos(ImVec2(rowMax.x - 40.0f, rowPos.y + 5.0f));
        ImGui::SetNextItemWidth(26.0f);
        ImGui::ColorEdit4("##c", color, kInlineColorFlags);
        ImGui::SetCursorScreenPos(ImVec2(rowPos.x, rowMax.y + 7.0f));
        ImGui::Dummy(ImVec2(rowWidth, 1.0f));
        ImGui::PopID();
    }

    void SizeRow(const char* id, const char* label, float* size, float minValue, float maxValue)
    {
        ImGui::PushID(id);
        const float rowWidth = std::max(340.0f, ImGui::GetContentRegionAvail().x);
        const float rowHeight = 38.0f;
        const ImVec2 rowPos = Pixel(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
        const ImVec2 rowMax = Pixel(rowPos.x + rowWidth, rowPos.y + rowHeight);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(rowPos, rowMax, ColorU32(8, 17, 29, 238), 7.0f);
        drawList->AddRect(rowPos, rowMax, ColorU32(41, 58, 82, 116), 7.0f, 0, 0.65f);
        drawList->AddText(
            ImVec2(rowPos.x + 12.0f, rowPos.y + (rowHeight - ImGui::GetFontSize()) * 0.5f),
            ColorU32(226, 234, 246, 230),
            label);
        ImGui::SetCursorScreenPos(ImVec2(rowMax.x - kSliderWidth - 12.0f, rowPos.y + 8.0f));
        ImGui::SetNextItemWidth(kSliderWidth);
        ImGui::SliderFloat("##s", size, minValue, maxValue, *size == 0.0f ? "Default" : "%.0f");
        ImGui::SetCursorScreenPos(ImVec2(rowPos.x, rowMax.y + 7.0f));
        ImGui::Dummy(ImVec2(rowWidth, 1.0f));
        ImGui::PopID();
    }

    void FlagSettingsRow(const char* id, const char* label, bool* toggle, float* color, float* size)
    {
        ImGui::PushID(id);
        const float rowWidth = std::max(430.0f, ImGui::GetContentRegionAvail().x);
        const float rowHeight = 38.0f;
        const ImVec2 rowPos = Pixel(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
        const ImVec2 rowMax = Pixel(rowPos.x + rowWidth, rowPos.y + rowHeight);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const bool isOn = toggle && *toggle;
        drawList->AddRectFilled(rowPos, rowMax, ColorU32(8, 17, 29, 238), 7.0f);
        drawList->AddRect(
            rowPos,
            rowMax,
            isOn ? ColorU32(38, 92, 162, 102) : ColorU32(41, 58, 82, 116),
            7.0f,
            0,
            0.65f);
        drawList->AddText(
            ImVec2(rowPos.x + 12.0f, rowPos.y + (rowHeight - ImGui::GetFontSize()) * 0.5f),
            ColorU32(226, 234, 246, isOn ? 255 : 218),
            label);
        ImGui::SetCursorScreenPos(ImVec2(rowMax.x - 260.0f, rowPos.y + 9.0f));
        ToggleSwitch("toggle", toggle);
        ImGui::SetCursorScreenPos(ImVec2(rowMax.x - 210.0f, rowPos.y + 7.0f));
        ImGui::SetNextItemWidth(26.0f);
        ImGui::ColorEdit4("##c", color, kInlineColorFlags);
        ImGui::SetCursorScreenPos(ImVec2(rowMax.x - 170.0f, rowPos.y + 9.0f));
        ImGui::SetNextItemWidth(158.0f);
        ImGui::SliderFloat("##s", size, 0.0f, 24.0f, *size == 0.0f ? "Default" : "%.0f");
        ImGui::SetCursorScreenPos(ImVec2(rowPos.x, rowMax.y + 7.0f));
        ImGui::Dummy(ImVec2(rowWidth, 1.0f));
        ImGui::PopID();
    }

    const char* ItemEspGroupLabel(ItemEspGroup group)
    {
        switch (group) {
        case ItemEspGroup::Pistols: return "Pistols";
        case ItemEspGroup::SMGs: return "SMGs";
        case ItemEspGroup::Rifles: return "Rifles";
        case ItemEspGroup::Snipers: return "Snipers";
        case ItemEspGroup::Heavy: return "Heavy";
        case ItemEspGroup::Gear: return "Gear";
        default: break;
        }
        return "Items";
    }

    void SetItemEspEnabled(uint16_t id, bool enabled)
    {
        if (id == 0 || id >= 1200)
            return;
        g::espItemEnabledMask.set(id, enabled);
    }

    bool IsItemEspEnabled(uint16_t id)
    {
        if (id == 0 || id >= 1200)
            return false;
        return g::espItemEnabledMask.test(id);
    }

    void SetAllKnownItemsEnabled(bool enabled)
    {
        for (const ItemEspEntry& entry : kItemEspEntries)
            SetItemEspEnabled(entry.id, enabled);
    }

    void RenderItemGroupBlock(ItemEspGroup group)
    {
        ImGui::TextDisabled("%s", ItemEspGroupLabel(group));
        ImGui::Separator();
        ImGui::Spacing();

        for (const ItemEspEntry& entry : kItemEspEntries) {
            if (entry.group != group)
                continue;

            bool enabled = IsItemEspEnabled(entry.id);
            if (ImGui::Checkbox(entry.label, &enabled))
                SetItemEspEnabled(entry.id, enabled);
        }

        ImGui::Spacing();
    }

    float EspGridColumnWidth()
    {
        const float availableWidth = std::max(520.0f, ImGui::GetContentRegionAvail().x);
        return std::floor((availableWidth - kEspColumnGap) * 0.5f);
    }

    template <typename LeftFn, typename RightFn>
    void RenderEspGridPair(float columnWidth, LeftFn&& leftFn, RightFn&& rightFn)
    {
        const ImVec2 start = ImGui::GetCursorScreenPos();
        leftFn(columnWidth);
        ImGui::SetCursorScreenPos(ImVec2(start.x + columnWidth + kEspColumnGap, start.y));
        rightFn(columnWidth);
        ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + kEspRowHeight + kEspRowGap));
    }
}

void ui::tabs::esp_sections::RenderCoreSection()
{
    if (!g::espEnabled) {
        DrawOptionRow("enable", EspIcon::Enable, "Enable ESP", "Enable or disable all ESP features", &g::espEnabled, nullptr, [] {}, false);
        return;
    }

    const float columnWidth = EspGridColumnWidth();
    RenderEspGridPair(
        columnWidth,
        [](float width) { DrawOptionRow("enable", EspIcon::Enable, "Enable ESP", "Enable or disable all ESP features", &g::espEnabled, nullptr, [] {}, false, width); },
        [](float width) { DrawOptionRow("preview", EspIcon::Preview, "ESP Preview", "Show ESP elements preview", &g::espPreviewOpen, nullptr, [] {}, false, width); });
}

void ui::tabs::esp_sections::RenderGeneralSection()
{
    DrawOptionRow("box", EspIcon::Box, "Corner Box", "Draw corner boxes around players", &g::espBox, g::espBoxColor, [] {});
    DrawOptionRow("health", EspIcon::Health, "Health Bar", "Show players health bar", &g::espHealth, g::espHealthColor, [] {
        ToggleSetting("value", "Show Value", &g::espHealthText);
    });
    DrawOptionRow("armor", EspIcon::Armor, "Armor Bar", "Show players armor bar", &g::espArmor, g::espArmorColor, [] {
        ToggleSetting("value", "Show Value", &g::espArmorText);
    });

    DrawOptionRow("vis", EspIcon::Visibility, "Visibility Colors", "Color code by visibility", &g::espVisibilityColoring, g::espVisibleColor, [] {
        ColorRow("occ", "Occluded", g::espHiddenColor);
    });
}

void ui::tabs::esp_sections::RenderOptionsGrid()
{
    const float columnWidth = EspGridColumnWidth();
    const auto renderPair = [columnWidth](auto&& leftFn, auto&& rightFn) {
        RenderEspGridPair(columnWidth, std::forward<decltype(leftFn)>(leftFn), std::forward<decltype(rightFn)>(rightFn));
    };

    renderPair(
        [] (float width) { DrawOptionRow("box", EspIcon::Box, "Corner Box", "", &g::espBox, g::espBoxColor, [] {}, true, width); },
        [] (float width) { DrawOptionRow("skeleton", EspIcon::Skeleton, "Skeleton", "", &g::espSkeleton, g::espSkeletonColor, [] {
                ToggleSetting("dots", "Show Dots", &g::espSkeletonDots);
            }, true, width); });

    renderPair(
        [] (float width) { DrawOptionRow("health", EspIcon::Health, "Health Bar", "", &g::espHealth, g::espHealthColor, [] {
                ToggleSetting("value", "Show Value", &g::espHealthText);
            }, true, width); },
        [] (float width) { DrawOptionRow("snap", EspIcon::Snap, "Snap Lines", "", &g::espSnaplines, g::espSnaplineColor, [] {
                ToggleSetting("top", "Snap From Top", &g::espSnaplineFromTop);
                ImGui::Separator();
                ToggleColorRow("arrows", "Screen Arrows", &g::espOffscreenArrows, g::espOffscreenColor);
                ToggleSetting("sound", "Sound ESP", &g::espSound);
            }, true, width); });

    renderPair(
        [] (float width) { DrawOptionRow("armor", EspIcon::Armor, "Armor Bar", "", &g::espArmor, g::espArmorColor, [] {
                ToggleSetting("value", "Show Value", &g::espArmorText);
            }, true, width); },
        [] (float width) { DrawOptionRow("flags", EspIcon::Flags, "Player Flags", "", &g::espFlags, nullptr, [] {
                FlagSettingsRow("name", "Name", &g::espName, g::espNameColor, &g::espNameFontSize);
                FlagSettingsRow("distance", "Distance", &g::espDistance, g::espDistanceColor, &g::espDistanceSize);
                FlagSettingsRow("blind", "Blind", &g::espFlagBlind, g::espFlagBlindColor, &g::espFlagBlindSize);
                FlagSettingsRow("scoped", "Scoped", &g::espFlagScoped, g::espFlagScopedColor, &g::espFlagScopedSize);
                FlagSettingsRow("defusing", "Defusing", &g::espFlagDefusing, g::espFlagDefusingColor, &g::espFlagDefusingSize);
                FlagSettingsRow("kit", "Kit", &g::espFlagKit, g::espFlagKitColor, &g::espFlagKitSize);
                FlagSettingsRow("money", "Money", &g::espFlagMoney, g::espFlagMoneyColor, &g::espFlagMoneySize);
            }, true, width); });

    renderPair(
        [] (float width) { DrawOptionRow("vis", EspIcon::Visibility, "Visibility Colors", "", &g::espVisibilityColoring, g::espVisibleColor, [] {
                ColorRow("occ", "Occluded", g::espHiddenColor);
            }, true, width); },
        [] (float width) { DrawOptionRow("world", EspIcon::World, "World ESP", "", &g::espWorld, g::espWorldColor, [] {
                ToggleSetting("smoke", "Smoke Timer", &g::espWorldSmokeTimer);
                ToggleSetting("inferno", "Molotov Timer", &g::espWorldInfernoTimer);
                ToggleSetting("decoy", "Decoy Timer", &g::espWorldDecoyTimer);
            }, true, width); });

    renderPair(
        [] (float width) { DrawOptionRow("weapon", EspIcon::Weapon, "Weapon Label", "", &g::espWeapon, nullptr, [] {
                ToggleColorRow("txt", "Label Text", &g::espWeaponText, g::espWeaponTextColor);
                SizeRow("txtsz", "Text Size", &g::espWeaponTextSize, 0.0f, 24.0f);
                ImGui::Separator();
                ToggleColorRow("icon", "Icon Weapon", &g::espWeaponIcon, g::espWeaponIconColor);
                ToggleSetting("knife", "No Knife", &g::espWeaponIconNoKnife);
                SizeRow("iconsz", "Icon Size", &g::espWeaponIconSize, 10.0f, 30.0f);
                ImGui::Separator();
                ToggleColorRow("ammo", "Weapon Ammo", &g::espWeaponAmmo, g::espWeaponAmmoColor);
                SizeRow("ammosz", "Ammo Size", &g::espWeaponAmmoSize, 0.0f, 24.0f);
            }, true, width); },
        [] (float width) { DrawOptionRow("bomb", EspIcon::Bomb, "Bomb ESP", "", &g::espBombInfo, g::espBombColor, [] {
                ToggleSetting("text", "Show Text", &g::espBombText);
                ToggleSetting("timer", "Bomb Time", &g::espBombTime);
                SizeRow("bmbtxtsz", "Text Size", &g::espBombTextSize, 0.0f, 24.0f);
            }, true, width); });

    if (!g::espOffscreenArrows)
        g::espSound = false;
}

void ui::tabs::esp_sections::RenderWeaponSection()
{
    DrawOptionRow("weapon", EspIcon::Weapon, "Weapon Label", "Show weapon name or icon", &g::espWeapon, nullptr, [] {
        ToggleColorRow("txt", "Label Text", &g::espWeaponText, g::espWeaponTextColor);
        SizeRow("txtsz", "Text Size", &g::espWeaponTextSize, 0.0f, 24.0f);
        ImGui::Separator();
        ToggleColorRow("icon", "Icon Weapon", &g::espWeaponIcon, g::espWeaponIconColor);
        ToggleSetting("knife", "No Knife", &g::espWeaponIconNoKnife);
        SizeRow("iconsz", "Icon Size", &g::espWeaponIconSize, 10.0f, 30.0f);
        ImGui::Separator();
        ToggleColorRow("ammo", "Weapon Ammo", &g::espWeaponAmmo, g::espWeaponAmmoColor);
        SizeRow("ammosz", "Ammo Size", &g::espWeaponAmmoSize, 0.0f, 24.0f);
    });
}

void ui::tabs::esp_sections::RenderPlayerVisualsSection()
{
    DrawOptionRow("skeleton", EspIcon::Skeleton, "Skeleton", "Draw player skeleton", &g::espSkeleton, g::espSkeletonColor, [] {
        ToggleSetting("dots", "Show Dots", &g::espSkeletonDots);
    });

    DrawOptionRow("snap", EspIcon::Snap, "Snap Lines", "Draw lines to players", &g::espSnaplines, g::espSnaplineColor, [] {
        ToggleSetting("top", "Snap From Top", &g::espSnaplineFromTop);
        ImGui::Separator();
        ToggleColorRow("arrows", "Screen Arrows", &g::espOffscreenArrows, g::espOffscreenColor);
        ToggleSetting("sound", "Sound ESP", &g::espSound);
    });

    if (!g::espOffscreenArrows)
        g::espSound = false;
}

void ui::tabs::esp_sections::RenderFlagsSection()
{
    DrawOptionRow("flags", EspIcon::Flags, "Player Flags", "Show player status flags", &g::espFlags, nullptr, [] {
        FlagSettingsRow("name", "Name", &g::espName, g::espNameColor, &g::espNameFontSize);
        FlagSettingsRow("distance", "Distance", &g::espDistance, g::espDistanceColor, &g::espDistanceSize);
        FlagSettingsRow("blind", "Blind", &g::espFlagBlind, g::espFlagBlindColor, &g::espFlagBlindSize);
        FlagSettingsRow("scoped", "Scoped", &g::espFlagScoped, g::espFlagScopedColor, &g::espFlagScopedSize);
        FlagSettingsRow("defusing", "Defusing", &g::espFlagDefusing, g::espFlagDefusingColor, &g::espFlagDefusingSize);
        FlagSettingsRow("kit", "Kit", &g::espFlagKit, g::espFlagKitColor, &g::espFlagKitSize);
        FlagSettingsRow("money", "Money", &g::espFlagMoney, g::espFlagMoneyColor, &g::espFlagMoneySize);
    });
}

void ui::tabs::esp_sections::RenderItemSection()
{
    DrawOptionRow("item", EspIcon::Item, "Item ESP", "Show selected dropped items", &g::espItem, g::espWorldColor, [] {
        if (ImGui::Button("Enable All"))
            SetAllKnownItemsEnabled(true);
        ImGui::SameLine();
        if (ImGui::Button("Disable All"))
            SetAllKnownItemsEnabled(false);

        ImGui::Separator();

        ImGui::BeginChild("##itemfilter", ImVec2(0.0f, 380.0f), ImGuiChildFlags_Borders);
        if (ImGui::BeginTable("##itemgrid", 3, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_PadOuterX)) {
            ImGui::TableNextColumn();
            RenderItemGroupBlock(ItemEspGroup::Pistols);
            ImGui::Spacing();
            RenderItemGroupBlock(ItemEspGroup::SMGs);

            ImGui::TableNextColumn();
            RenderItemGroupBlock(ItemEspGroup::Rifles);
            ImGui::Spacing();
            RenderItemGroupBlock(ItemEspGroup::Snipers);

            ImGui::TableNextColumn();
            RenderItemGroupBlock(ItemEspGroup::Heavy);
            ImGui::Spacing();
            RenderItemGroupBlock(ItemEspGroup::Gear);

            ImGui::EndTable();
        }
        ImGui::EndChild();
    });
}

void ui::tabs::esp_sections::RenderWorldSection()
{
    DrawOptionRow("world", EspIcon::World, "World ESP", "Show world utility timers", &g::espWorld, g::espWorldColor, [] {
        ToggleSetting("smoke", "Smoke Timer", &g::espWorldSmokeTimer);
        ToggleSetting("inferno", "Molotov Timer", &g::espWorldInfernoTimer);
        ToggleSetting("decoy", "Decoy Timer", &g::espWorldDecoyTimer);
    });

    DrawOptionRow("bomb", EspIcon::Bomb, "Bomb ESP", "Show planted and dropped C4 info", &g::espBombInfo, g::espBombColor, [] {
        ToggleSetting("text", "Show Text", &g::espBombText);
        ToggleSetting("timer", "Bomb Time", &g::espBombTime);
        SizeRow("bmbtxtsz", "Text Size", &g::espBombTextSize, 0.0f, 24.0f);
    });
}
