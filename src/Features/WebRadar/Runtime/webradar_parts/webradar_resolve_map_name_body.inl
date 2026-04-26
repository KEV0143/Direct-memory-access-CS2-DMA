    const std::string resolvedMapKey = NormalizeMapName(snapshot.mapKey);
    if (!resolvedMapKey.empty())
        return resolvedMapKey;

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

    if (const auto* map = radar::ResolveMapByBounds(
            runtimeMinX,
            runtimeMinY,
            runtimeMaxX,
            runtimeMaxY,
            false)) {
        return map->name;
    }

    return "unknown";
