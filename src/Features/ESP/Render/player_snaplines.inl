if (g::espSnaplines) {
    float originY = g::espSnaplineFromTop ? 0.0f : screenH;
    const ImVec2 from(screenW * 0.5f, originY);
    const ImVec2 to(screenFeet.x, screenFeet.y);
    drawList->AddLine(from, to, IM_COL32(0, 0, 0, 120), 2.5f);
    drawList->AddLine(from, to, snapCol, 1.0f);
}
