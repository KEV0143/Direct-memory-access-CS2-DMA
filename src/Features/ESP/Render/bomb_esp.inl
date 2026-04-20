if (g::espEnabled && g::espBombInfo && (bombState.planted || bombState.dropped)) {
    const bool plantedBomb = bombState.planted;
    const bool blowTimerValid =
        std::isfinite(bombState.blowTime) &&
        std::isfinite(bombState.currentGameTime) &&
        bombState.blowTime > bombState.currentGameTime &&
        bombState.blowTime < bombState.currentGameTime + 120.0f;
    const bool defuseTimerValid =
        std::isfinite(bombState.defuseEndTime) &&
        std::isfinite(bombState.currentGameTime) &&
        bombState.defuseEndTime > bombState.currentGameTime &&
        bombState.defuseEndTime < bombState.currentGameTime + 30.0f;
    const float blowLeft = blowTimerValid ? (bombState.blowTime - bombState.currentGameTime) : -1.0f;
    const float defuseLeft = defuseTimerValid ? (bombState.defuseEndTime - bombState.currentGameTime) : -1.0f;

    const float bombFontSz = g::espBombTextSize > 0.0f ? g::espBombTextSize : 0.0f;
    ImFont* bombFont = ImGui::GetFont();
    const float bombFsUse = bombFontSz > 0.0f ? bombFontSz : ImGui::GetFontSize();
    char defText[32] = {};
    char blowText[24] = {};

    if (g::espBombText && plantedBomb && bombState.beingDefused && defuseLeft > 0.0f) {
        std::snprintf(defText, sizeof(defText), "Defuse %.1fs", static_cast<double>(defuseLeft));
        const ImVec2 defSize = bombFont->CalcTextSizeA(bombFsUse, FLT_MAX, 0.0f, defText);
        DrawTextShadow(
            drawList,
            nullptr,
            bombFontSz,
            ImVec2(screenW * 0.5f - defSize.x * 0.5f, 50.0f),
            IM_COL32(255, 180, 120, 240),
            defText);
    }

    const bool hasBombPos = isValidWorldPos(bombState.position);
    ImVec2 boxMin = {};
    ImVec2 boxMax = {};
    bool hasScreenBox = false;

    if (hasBombPos && bombState.boundsValid) {
        hasScreenBox = ProjectWorldAabbToScreen(
            bombState.position,
            bombState.boundsMins,
            bombState.boundsMaxs,
            viewMatrix,
            screenW,
            screenH,
            &boxMin,
            &boxMax);
    }

    if (!hasScreenBox && hasBombPos) {
        Vector3 bombPos = bombState.position;
        bombPos.z += plantedBomb ? 10.0f : 5.0f;
        const ScreenPos bombScreen = WorldToScreen(bombPos, viewMatrix, screenW, screenH);
        if (bombScreen.onScreen) {
            const float halfW = plantedBomb ? 14.0f : 13.0f;
            const float halfH = plantedBomb ? 12.0f : 10.0f;
            boxMin = ImVec2(bombScreen.x - halfW, bombScreen.y - halfH);
            boxMax = ImVec2(bombScreen.x + halfW, bombScreen.y + halfH);
            hasScreenBox = true;
        }
    }

    if (hasScreenBox) {
        drawList->AddRect(
            ImVec2(boxMin.x - 1.0f, boxMin.y - 1.0f),
            ImVec2(boxMax.x + 1.0f, boxMax.y + 1.0f),
            IM_COL32(0, 0, 0, 220),
            2.0f,
            0,
            3.0f);
        drawList->AddRect(boxMin, boxMax, bombCol, 2.0f, 0, 1.8f);

        const float centerX = (boxMin.x + boxMax.x) * 0.5f;

        if (g::espBombText) {
            const char* bombLabel = plantedBomb ? "Bomb" : "Bomb Drop";
            const ImVec2 bombLabelSize = bombFont->CalcTextSizeA(bombFsUse, FLT_MAX, 0.0f, bombLabel);
            const float labelY = boxMin.y - bombLabelSize.y - 4.0f;
            DrawTextShadow(
                drawList,
                nullptr,
                bombFontSz,
                ImVec2(centerX - bombLabelSize.x * 0.5f, labelY),
                bombCol,
                bombLabel);

            if (plantedBomb && bombState.ticking && blowLeft > 0.0f) {
                std::snprintf(blowText, sizeof(blowText), "%.1fs", static_cast<double>(blowLeft));
                const ImVec2 blowSize = bombFont->CalcTextSizeA(bombFsUse, FLT_MAX, 0.0f, blowText);
                DrawTextShadow(
                    drawList,
                    nullptr,
                    bombFontSz,
                    ImVec2(centerX - blowSize.x * 0.5f, boxMax.y + 4.0f),
                    IM_COL32(255, 220, 140, 245),
                    blowText);
            }
        }

        if (plantedBomb && bombState.beingDefused && defuseLeft > 0.0f) {
            const float expectedDefuseTime =
                (defuseLeft > esp::style::kBombDefuseKitHeuristicThreshold)
                    ? esp::style::kBombDefuseNoKitSeconds
                    : esp::style::kBombDefuseKitSeconds;
            const float progress = Clamp01(1.0f - (defuseLeft / expectedDefuseTime));
            const float barW = std::max(54.0f, boxMax.x - boxMin.x);
            const float barH = 4.0f;
            const float barX = centerX - barW * 0.5f;
            const float barY = boxMax.y + 18.0f;
            drawList->AddRectFilled(
                ImVec2(barX - 1.0f, barY - 1.0f),
                ImVec2(barX + barW + 1.0f, barY + barH + 1.0f),
                IM_COL32(0, 0, 0, 220));
            drawList->AddRectFilled(
                ImVec2(barX, barY),
                ImVec2(barX + barW, barY + barH),
                IM_COL32(25, 25, 25, 200));
            drawList->AddRectFilled(
                ImVec2(barX, barY),
                ImVec2(barX + barW * progress, barY + barH),
                IM_COL32(255, 155, 95, 240));
        }
    }
}
