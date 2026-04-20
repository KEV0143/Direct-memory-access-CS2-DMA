static void SaveRadarCalibrationForMap(
    const std::string& mapKey,
    float rotationDeg,
    float scale,
    float baseOffsetX,
    float baseOffsetY,
    float offsetX,
    float offsetY)
{
    if (mapKey.empty())
        return;

    MigrateLegacyRadarProfilesToUserStateIfNeeded();

    const float clampedRotation = std::clamp(rotationDeg, -180.0f, 180.0f);
    const float clampedScale = std::clamp(scale, 0.50f, 1.50f);
    const float clampedBaseOffsetX = std::clamp(baseOffsetX, -0.25f, 0.25f);
    const float clampedBaseOffsetY = std::clamp(baseOffsetY, -0.25f, 0.25f);
    const float clampedOffsetX = std::clamp(offsetX, -0.25f, 0.25f);
    const float clampedOffsetY = std::clamp(offsetY, -0.25f, 0.25f);

    app::user_state::RadarCalibrationRecord record = {};
    app::user_state::LoadRadarCalibration(mapKey, &record);
    record.rotationDeg = clampedRotation;
    record.scale = clampedScale;
    record.baseOffsetX = clampedBaseOffsetX;
    record.baseOffsetY = clampedBaseOffsetY;
    record.offsetX = clampedOffsetX;
    record.offsetY = clampedOffsetY;
    app::user_state::SaveRadarCalibration(mapKey, record);
}
