static std::string BuildMapKeyFromBounds(const Vector3& mins, const Vector3& maxs)
{
    struct KnownMapBounds
    {
        const char* name;
        float minX;
        float maxX;
        float minY;
        float maxY;
    };

    static const KnownMapBounds kKnownMaps[] = {
        { "de_mirage",   -3230.0f, 1890.0f,   -3407.0f, 1713.0f },
        { "de_inferno",  -2087.0f, 2930.6f,   -1147.6f, 3870.0f },
        { "de_dust2",    -2476.0f, 2029.6f,   -1266.6f, 3239.0f },
        { "de_nuke",     -3453.0f, 3715.0f,   -4281.0f, 2887.0f },
        { "de_overpass", -4831.0f, 493.8f,    -3543.8f, 1781.0f },
        { "de_ancient",  -2953.0f, 2167.0f,   -2956.0f, 2164.0f },
        { "de_anubis",   -2796.0f, 2549.28f,  -2017.28f, 3328.0f },
        { "de_vertigo",  -3168.0f, 928.0f,    -2334.0f, 1762.0f },
    };

    if (std::isfinite(mins.x) && std::isfinite(maxs.x) &&
        std::isfinite(mins.y) && std::isfinite(maxs.y)) {
        const KnownMapBounds* best = nullptr;
        float bestScore = FLT_MAX;
        for (const auto& map : kKnownMaps) {
            const float score =
                std::fabs(mins.x - map.minX) +
                std::fabs(maxs.x - map.maxX) +
                std::fabs(mins.y - map.minY) +
                std::fabs(maxs.y - map.maxY);
            if (score < bestScore) {
                bestScore = score;
                best = &map;
            }
        }

        if (best != nullptr && bestScore <= 2200.0f)
            return best->name;
    }

    auto quantize = [](float value) -> int {
        constexpr float kStep = 16.0f;
        return static_cast<int>(std::lround(value / kStep) * kStep);
    };

    return std::format("dynamic_{}_{}__{}_{}",
        quantize(mins.x),
        quantize(mins.y),
        quantize(maxs.x),
        quantize(maxs.y));
}
