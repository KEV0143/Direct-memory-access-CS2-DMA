
if (g::espBox) {
    float cornerLen = boxHeight * 0.25f;
    if (cornerLen < 5.0f) cornerLen = 5.0f;
    DrawCornerBox(drawList, boxLeft, boxTop, boxWidth, boxHeight,
                  entityCol, IM_COL32(0, 0, 0, 220), cornerLen, 2.0f);
}

if (g::espBombInfo && p.hasBomb && !bombState.dropped && !bombState.planted) {
    const float pad = 4.0f;
    const ImVec2 minPt(boxLeft - pad, boxTop - pad);
    const ImVec2 maxPt(boxLeft + boxWidth + pad, boxTop + boxHeight + pad);
    drawList->AddRect(
        ImVec2(minPt.x - 1.0f, minPt.y - 1.0f),
        ImVec2(maxPt.x + 1.0f, maxPt.y + 1.0f),
        IM_COL32(0, 0, 0, 220),
        0.0f,
        0,
        3.0f);
    drawList->AddRect(minPt, maxPt, bombCol, 0.0f, 0, 1.8f);
}
