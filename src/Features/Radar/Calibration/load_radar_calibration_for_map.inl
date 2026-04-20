static bool LoadRadarCalibrationForMap(
    const std::string& mapKey,
    float* outRotationDeg,
    float* outScale,
    float* outBaseOffsetX,
    float* outBaseOffsetY,
    float* outOffsetX,
    float* outOffsetY,
    bool* outHasOverview,
    float* outOverviewPosX,
    float* outOverviewPosY,
    float* outOverviewScale)
{
    if (mapKey.empty() || !outRotationDeg || !outScale ||
        !outBaseOffsetX || !outBaseOffsetY || !outOffsetX || !outOffsetY ||
        !outHasOverview || !outOverviewPosX || !outOverviewPosY || !outOverviewScale)
        return false;

    MigrateLegacyRadarProfilesToUserStateIfNeeded();

    app::user_state::RadarCalibrationRecord record = {};
    if (!app::user_state::LoadRadarCalibration(mapKey, &record))
        return false;

    *outRotationDeg = record.rotationDeg;
    *outScale = record.scale;
    *outBaseOffsetX = record.baseOffsetX;
    *outBaseOffsetY = record.baseOffsetY;
    *outOffsetX = record.offsetX;
    *outOffsetY = record.offsetY;
    *outHasOverview = record.hasOverview;
    *outOverviewPosX = record.overviewPosX;
    *outOverviewPosY = record.overviewPosY;
    *outOverviewScale = record.overviewScale;
    return true;
}
