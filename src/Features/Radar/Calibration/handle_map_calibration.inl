static void HandleMapCalibration(const Vector3& mins, const Vector3& maxs, std::string_view liveMapKey = {})
{
    if (!IsBoundsValid(mins, maxs))
        return;

    std::string mapKey = radar::NormalizeMapName(liveMapKey);
    if (mapKey.empty())
        mapKey = BuildMapKeyFromBounds(mins, maxs);
    if (mapKey.empty())
        return;

    std::string currentMapKey;
    {
        std::lock_guard<std::mutex> lock(s_activeMapMutex);
        currentMapKey = s_activeMapKey;
    }

    if (mapKey != currentMapKey) {
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
        const bool loaded = LoadRadarCalibrationForMap(
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
            &loadedOverviewScale);

        if (loaded) {
            g::radarWorldRotationDeg = loadedRotation;
            g::radarWorldScale = loadedScale;
            g::radarWorldOffsetX = loadedOffsetX;
            g::radarWorldOffsetY = loadedOffsetY;
        } else {
            SaveRadarCalibrationForMap(
                mapKey,
                g::radarWorldRotationDeg,
                g::radarWorldScale,
                0.0f,
                0.0f,
                g::radarWorldOffsetX,
                g::radarWorldOffsetY);
        }

        {
            std::lock_guard<std::mutex> lock(s_activeMapMutex);
            s_activeMapKey = mapKey;
            s_activeMapBaseOffsetX = loaded ? loadedBaseOffsetX : 0.0f;
            s_activeMapBaseOffsetY = loaded ? loadedBaseOffsetY : 0.0f;
            s_activeMapOverviewAvailable = loaded && loadedHasOverview;
            s_activeMapOverviewPosX = loaded ? loadedOverviewPosX : 0.0f;
            s_activeMapOverviewPosY = loaded ? loadedOverviewPosY : 0.0f;
            s_activeMapOverviewScale = loaded ? loadedOverviewScale : 0.0f;
            s_lastSavedMapRotation = g::radarWorldRotationDeg;
            s_lastSavedMapScale = g::radarWorldScale;
            s_lastSavedMapOffsetX = g::radarWorldOffsetX;
            s_lastSavedMapOffsetY = g::radarWorldOffsetY;
            s_lastMapPersistTime = std::chrono::steady_clock::now();
        }
        return;
    }

    const float rotNow = std::clamp(g::radarWorldRotationDeg, -180.0f, 180.0f);
    const float scaleNow = std::clamp(g::radarWorldScale, 0.50f, 1.50f);
    const float offsetXNow = std::clamp(g::radarWorldOffsetX, -0.25f, 0.25f);
    const float offsetYNow = std::clamp(g::radarWorldOffsetY, -0.25f, 0.25f);

    std::string activeMapKey;
    float baseOffsetX = 0.0f;
    float baseOffsetY = 0.0f;
    float lastSavedRotation = 0.0f;
    float lastSavedScale = 1.0f;
    float lastSavedOffsetX = 0.0f;
    float lastSavedOffsetY = 0.0f;
    std::chrono::steady_clock::time_point lastPersistTime;
    {
        std::lock_guard<std::mutex> lock(s_activeMapMutex);
        activeMapKey = s_activeMapKey;
        baseOffsetX = s_activeMapBaseOffsetX;
        baseOffsetY = s_activeMapBaseOffsetY;
        lastSavedRotation = s_lastSavedMapRotation;
        lastSavedScale = s_lastSavedMapScale;
        lastSavedOffsetX = s_lastSavedMapOffsetX;
        lastSavedOffsetY = s_lastSavedMapOffsetY;
        lastPersistTime = s_lastMapPersistTime;
    }
    if (activeMapKey.empty())
        return;

    const bool changed =
        std::fabs(rotNow - lastSavedRotation) > 0.01f ||
        std::fabs(scaleNow - lastSavedScale) > 0.001f ||
        std::fabs(offsetXNow - lastSavedOffsetX) > 0.0005f ||
        std::fabs(offsetYNow - lastSavedOffsetY) > 0.0005f;
    if (!changed)
        return;

    const auto now = std::chrono::steady_clock::now();
    if (now - lastPersistTime < std::chrono::milliseconds(1200))
        return;

    SaveRadarCalibrationForMap(
        activeMapKey,
        rotNow,
        scaleNow,
        baseOffsetX,
        baseOffsetY,
        offsetXNow,
        offsetYNow);

    {
        std::lock_guard<std::mutex> lock(s_activeMapMutex);
        if (s_activeMapKey != activeMapKey)
            return;
        s_lastSavedMapRotation = rotNow;
        s_lastSavedMapScale = scaleNow;
        s_lastSavedMapOffsetX = offsetXNow;
        s_lastSavedMapOffsetY = offsetYNow;
        s_lastMapPersistTime = now;
    }
}
