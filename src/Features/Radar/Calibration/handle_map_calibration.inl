static void HandleMapCalibration(const Vector3& mins, const Vector3& maxs)
{
    if (!IsBoundsValid(mins, maxs))
        return;

    const std::string mapKey = BuildMapKeyFromBounds(mins, maxs);
    if (mapKey.empty())
        return;

    if (mapKey != s_activeMapKey)
    {
        s_activeMapKey = mapKey;
        float loadedRotation = 0.0f;
        float loadedScale = 1.0f;
        float loadedBaseOffsetX = 0.0f;
        float loadedBaseOffsetY = 0.0f;
        float loadedOffsetX = 0.0f;
        float loadedOffsetY = 0.0f;
        bool loadedHasOverview = false;
        float loadedOverviewPosX = 0.0f;
        float loadedOverviewPosY = 0.0f;
        float loadedOverviewScale = 0.0f;
        if (LoadRadarCalibrationForMap(
                mapKey,
                &loadedRotation,
                &loadedScale,
                &loadedBaseOffsetX,
                &loadedBaseOffsetY,
                &loadedOffsetX,
                &loadedOffsetY,
                &loadedHasOverview,
                &loadedOverviewPosX,
                &loadedOverviewPosY,
                &loadedOverviewScale))
        {
            g::radarWorldRotationDeg = loadedRotation;
            g::radarWorldScale = loadedScale;
            s_activeMapBaseOffsetX = loadedBaseOffsetX;
            s_activeMapBaseOffsetY = loadedBaseOffsetY;
            g::radarWorldOffsetX = loadedOffsetX;
            g::radarWorldOffsetY = loadedOffsetY;
            s_activeMapOverviewAvailable = loadedHasOverview;
            s_activeMapOverviewPosX = loadedOverviewPosX;
            s_activeMapOverviewPosY = loadedOverviewPosY;
            s_activeMapOverviewScale = loadedOverviewScale;
        }
        else
        {
            s_activeMapBaseOffsetX = 0.0f;
            s_activeMapBaseOffsetY = 0.0f;
            SaveRadarCalibrationForMap(
                mapKey,
                g::radarWorldRotationDeg,
                g::radarWorldScale,
                s_activeMapBaseOffsetX,
                s_activeMapBaseOffsetY,
                g::radarWorldOffsetX,
                g::radarWorldOffsetY);
            s_activeMapOverviewAvailable = false;
            s_activeMapOverviewPosX = 0.0f;
            s_activeMapOverviewPosY = 0.0f;
            s_activeMapOverviewScale = 0.0f;
        }

        s_lastSavedMapRotation = g::radarWorldRotationDeg;
        s_lastSavedMapScale = g::radarWorldScale;
        s_lastSavedMapOffsetX = g::radarWorldOffsetX;
        s_lastSavedMapOffsetY = g::radarWorldOffsetY;
        s_lastMapPersistTime = std::chrono::steady_clock::now();
        return;
    }

    const float rotNow = std::clamp(g::radarWorldRotationDeg, -180.0f, 180.0f);
    const float scaleNow = std::clamp(g::radarWorldScale, 0.50f, 1.50f);
    const float offsetXNow = std::clamp(g::radarWorldOffsetX, -0.25f, 0.25f);
    const float offsetYNow = std::clamp(g::radarWorldOffsetY, -0.25f, 0.25f);
    const bool changed = std::fabs(rotNow - s_lastSavedMapRotation) > 0.01f ||
                         std::fabs(scaleNow - s_lastSavedMapScale) > 0.001f ||
                         std::fabs(offsetXNow - s_lastSavedMapOffsetX) > 0.0005f ||
                         std::fabs(offsetYNow - s_lastSavedMapOffsetY) > 0.0005f;

    if (!changed)
        return;

    const auto now = std::chrono::steady_clock::now();
    if (now - s_lastMapPersistTime < std::chrono::milliseconds(1200))
        return;

    SaveRadarCalibrationForMap(
        s_activeMapKey,
        rotNow,
        scaleNow,
        s_activeMapBaseOffsetX,
        s_activeMapBaseOffsetY,
        offsetXNow,
        offsetYNow);
    s_lastSavedMapRotation = rotNow;
    s_lastSavedMapScale = scaleNow;
    s_lastSavedMapOffsetX = offsetXNow;
    s_lastSavedMapOffsetY = offsetYNow;
    s_lastMapPersistTime = now;
}
