static void DrawTextShadow(ImDrawList* drawList,
                           ImFont* font,
                           float fontSize,
                           const ImVec2& pos,
                           ImU32 color,
                           const char* text)
{
    if (!drawList || !text || text[0] == '\0')
        return;
    if (!font)
        font = ImGui::GetFont();
    if (!font)
        return;
    if (fontSize <= 0.0f)
        fontSize = ImGui::GetFontSize();

    drawList->AddText(font, fontSize, ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 220), text);
    drawList->AddText(font, fontSize, pos, color, text);
}

