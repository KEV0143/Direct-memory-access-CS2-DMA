if (g::espOffscreenArrows) {
    const float dx = renderPlayerPos.x - renderLocalPos.x;
    const float dy = renderPlayerPos.y - renderLocalPos.y;
    const float relForward = dx * yawCos + dy * yawSin;
    const float relRight = -dx * yawSin + dy * yawCos;
    float dirX = relRight;
    float dirY = -relForward;
    const float len = static_cast<float>(std::hypot(dirX, dirY));
    if (len > 0.001f) {
        dirX /= len;
        dirY /= len;

        constexpr float arrowSize = 14.0f;
        const float radius = (std::min(screenW, screenH) * 0.47f) - arrowSize * 2.5f;
        const ImVec2 center(screenW * 0.5f, screenH * 0.5f);
        const ImVec2 base(center.x + dirX * radius, center.y + dirY * radius);
        const float perpX = -dirY;
        const float perpY = dirX;
        const ImVec2 tip(base.x + dirX * arrowSize, base.y + dirY * arrowSize);
        const ImVec2 left(
            base.x - dirX * arrowSize * 0.85f + perpX * arrowSize * 0.70f,
            base.y - dirY * arrowSize * 0.85f + perpY * arrowSize * 0.70f);
        const ImVec2 right(
            base.x - dirX * arrowSize * 0.85f - perpX * arrowSize * 0.70f,
            base.y - dirY * arrowSize * 0.85f - perpY * arrowSize * 0.70f);
        ImU32 renderArrowCol = offscreenCol;
        if (g::espVisibilityColoring)
            renderArrowCol = p.visible ? visibleCol : hiddenCol;
        drawList->AddTriangleFilled(tip, left, right, renderArrowCol);
        drawList->AddTriangle(tip, left, right, IM_COL32(0, 0, 0, 220), 2.0f);
    }
}
