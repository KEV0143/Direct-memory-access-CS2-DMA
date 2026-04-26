#pragma once

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace ui::widgets {

inline ImU32 ColorU32(int r, int g, int b, int a)
{
    return IM_COL32(r, g, b, a);
}

inline ImVec2 Pixel(float x, float y)
{
    return ImVec2(std::floor(x) + 0.5f, std::floor(y) + 0.5f);
}

inline void SectionTitle(const char* label)
{
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    ImGui::TextColored(ImVec4(0.86f, 0.92f, 1.0f, 1.0f), "%s", label);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(p.x, p.y + 2.0f),
        ImVec2(p.x + ImGui::GetContentRegionAvail().x, p.y + 3.0f),
        ColorU32(43, 84, 148, 98),
        1.0f);
    ImGui::Dummy(ImVec2(0.0f, 9.0f));
}

inline bool ToggleSwitch(const char* id, bool* value)
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

inline void DrawRowFrame(const ImVec2& min, const ImVec2& max, bool active, bool hovered)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        min,
        max,
        hovered ? ColorU32(12, 24, 39, 250) : ColorU32(8, 17, 29, 240),
        8.0f);
    drawList->AddRect(
        min,
        max,
        active ? ColorU32(38, 92, 162, hovered ? 130 : 88) : ColorU32(36, 48, 65, hovered ? 105 : 72),
        8.0f,
        0,
        0.65f);
    if (active) {
        drawList->AddRectFilled(
            ImVec2(min.x + 1.0f, min.y + 11.0f),
            ImVec2(min.x + 2.0f, max.y - 11.0f),
            ColorU32(37, 113, 255, 76),
            1.5f);
    }
}

inline bool ToggleRow(const char* id, const char* label, bool* value)
{
    ImGui::PushID(id);
    const float rowWidth = std::max(260.0f, ImGui::GetContentRegionAvail().x - 2.0f);
    const float rowHeight = 42.0f;
    const ImVec2 rowPos = Pixel(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
    const ImVec2 rowMax = Pixel(rowPos.x + rowWidth, rowPos.y + rowHeight);
    const bool hovered = ImGui::IsMouseHoveringRect(rowPos, rowMax);
    const bool active = value && *value;

    DrawRowFrame(rowPos, rowMax, active, hovered);
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(rowPos.x + 14.0f, rowPos.y + (rowHeight - ImGui::GetFontSize()) * 0.5f),
        ColorU32(226, 234, 246, active ? 255 : 210),
        label);

    ImGui::SetCursorScreenPos(ImVec2(rowMax.x - 52.0f, rowPos.y + (rowHeight - 20.0f) * 0.5f));
    const bool changed = ToggleSwitch("toggle", value);
    ImGui::SetCursorScreenPos(ImVec2(rowPos.x, rowMax.y + 8.0f));
    ImGui::Dummy(ImVec2(rowWidth, 1.0f));
    ImGui::PopID();
    return changed;
}

inline bool CompactToggleRow(const char* id, const char* label, bool* value, float rowWidth)
{
    ImGui::PushID(id);
    rowWidth = std::max(260.0f, rowWidth);
    const float rowHeight = 42.0f;
    const ImVec2 rowPos = Pixel(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
    const ImVec2 rowMax = Pixel(rowPos.x + rowWidth, rowPos.y + rowHeight);
    const bool hovered = ImGui::IsMouseHoveringRect(rowPos, rowMax);
    const bool active = value && *value;

    DrawRowFrame(rowPos, rowMax, active, hovered);
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(rowPos.x + 14.0f, rowPos.y + (rowHeight - ImGui::GetFontSize()) * 0.5f),
        ColorU32(226, 234, 246, active ? 255 : 210),
        label);

    ImGui::SetCursorScreenPos(ImVec2(rowMax.x - 52.0f, rowPos.y + (rowHeight - 20.0f) * 0.5f));
    const bool changed = ToggleSwitch("toggle", value);
    ImGui::SetCursorScreenPos(ImVec2(rowPos.x, rowMax.y + 8.0f));
    ImGui::Dummy(ImVec2(rowWidth, 1.0f));
    ImGui::PopID();
    return changed;
}

