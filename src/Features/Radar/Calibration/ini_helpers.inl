static bool HasIniKey(const char* section, const char* key, const std::filesystem::path& path)
{
    char sentinel[8] = {};
    GetPrivateProfileStringA(section, key, "", sentinel, static_cast<DWORD>(sizeof(sentinel)), path.string().c_str());
    return sentinel[0] != '\0';
}

static float ReadIniFloat(const char* section, const char* key, float fallback, const std::filesystem::path& path)
{
    const auto defStr = std::format("{:.4f}", fallback);
    char valBuf[32] = {};
    GetPrivateProfileStringA(section, key, defStr.c_str(), valBuf, static_cast<DWORD>(sizeof(valBuf)), path.string().c_str());
    char* end = nullptr;
    float val = std::strtof(valBuf, &end);
    return (end != valBuf) ? val : fallback;
}

static bool IsBoundsValid(const Vector3& mins, const Vector3& maxs)
{
    const float spanX = std::fabs(maxs.x - mins.x);
    const float spanY = std::fabs(maxs.y - mins.y);
    return spanX > 100.0f && spanY > 100.0f;
}
