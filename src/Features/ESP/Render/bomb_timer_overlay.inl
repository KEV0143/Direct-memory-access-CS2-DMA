if (g::espBombInfo && g::espBombTime) {
    constexpr float kBombTimerMaxSeconds = 40.0f;
    constexpr float kBombTimerMinSeconds = 5.0f;
    constexpr float kBombTimerHardMaxSeconds = 90.0f;
    static float s_bombTimerFrac = 1.0f;
    static float s_defuseTimerFrac = 0.0f;
    static float s_bombDisplayedLeft = kBombTimerMaxSeconds;
    static float s_defuseDisplayedLeft = 0.0f;
    static float s_defuseDisplayedTotal = esp::style::kBombDefuseNoKitSeconds;
    static bool s_prevBombTimerVisible = false;
    static bool s_prevDefusing = false;
    static double s_expiredSuppressUntil = 0.0;
    const double timerNow = ImGui::GetTime();

    const float bombTotal =
        std::isfinite(bombState.timerLength) &&
        bombState.timerLength >= kBombTimerMinSeconds &&
        bombState.timerLength <= kBombTimerHardMaxSeconds
            ? bombState.timerLength
            : kBombTimerMaxSeconds;
    const bool blowTimerValid =
        std::isfinite(bombState.blowTime) &&
        std::isfinite(bombState.currentGameTime) &&
        bombState.blowTime > bombState.currentGameTime &&
        bombState.blowTime <= bombState.currentGameTime + bombTotal + 1.0f;
    const bool blowTimerRawPresent =
        std::isfinite(bombState.blowTime) &&
        std::isfinite(bombState.currentGameTime) &&
        bombState.blowTime > bombState.currentGameTime;
    const float defuseTotalFromState =
        std::isfinite(bombState.defuseLength) &&
        bombState.defuseLength >= 1.0f &&
        bombState.defuseLength <= 15.0f
            ? bombState.defuseLength
            : esp::style::kBombDefuseNoKitSeconds;
    const bool defuseTimerValid =
        std::isfinite(bombState.defuseEndTime) &&
        std::isfinite(bombState.currentGameTime) &&
        bombState.defuseEndTime > bombState.currentGameTime &&
        bombState.defuseEndTime <= bombState.currentGameTime + defuseTotalFromState + 1.0f;
    const bool liveTimerVisible = bombState.ticking || (bombState.planted && bombState.beingDefused);
    const bool timerSuppressed = liveTimerVisible && timerNow < s_expiredSuppressUntil;

    if (liveTimerVisible && !timerSuppressed) {
        const float rawBlowLeft = blowTimerRawPresent
            ? std::max(0.0f, bombState.blowTime - bombState.currentGameTime)
            : 0.0f;
        const float rawDefuseLeft = defuseTimerValid
            ? std::max(0.0f, bombState.defuseEndTime - bombState.currentGameTime)
            : 0.0f;

        const float frameDt = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.10f);
        if (blowTimerValid) {
            s_bombDisplayedLeft = std::clamp(rawBlowLeft, 0.0f, bombTotal);
        } else if (!s_prevBombTimerVisible || s_bombDisplayedLeft <= 0.0f) {
            s_bombDisplayedLeft = bombTotal;
        } else {
            s_bombDisplayedLeft = std::max(0.0f, s_bombDisplayedLeft - frameDt);
        }
        s_prevBombTimerVisible = true;
        if (s_bombDisplayedLeft <= 0.03f) {
            s_expiredSuppressUntil = timerNow + 1.75;
            s_prevBombTimerVisible = false;
            s_prevDefusing = false;
            s_bombTimerFrac = 1.0f;
            s_defuseTimerFrac = 0.0f;
            s_defuseDisplayedLeft = 0.0f;
        }

        if (bombState.beingDefused) {
            const float inferredDefuseTotal = defuseTotalFromState;
            const float normalizedRawDefuseLeft =
                defuseTimerValid
                    ? std::clamp(rawDefuseLeft, 0.0f, inferredDefuseTotal)
                    : inferredDefuseTotal;
            if (!s_prevDefusing || s_defuseDisplayedLeft <= 0.0f) {
                s_defuseDisplayedTotal = inferredDefuseTotal;
                s_defuseDisplayedLeft = normalizedRawDefuseLeft;
            } else {
                s_defuseDisplayedLeft = std::max(0.0f, s_defuseDisplayedLeft - frameDt);
                if (defuseTimerValid && normalizedRawDefuseLeft < s_defuseDisplayedLeft)
                    s_defuseDisplayedLeft = normalizedRawDefuseLeft;
            }
            s_prevDefusing = true;
        } else {
            s_prevDefusing = false;
            s_defuseDisplayedLeft = 0.0f;
            s_defuseDisplayedTotal = esp::style::kBombDefuseNoKitSeconds;
        }

        if (timerNow >= s_expiredSuppressUntil) {
        const bool showDefuse = bombState.beingDefused && s_defuseDisplayedLeft > 0.0f;
        const float targetBombFrac = Clamp01(s_bombDisplayedLeft / std::max(1.0f, bombTotal));
        const float targetDefuseFrac = showDefuse
            ? Clamp01(s_defuseDisplayedLeft / std::max(1.0f, s_defuseDisplayedTotal))
            : 0.0f;
        const float smoothing = std::clamp(ImGui::GetIO().DeltaTime * 12.0f, 0.0f, 1.0f);
        s_bombTimerFrac += (targetBombFrac - s_bombTimerFrac) * smoothing;
        s_defuseTimerFrac += (targetDefuseFrac - s_defuseTimerFrac) * smoothing;

        ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBackground;
        if (!g::menuOpen)
            windowFlags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;

        g::espBombTimerX = std::clamp(g::espBombTimerX, 0.0f, std::max(0.0f, screenW - 230.0f));
        g::espBombTimerY = std::clamp(g::espBombTimerY, 0.0f, std::max(0.0f, screenH - 90.0f));
        ImGui::SetNextWindowPos(ImVec2(g::espBombTimerX, g::espBombTimerY), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5.0f, 5.0f));
        const bool useBombTimerFont = g::fontComicSans != nullptr;
        if (useBombTimerFont)
            ImGui::PushFont(g::fontComicSans);
        if (ImGui::Begin("##kevqdma_bomb_timer", nullptr, windowFlags)) {
            static bool s_bombTimerDragging = false;
            const auto drawTimerText = [](ImDrawList* drawList, const ImVec2& pos, ImU32 color, const char* text) {
                ImFont* font = ImGui::GetFont();
                const float fontSize = ImGui::GetFontSize();
                drawList->AddText(font, fontSize, pos, color, text);
                drawList->AddText(font, fontSize, ImVec2(pos.x + 0.55f, pos.y), color, text);
            };

            if (g::menuOpen) {
                if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
                    ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    s_bombTimerDragging = true;
                if (s_bombTimerDragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    const ImVec2 delta = ImGui::GetIO().MouseDelta;
                    g::espBombTimerX += delta.x;
                    g::espBombTimerY += delta.y;
                    ImGui::SetWindowPos(ImVec2(g::espBombTimerX, g::espBombTimerY), ImGuiCond_Always);
                } else if (s_bombTimerDragging) {
                    s_bombTimerDragging = false;
                    config::Save();
                }
            }

            char title[48] = {};
            std::snprintf(title, sizeof(title), "C4  %.1fs", static_cast<double>(s_bombDisplayedLeft));
            const ImVec2 titlePos = ImGui::GetCursorScreenPos();
            ImDrawList* timerDrawList = ImGui::GetWindowDrawList();
            timerDrawList->AddRectFilled(
                ImVec2(titlePos.x, titlePos.y + 2.0f),
                ImVec2(titlePos.x + 3.0f, titlePos.y + 17.0f),
                s_bombDisplayedLeft <= 10.0f ? IM_COL32(255, 82, 72, 240) : IM_COL32(255, 192, 72, 240),
                2.0f);
            drawTimerText(
                timerDrawList,
                ImVec2(titlePos.x + 10.0f, titlePos.y),
                s_bombDisplayedLeft <= 10.0f ? IM_COL32(255, 104, 96, 250) : IM_COL32(255, 202, 82, 250),
                title);
            ImGui::Dummy(ImVec2(204.0f, ImGui::GetTextLineHeight()));

            const ImVec2 barPos = ImGui::GetCursorScreenPos();
            const float barW = 204.0f;
            const float barH = 8.0f;
            const ImU32 bombBarColor = s_bombDisplayedLeft <= 10.0f
                ? IM_COL32(255, 82, 72, 245)
                : IM_COL32(255, 194, 72, 245);
            const ImU32 bombBarFrameColor = s_bombDisplayedLeft <= 10.0f
                ? IM_COL32(255, 82, 72, 70)
                : IM_COL32(255, 194, 72, 70);
            const ImU32 bombBarTrackColor = s_bombDisplayedLeft <= 10.0f
                ? IM_COL32(74, 24, 24, 150)
                : IM_COL32(72, 55, 22, 150);
            timerDrawList->AddRectFilled(
                ImVec2(barPos.x - 1.0f, barPos.y - 1.0f),
                ImVec2(barPos.x + barW + 1.0f, barPos.y + barH + 1.0f),
                bombBarFrameColor,
                5.0f);
            timerDrawList->AddRectFilled(
                barPos,
                ImVec2(barPos.x + barW, barPos.y + barH),
                bombBarTrackColor,
                4.0f);
            if (s_bombTimerFrac > 0.0f) {
                timerDrawList->AddRectFilled(
                    barPos,
                    ImVec2(barPos.x + std::max(barH, barW * s_bombTimerFrac), barPos.y + barH),
                    bombBarColor,
                    4.0f);
            }
            ImGui::Dummy(ImVec2(barW, barH));

            if (showDefuse) {
                char defuseText[56] = {};
                std::snprintf(defuseText, sizeof(defuseText), "Defuse  %.1fs", static_cast<double>(s_defuseDisplayedLeft));
                const ImVec2 defuseTextPos = ImGui::GetCursorScreenPos();
                drawTimerText(
                    timerDrawList,
                    defuseTextPos,
                    s_defuseDisplayedLeft <= s_bombDisplayedLeft ? IM_COL32(100, 255, 132, 245) : IM_COL32(255, 92, 82, 245),
                    defuseText);
                ImGui::Dummy(ImVec2(barW, ImGui::GetTextLineHeight()));
                const ImVec2 defBarPos = ImGui::GetCursorScreenPos();
                const bool defuseCanFinish = s_defuseDisplayedLeft <= s_bombDisplayedLeft;
                const ImU32 defuseBarColor = defuseCanFinish
                    ? IM_COL32(72, 235, 108, 245)
                    : IM_COL32(255, 82, 72, 245);
                const ImU32 defuseBarFrameColor = defuseCanFinish
                    ? IM_COL32(72, 235, 108, 70)
                    : IM_COL32(255, 82, 72, 70);
                const ImU32 defuseBarTrackColor = defuseCanFinish
                    ? IM_COL32(19, 62, 30, 150)
                    : IM_COL32(74, 24, 24, 150);
                timerDrawList->AddRectFilled(
                    ImVec2(defBarPos.x - 1.0f, defBarPos.y - 1.0f),
                    ImVec2(defBarPos.x + barW + 1.0f, defBarPos.y + barH + 1.0f),
                    defuseBarFrameColor,
                    5.0f);
                timerDrawList->AddRectFilled(
                    defBarPos,
                    ImVec2(defBarPos.x + barW, defBarPos.y + barH),
                    defuseBarTrackColor,
                    4.0f);
                if (s_defuseTimerFrac > 0.0f) {
                    timerDrawList->AddRectFilled(
                        defBarPos,
                        ImVec2(defBarPos.x + std::max(barH, barW * s_defuseTimerFrac), defBarPos.y + barH),
                        defuseBarColor,
                        4.0f);
                }
                ImGui::Dummy(ImVec2(barW, barH));
            }
        }
        ImGui::End();
        if (useBombTimerFont)
            ImGui::PopFont();
        ImGui::PopStyleVar(2);
        }
    }
    else {
        s_prevBombTimerVisible = false;
        s_prevDefusing = false;
        s_bombTimerFrac = 1.0f;
        s_defuseTimerFrac = 0.0f;
        s_defuseDisplayedLeft = 0.0f;
    }
}