template <typename LeftFn, typename RightFn>
inline void TwoColumnRows(const char* id, LeftFn&& leftFn, RightFn&& rightFn)
{
    if (ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        leftFn();
        ImGui::TableSetColumnIndex(1);
        rightFn();
        ImGui::EndTable();
    }
}

inline bool SliderFloatRow(const char* id, const char* label, float* value, float minValue, float maxValue, const char* format)
{
    ImGui::PushID(id);
    const float rowWidth = std::max(320.0f, ImGui::GetContentRegionAvail().x - 2.0f);
    const float rowHeight = 42.0f;
    const ImVec2 rowPos = Pixel(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
    const ImVec2 rowMax = Pixel(rowPos.x + rowWidth, rowPos.y + rowHeight);
    DrawRowFrame(rowPos, rowMax, false, ImGui::IsMouseHoveringRect(rowPos, rowMax));
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(rowPos.x + 14.0f, rowPos.y + (rowHeight - ImGui::GetFontSize()) * 0.5f),
        ColorU32(226, 234, 246, 225),
        label);
    const float sliderWidth = std::min(190.0f, std::max(120.0f, rowWidth - 170.0f));
    ImGui::SetCursorScreenPos(ImVec2(rowMax.x - sliderWidth - 12.0f, rowPos.y + 10.0f));
    ImGui::SetNextItemWidth(sliderWidth);
    const bool changed = ImGui::SliderFloat("##slider", value, minValue, maxValue, format);
    ImGui::SetCursorScreenPos(ImVec2(rowPos.x, rowMax.y + 8.0f));
    ImGui::Dummy(ImVec2(rowWidth, 1.0f));
    ImGui::PopID();
    return changed;
}

inline bool SliderIntRow(const char* id, const char* label, int* value, int minValue, int maxValue, const char* format)
{
    ImGui::PushID(id);
    const float rowWidth = std::max(320.0f, ImGui::GetContentRegionAvail().x - 2.0f);
    const float rowHeight = 42.0f;
    const ImVec2 rowPos = Pixel(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
    const ImVec2 rowMax = Pixel(rowPos.x + rowWidth, rowPos.y + rowHeight);
    DrawRowFrame(rowPos, rowMax, false, ImGui::IsMouseHoveringRect(rowPos, rowMax));
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(rowPos.x + 14.0f, rowPos.y + (rowHeight - ImGui::GetFontSize()) * 0.5f),
        ColorU32(226, 234, 246, 225),
        label);
    const float sliderWidth = std::min(190.0f, std::max(120.0f, rowWidth - 170.0f));
    ImGui::SetCursorScreenPos(ImVec2(rowMax.x - sliderWidth - 12.0f, rowPos.y + 10.0f));
    ImGui::SetNextItemWidth(sliderWidth);
    const bool changed = ImGui::SliderInt("##slider", value, minValue, maxValue, format);
    ImGui::SetCursorScreenPos(ImVec2(rowPos.x, rowMax.y + 8.0f));
    ImGui::Dummy(ImVec2(rowWidth, 1.0f));
    ImGui::PopID();
    return changed;
}

inline bool InputIntRow(const char* id, const char* label, int* value)
{
    ImGui::PushID(id);
    const float rowWidth = std::max(320.0f, ImGui::GetContentRegionAvail().x - 2.0f);
    const float rowHeight = 42.0f;
    const ImVec2 rowPos = Pixel(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
    const ImVec2 rowMax = Pixel(rowPos.x + rowWidth, rowPos.y + rowHeight);
    DrawRowFrame(rowPos, rowMax, false, ImGui::IsMouseHoveringRect(rowPos, rowMax));
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(rowPos.x + 14.0f, rowPos.y + (rowHeight - ImGui::GetFontSize()) * 0.5f),
        ColorU32(226, 234, 246, 225),
        label);
    ImGui::SetCursorScreenPos(ImVec2(rowMax.x - 126.0f, rowPos.y + 9.0f));
    ImGui::SetNextItemWidth(114.0f);
    const bool changed = ImGui::InputInt("##input", value, 0, 0);
    ImGui::SetCursorScreenPos(ImVec2(rowPos.x, rowMax.y + 8.0f));
    ImGui::Dummy(ImVec2(rowWidth, 1.0f));
    ImGui::PopID();
    return changed;
}

