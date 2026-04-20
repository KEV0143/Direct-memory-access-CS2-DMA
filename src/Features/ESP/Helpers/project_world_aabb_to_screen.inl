static bool ProjectWorldAabbToScreen(const Vector3& origin,
                                     const Vector3& mins,
                                     const Vector3& maxs,
                                     const view_matrix_t& matrix,
                                     float screenW,
                                     float screenH,
                                     ImVec2* outMin,
                                     ImVec2* outMax)
{
    const Vector3 corners[8] = {
        origin + Vector3(mins.x, mins.y, mins.z),
        origin + Vector3(mins.x, maxs.y, mins.z),
        origin + Vector3(maxs.x, mins.y, mins.z),
        origin + Vector3(maxs.x, maxs.y, mins.z),
        origin + Vector3(mins.x, mins.y, maxs.z),
        origin + Vector3(mins.x, maxs.y, maxs.z),
        origin + Vector3(maxs.x, mins.y, maxs.z),
        origin + Vector3(maxs.x, maxs.y, maxs.z),
    };

    float minX = FLT_MAX;
    float minY = FLT_MAX;
    float maxX = -FLT_MAX;
    float maxY = -FLT_MAX;
    int projected = 0;

    for (const Vector3& corner : corners) {
        const ScreenPos sp = WorldToScreen(corner, matrix, screenW, screenH);
        if (!sp.onScreen)
            continue;

        minX = std::min(minX, sp.x);
        minY = std::min(minY, sp.y);
        maxX = std::max(maxX, sp.x);
        maxY = std::max(maxY, sp.y);
        ++projected;
    }

    if (projected < 2)
        return false;
    if (!std::isfinite(minX) || !std::isfinite(minY) || !std::isfinite(maxX) || !std::isfinite(maxY))
        return false;
    if ((maxX - minX) < 2.0f || (maxY - minY) < 2.0f)
        return false;

    if (outMin)
        *outMin = ImVec2(minX, minY);
    if (outMax)
        *outMax = ImVec2(maxX, maxY);
    return true;
}
