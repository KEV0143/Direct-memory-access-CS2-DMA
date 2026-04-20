    if (!snapshot.hasMinimapBounds)
        return "unknown";

    const double runtimeMinX = static_cast<double>(snapshot.minimapMins.x);
    const double runtimeMaxX = static_cast<double>(snapshot.minimapMaxs.x);
    const double runtimeMinY = static_cast<double>(snapshot.minimapMins.y);
    const double runtimeMaxY = static_cast<double>(snapshot.minimapMaxs.y);

    if (!std::isfinite(runtimeMinX) || !std::isfinite(runtimeMaxX) ||
        !std::isfinite(runtimeMinY) || !std::isfinite(runtimeMaxY)) {
        return "unknown";
    }

    const MapDefinition* best = nullptr;
    double bestScore = 1e18;

    for (const auto& map : GetMapDefinitions()) {
        if (map.dynamic)
            continue;

        const double score =
            std::fabs(runtimeMinX - map.minX) +
            std::fabs(runtimeMaxX - map.maxX) +
            std::fabs(runtimeMinY - map.minY) +
            std::fabs(runtimeMaxY - map.maxY);

        if (score < bestScore) {
            bestScore = score;
            best = &map;
        }
    }

    return (best != nullptr && bestScore <= 2200.0) ? best->name : "unknown";