inline bool CompactInputIntRow(const char* id, const char* label, int* value, float rowWidth)
{
    ImGui::PushID(id);
    rowWidth = std::max(260.0f, rowWidth);
    const float rowHeight = 42.0f;
    const ImVec2 rowPos = Pixel(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
    const ImVec2 rowMax = Pixel(rowPos.x + rowWidth, rowPos.y + rowHeight);
    DrawRowFrame(rowPos, rowMax, false, ImGui::IsMouseHoveringRect(rowPos, rowMax));
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(rowPos.x + 14.0f, rowPos.y + (rowHeight - ImGui::GetFontSize()) * 0.5f),
        ColorU32(226, 234, 246, 225),
        label);
    ImGui::SetCursorScreenPos(ImVec2(rowMax.x - 126.0f, rowPos.y + 9.0f));
    ImGui::SetNextItemWidth(114.0f);
    const bool changed = ImGui::InputInt("##input", value, 0, 0);
    ImGui::SetCursorScreenPos(ImVec2(rowPos.x, rowMax.y + 8.0f));
    ImGui::Dummy(ImVec2(rowWidth, 1.0f));
    ImGui::PopID();
    return changed;
}

inline bool CompactInputIntRowAt(const char* id, const char* label, int* value, float rowWidth, float inputX)
{
    ImGui::PushID(id);
    rowWidth = std::max(260.0f, rowWidth);
    inputX = std::clamp(inputX, 80.0f, rowWidth - 126.0f);
    const float rowHeight = 42.0f;
    const ImVec2 rowPos = Pixel(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
    const ImVec2 rowMax = Pixel(rowPos.x + rowWidth, rowPos.y + rowHeight);
    DrawRowFrame(rowPos, rowMax, false, ImGui::IsMouseHoveringRect(rowPos, rowMax));
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(rowPos.x + 14.0f, rowPos.y + (rowHeight - ImGui::GetFontSize()) * 0.5f),
        ColorU32(226, 234, 246, 225),
        label);
    ImGui::SetCursorScreenPos(ImVec2(rowPos.x + inputX, rowPos.y + 9.0f));
    ImGui::SetNextItemWidth(114.0f);
    const bool changed = ImGui::InputInt("##input", value, 0, 0);
    ImGui::SetCursorScreenPos(ImVec2(rowPos.x, rowMax.y + 8.0f));
    ImGui::Dummy(ImVec2(rowWidth, 1.0f));
    ImGui::PopID();
    return changed;
}

inline void ColorRow(const char* id, const char* label, float* color, ImGuiColorEditFlags flags)
{
    ImGui::PushID(id);
    const float rowWidth = std::max(260.0f, ImGui::GetContentRegionAvail().x - 2.0f);
    const float rowHeight = 42.0f;
    const ImVec2 rowPos = Pixel(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
    const ImVec2 rowMax = Pixel(rowPos.x + rowWidth, rowPos.y + rowHeight);
    DrawRowFrame(rowPos, rowMax, false, ImGui::IsMouseHoveringRect(rowPos, rowMax));
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(rowPos.x + 14.0f, rowPos.y + (rowHeight - ImGui::GetFontSize()) * 0.5f),
        ColorU32(226, 234, 246, 225),
        label);
    ImGui::SetCursorScreenPos(ImVec2(rowMax.x - 42.0f, rowPos.y + 8.0f));
    ImGui::SetNextItemWidth(28.0f);
    ImGui::ColorEdit4("##color", color, flags);
    ImGui::SetCursorScreenPos(ImVec2(rowPos.x, rowMax.y + 8.0f));
    ImGui::Dummy(ImVec2(rowWidth, 1.0f));
    ImGui::PopID();
}

inline bool FullButton(const char* label, float height = 32.0f)
{
    const float width = ImGui::GetContentRegionAvail().x;
    return ImGui::Button(label, ImVec2(width, height));
}

inline bool CompactButton(const char* label, float width, float height = 32.0f)
{
    width = std::max(120.0f, width);
    return ImGui::Button(label, ImVec2(width, height));
}

} 
