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

    auto resolveKnownMap = [&](const Vector3& runtimeMins, const Vector3& runtimeMaxs) -> const KnownMapBounds* {
        if (!std::isfinite(runtimeMins.x) || !std::isfinite(runtimeMaxs.x) ||
            !std::isfinite(runtimeMins.y) || !std::isfinite(runtimeMaxs.y)) {
            return nullptr;
        }

        const float runtimeSpanX = std::fabs(runtimeMaxs.x - runtimeMins.x);
        const float runtimeSpanY = std::fabs(runtimeMaxs.y - runtimeMins.y);
        const float runtimeCenterX = (runtimeMins.x + runtimeMaxs.x) * 0.5f;
        const float runtimeCenterY = (runtimeMins.y + runtimeMaxs.y) * 0.5f;

        const KnownMapBounds* best = nullptr;
        float bestLegacyScore = FLT_MAX;
        float bestSpanScore = FLT_MAX;
        float bestCenterScore = FLT_MAX;

        for (const auto& map : kKnownMaps) {
            const float mapSpanX = std::fabs(map.maxX - map.minX);
            const float mapSpanY = std::fabs(map.maxY - map.minY);
            const float mapCenterX = (map.minX + map.maxX) * 0.5f;
            const float mapCenterY = (map.minY + map.maxY) * 0.5f;

            const float legacyScore =
                std::fabs(runtimeMins.x - map.minX) +
                std::fabs(runtimeMaxs.x - map.maxX) +
                std::fabs(runtimeMins.y - map.minY) +
                std::fabs(runtimeMaxs.y - map.maxY);
            const float spanScore =
                std::fabs(runtimeSpanX - mapSpanX) +
                std::fabs(runtimeSpanY - mapSpanY);
            const float centerScore =
                std::fabs(runtimeCenterX - mapCenterX) +
                std::fabs(runtimeCenterY - mapCenterY);

            const bool betterCandidate =
                legacyScore < bestLegacyScore - 0.1f ||
                (std::fabs(legacyScore - bestLegacyScore) <= 0.1f &&
                 (spanScore < bestSpanScore - 0.1f ||
                  (std::fabs(spanScore - bestSpanScore) <= 0.1f &&
                   centerScore < bestCenterScore)));
            if (!betterCandidate)
                continue;

            best = &map;
            bestLegacyScore = legacyScore;
            bestSpanScore = spanScore;
            bestCenterScore = centerScore;
        }

        const bool exactBoundsMatch = bestLegacyScore <= 2200.0f;
        const bool closeSpanMatch = bestSpanScore <= 192.0f && bestCenterScore <= 768.0f;
        return (best != nullptr && (exactBoundsMatch || closeSpanMatch)) ? best : nullptr;
    };

    if (const KnownMapBounds* resolved = resolveKnownMap(mins, maxs))
        return resolved->name;

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
