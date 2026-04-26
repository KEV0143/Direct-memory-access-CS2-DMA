if (g::radarSpectatorList) {
    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBackground;
    if (!g::menuOpen)
        windowFlags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;

    ImGui::SetNextWindowPos(ImVec2(g::radarSpectatorListX, g::radarSpectatorListY), ImGuiCond_Once);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 5.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5.0f, 3.0f));
    const bool useSpectatorFont = g::fontComicSans != nullptr;
    if (useSpectatorFont)
        ImGui::PushFont(g::fontComicSans);
    if (ImGui::Begin("##kevqdma_spectator_list", nullptr, windowFlags)) {
        static bool s_spectatorWindowDragging = false;
        if (g::menuOpen) {
            const ImVec2 pos = ImGui::GetWindowPos();
            g::radarSpectatorListX = pos.x;
            g::radarSpectatorListY = pos.y;
            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
                ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                s_spectatorWindowDragging = true;
            if (s_spectatorWindowDragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                s_spectatorWindowDragging = false;
                config::Save();
            }
        }

        const int spectatorCount = std::clamp(snap.spectatorCount, 0, 64);
        const ImVec2 titlePos = ImGui::GetCursorScreenPos();
        ImDrawList* spectatorDrawList = ImGui::GetWindowDrawList();
        const auto drawSpectatorText = [](ImDrawList* drawList, const ImVec2& pos, ImU32 color, const char* text) {
            ImFont* font = ImGui::GetFont();
            const float fontSize = ImGui::GetFontSize();
            drawList->AddText(font, fontSize, pos, color, text);
            drawList->AddText(font, fontSize, ImVec2(pos.x + 0.45f, pos.y), color, text);
        };
        spectatorDrawList->AddRectFilled(
            ImVec2(titlePos.x, titlePos.y + 2.0f),
            ImVec2(titlePos.x + 3.0f, titlePos.y + 16.0f),
            spectatorCount > 0 ? IM_COL32(90, 205, 255, 230) : IM_COL32(145, 150, 160, 150),
            2.0f);
        char title[32] = {};
        std::snprintf(title, sizeof(title), "Spectators %d", spectatorCount);
        drawSpectatorText(
            spectatorDrawList,
            ImVec2(titlePos.x + 9.0f, titlePos.y),
            spectatorCount > 0 ? IM_COL32(224, 245, 255, 250) : IM_COL32(199, 204, 214, 210),
            title);
        ImGui::Dummy(ImVec2(145.0f, ImGui::GetTextLineHeight()));

        if (spectatorCount > 0) {
            for (int i = 0; i < spectatorCount; ++i) {
                const SpectatorEntry& spectator = snap.spectators[i];
                if (!spectator.valid)
                    continue;
                const ImVec2 rowPos = ImGui::GetCursorScreenPos();
                spectatorDrawList->AddCircleFilled(
                    ImVec2(rowPos.x + 4.0f, rowPos.y + 7.5f),
                    2.0f,
                    IM_COL32(95, 210, 255, 210));
                drawSpectatorText(
                    spectatorDrawList,
                    ImVec2(rowPos.x + 12.0f, rowPos.y),
                    IM_COL32(219, 235, 255, 240),
                    spectator.name[0] ? spectator.name : "Player");
                ImGui::Dummy(ImVec2(180.0f, ImGui::GetTextLineHeight()));
            }
        }
    }
    ImGui::End();
    if (useSpectatorFont)
        ImGui::PopFont();
    ImGui::PopStyleVar(2);
}
