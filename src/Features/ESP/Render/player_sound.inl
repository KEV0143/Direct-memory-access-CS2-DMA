if (g::espSound && p.soundUntilMs > nowMs) {
    const float remain = static_cast<float>(p.soundUntilMs - nowMs);
    const float strength = Clamp01(1.0f - (remain / 420.0f));
    const float radiusA = 10.0f + 18.0f * strength;
    const float radiusB = radiusA + 7.0f;
    constexpr int sr = 80;
    constexpr int sg = 155;
    constexpr int sb = 255;
    const int alphaA = static_cast<int>(140.0f * (1.0f - 0.2f * strength));
    const int alphaB = static_cast<int>(70.0f * (1.0f - 0.3f * strength));
    drawList->AddCircle(ImVec2(screenFeet.x, screenFeet.y), radiusA, IM_COL32(sr, sg, sb, alphaA), 16, 1.8f);
    drawList->AddCircle(ImVec2(screenFeet.x, screenFeet.y), radiusB, IM_COL32(sr, sg, sb, alphaB), 16, 1.2f);
}
