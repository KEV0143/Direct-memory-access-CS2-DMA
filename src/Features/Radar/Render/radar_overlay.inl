
if (g::radarEnabled) {
    const bool radarStaticMode = (g::radarMode != 0);
    const float staticFlipX = g::radarStaticFlipX ? -1.0f : 1.0f;
    constexpr float kRadarOverviewTextureSize = 1024.0f;
    const float hudScale = screenH / 1080.0f;
    const float cs2MinimapMarginX = 5.0f * hudScale;
    const float cs2MinimapMarginY = 7.0f * hudScale;
    const float cs2MinimapSize = 290.0f * hudScale;

    const float calibrationRad = g::radarWorldRotationDeg * (std::numbers::pi_v<float> / 180.0f);
    const float calibrationCos = cosf(calibrationRad);
    const float calibrationSin = sinf(calibrationRad);
    const float scaleAdjust = std::clamp(g::radarWorldScale, 0.5f, 1.5f);
    const float offsetAdjustX = std::clamp(s_activeMapBaseOffsetX + g::radarWorldOffsetX, -0.25f, 0.25f);
    const float offsetAdjustY = std::clamp(s_activeMapBaseOffsetY + g::radarWorldOffsetY, -0.25f, 0.25f);

    static bool s_legacyAnchoredSizeMigrated = false;
    const float legacyAnchoredSize = 234.0f * hudScale;
    const float oversizedAnchoredSize = 324.0f * hudScale;
    if (!s_legacyAnchoredSizeMigrated &&
        (std::fabs(g::radarSize - legacyAnchoredSize) <= 1.5f ||
         std::fabs(g::radarSize - oversizedAnchoredSize) <= 1.5f)) {
        g::radarSize = cs2MinimapSize;
        s_legacyAnchoredSizeMigrated = true;
    }

    const float radarSz = g::radarSize;
    const float halfRad = radarSz * 0.5f;
    const float screenClampMargin = 4.0f * hudScale;
    float radarPosX = cs2MinimapMarginX;
    float radarPosY = cs2MinimapMarginY;

    if (radarPosX < screenClampMargin) radarPosX = screenClampMargin;
    if (radarPosY < screenClampMargin) radarPosY = screenClampMargin;
    if (radarPosX > screenW - radarSz - screenClampMargin) radarPosX = screenW - radarSz - screenClampMargin;
    if (radarPosY > screenH - radarSz - screenClampMargin) radarPosY = screenH - radarSz - screenClampMargin;
    const ImVec2 radarPos(radarPosX, radarPosY);
    const ImVec2 radarEnd(radarPos.x + radarSz, radarPos.y + radarSz);
    const ImVec2 radarCenter(radarPos.x + halfRad, radarPos.y + halfRad);

    const ImU32 radarDotCol = ColorToImU32(g::radarDotColor);
    const ImU32 radarBombCol = ColorToImU32(g::radarBombColor);
    const ImU32 radarAngleCol = ColorToImU32(g::radarAngleColor);
    const ImU32 radarBorderCol = ColorToImU32(g::radarBorderColor);
    
    drawList->AddRect(radarPos, radarEnd, radarBorderCol);
    if (g::radarShowCrosshair) {
        drawList->AddLine(ImVec2(radarCenter.x, radarPos.y), ImVec2(radarCenter.x, radarEnd.y), IM_COL32(255, 255, 255, 26));
        drawList->AddLine(ImVec2(radarPos.x, radarCenter.y), ImVec2(radarEnd.x, radarCenter.y), IM_COL32(255, 255, 255, 26));
        drawList->AddCircleFilled(radarCenter, 2.8f, IM_COL32(255, 255, 255, 230));
    }

    float localDirRight = 0.0f;
    float localDirForward = 1.0f;
    if (radarStaticMode) {
        const float worldForwardX = cosf(yawRad) * staticFlipX;
        const float worldForwardY = sinf(yawRad);
        localDirRight = worldForwardX * calibrationCos - worldForwardY * calibrationSin;
        localDirForward = worldForwardX * calibrationSin + worldForwardY * calibrationCos;
    } else {
        
        localDirRight = 0.0f;
        localDirForward = 1.0f;
    }
    const float localDirLen = std::sqrt(localDirRight * localDirRight + localDirForward * localDirForward);
    if (localDirLen > 0.0001f) {
        localDirRight /= localDirLen;
        localDirForward /= localDirLen;
    }
    struct KnownRadarMap
    {
        std::string name;
        float runtimeMinX;
        float runtimeMaxX;
        float runtimeMinY;
        float runtimeMaxY;
        float originX;
        float originY;
        float scale;
    };
    auto loadKnownRadarMaps = []() -> std::vector<KnownRadarMap> {
        auto fallbackMaps = []() -> std::vector<KnownRadarMap> {
            return {
                { "de_mirage",   -3230.0f, 1890.0f,  -3407.0f, 1713.0f,  -3230.0f, 1713.0f, 5.00f },
                { "de_inferno",  -2087.0f, 2930.6f,  -1147.6f, 3870.0f,  -2087.0f, 3870.0f, 4.90f },
                { "de_dust2",    -2476.0f, 2029.6f,  -1266.6f, 3239.0f,  -2476.0f, 3239.0f, 4.40f },
                { "de_nuke",     -3453.0f, 3715.0f,  -4281.0f, 2887.0f,  -3453.0f, 2887.0f, 7.00f },
                { "de_overpass", -4831.0f, 493.8f,   -3543.8f, 1781.0f,  -4831.0f, 1781.0f, 5.20f },
                { "de_ancient",  -2953.0f, 2167.0f,  -2956.0f, 2164.0f,  -2953.0f, 2164.0f, 5.00f },
                { "de_anubis",   -2796.0f, 2549.28f, -2017.28f, 3328.0f, -2796.0f, 3328.0f, 5.22f },
                { "de_vertigo",  -3168.0f, 928.0f,   -2334.0f, 1762.0f,  -3168.0f, 1762.0f, 4.00f },
            };
        };

        const auto rawPath = app::paths::ResolveWebRadarAssetPath("maps.json");
        if (!rawPath.empty()) {
            std::error_code ec;
            const auto path = std::filesystem::weakly_canonical(rawPath, ec);
            const auto& candidatePath = ec ? rawPath : path;
            if (std::filesystem::exists(candidatePath)) {
                try {
                    std::ifstream file(candidatePath, std::ios::in | std::ios::binary);
                    if (file.is_open()) {
                        nlohmann::json root = nlohmann::json::parse(file, nullptr, false);
                        if (!root.is_discarded() && root.contains("maps") && root["maps"].is_array()) {
                            std::vector<KnownRadarMap> maps;
                            for (const auto& item : root["maps"]) {
                                if (!item.is_object() || item.value("dynamic", false))
                                    continue;
                                if (!item.contains("name") || !item["name"].is_string())
                                    continue;
                                if (!item.contains("origin") || !item["origin"].is_object())
                                    continue;
                                if (!item.contains("bounds") || !item["bounds"].is_object())
                                    continue;
                                if (!item.contains("scale") || !item["scale"].is_number())
                                    continue;

                                const auto& origin = item["origin"];
                                const auto& bounds = item["bounds"];
                                if (!origin.contains("x") || !origin["x"].is_number() ||
                                    !origin.contains("y") || !origin["y"].is_number() ||
                                    !bounds.contains("min_x") || !bounds["min_x"].is_number() ||
                                    !bounds.contains("max_x") || !bounds["max_x"].is_number() ||
                                    !bounds.contains("min_y") || !bounds["min_y"].is_number() ||
                                    !bounds.contains("max_y") || !bounds["max_y"].is_number()) {
                                    continue;
                                }

                                const float scale = item["scale"].get<float>();
                                if (!std::isfinite(scale) || std::fabs(scale) <= 0.0001f)
                                    continue;

                                maps.push_back(KnownRadarMap{
                                    item["name"].get<std::string>(),
                                    bounds["min_x"].get<float>(),
                                    bounds["max_x"].get<float>(),
                                    bounds["min_y"].get<float>(),
                                    bounds["max_y"].get<float>(),
                                    origin["x"].get<float>(),
                                    origin["y"].get<float>(),
                                    scale,
                                });
                            }

                            if (!maps.empty())
                                return maps;
                        }
                    }
                } catch (...) {
                }
            }
        }

        return fallbackMaps();
    };
    static const std::vector<KnownRadarMap> knownRadarMaps = loadKnownRadarMaps();
    auto resolveKnownRadarMap = [&](const Vector3& mins, const Vector3& maxs) -> const KnownRadarMap* {
        if (!s_activeMapKey.empty()) {
            for (const auto& map : knownRadarMaps) {
                if (_stricmp(map.name.c_str(), s_activeMapKey.c_str()) == 0)
                    return &map;
            }
        }

        if (!hasMinimapBounds)
            return nullptr;

        const float runtimeMinX = mins.x;
        const float runtimeMaxX = maxs.x;
        const float runtimeMinY = mins.y;
        const float runtimeMaxY = maxs.y;
        if (!std::isfinite(runtimeMinX) || !std::isfinite(runtimeMaxX) ||
            !std::isfinite(runtimeMinY) || !std::isfinite(runtimeMaxY)) {
            return nullptr;
        }

        const KnownRadarMap* best = nullptr;
        float bestScore = FLT_MAX;
        for (const auto& map : knownRadarMaps) {
            const float score =
                std::fabs(runtimeMinX - map.runtimeMinX) +
                std::fabs(runtimeMaxX - map.runtimeMaxX) +
                std::fabs(runtimeMinY - map.runtimeMinY) +
                std::fabs(runtimeMaxY - map.runtimeMaxY);
            if (score < bestScore) {
                bestScore = score;
                best = &map;
            }
        }

        return (best != nullptr && bestScore <= 2200.0f) ? best : nullptr;
    };
    const KnownRadarMap* knownRadarMap = resolveKnownRadarMap(minimapMins, minimapMaxs);
    float mapCenterX = 0.0f;
    float mapCenterY = 0.0f;
    float mapUniformHalfSpan = 0.0f;
    bool hasUsableBounds = false;
    constexpr float kStaticFallbackRange = 2000.0f;
    float effectiveRange = kStaticFallbackRange;
    if (hasMinimapBounds) {
        const float spanX = std::fabs(minimapMaxs.x - minimapMins.x);
        const float spanY = std::fabs(minimapMaxs.y - minimapMins.y);
        const float halfSpan = (spanX > spanY ? spanX : spanY) * 0.5f;
        if (halfSpan > 100.0f) {
            hasUsableBounds = true;
            mapCenterX = (minimapMins.x + minimapMaxs.x) * 0.5f;
            mapCenterY = (minimapMins.y + minimapMaxs.y) * 0.5f;
            mapUniformHalfSpan = halfSpan;
            effectiveRange = halfSpan;
        }
    }
    if (radarStaticMode && s_activeMapOverviewAvailable && s_activeMapOverviewScale > 0.01f) {
        const float overviewWorldSpan = s_activeMapOverviewScale * kRadarOverviewTextureSize;
        const float overviewHalfSpan = overviewWorldSpan * 0.5f;
        if (overviewHalfSpan > 100.0f) {
            hasUsableBounds = true;
            mapCenterX = s_activeMapOverviewPosX + overviewHalfSpan;
            mapCenterY = s_activeMapOverviewPosY - overviewHalfSpan;
            mapUniformHalfSpan = overviewHalfSpan;
            effectiveRange = overviewHalfSpan;
        }
    }
    if (effectiveRange < 500.0f)
        effectiveRange = 500.0f;
    const bool useKnownMapProjection = radarStaticMode && knownRadarMap != nullptr;
    const bool useBoundsProjection = radarStaticMode && !useKnownMapProjection && hasUsableBounds;

    auto projectRadarPoint = [&](float worldX, float worldY, float* outX, float* outY) -> bool {
        float px = radarCenter.x;
        float py = radarCenter.y;
        if (useKnownMapProjection) {
            const float normX = (worldX - knownRadarMap->originX) / knownRadarMap->scale / kRadarOverviewTextureSize;
            const float normY = (((worldY - knownRadarMap->originY) / knownRadarMap->scale) * -1.0f) / kRadarOverviewTextureSize;
            const float baseRight = (normX - 0.5f) * 2.0f * staticFlipX;
            const float baseForward = (0.5f - normY) * 2.0f;
            const float calRight = baseRight * calibrationCos - baseForward * calibrationSin;
            const float calForward = baseRight * calibrationSin + baseForward * calibrationCos;
            px = radarCenter.x + (calRight + offsetAdjustX) * (halfRad * scaleAdjust);
            py = radarCenter.y - (calForward + offsetAdjustY) * (halfRad * scaleAdjust);
        } else if (useBoundsProjection) {
            if (mapUniformHalfSpan <= 1.0f)
                return false;
            const float normX = (worldX - mapCenterX) / mapUniformHalfSpan;
            const float normY = (worldY - mapCenterY) / mapUniformHalfSpan;
            const float baseRight = normX * staticFlipX;
            const float baseForward = normY;
            const float calRight = baseRight * calibrationCos - baseForward * calibrationSin;
            const float calForward = baseRight * calibrationSin + baseForward * calibrationCos;
            px = radarCenter.x + (calRight + offsetAdjustX) * (halfRad * scaleAdjust);
            py = radarCenter.y - (calForward + offsetAdjustY) * (halfRad * scaleAdjust);
        } else {
            const float dx = worldX - localPos.x;
            const float dy = worldY - localPos.y;
            float baseForward = 0.0f;
            float baseRight = 0.0f;
            if (radarStaticMode) {
                baseRight = dx * staticFlipX;
                baseForward = dy;
            } else {
                baseForward = dx * yawCos + dy * yawSin;
                
                baseRight = dx * yawSin - dy * yawCos;
            }
            const float calRight = radarStaticMode
                ? (baseRight * calibrationCos - baseForward * calibrationSin)
                : baseRight;
            const float calForward = radarStaticMode
                ? (baseRight * calibrationSin + baseForward * calibrationCos)
                : baseForward;
            const float scale = (halfRad / effectiveRange) * (radarStaticMode ? scaleAdjust : 1.0f);
            px = radarCenter.x + calRight * scale;
            py = radarCenter.y - calForward * scale;
        }
        if (px < radarPos.x + 3.0f) px = radarPos.x + 3.0f;
        if (px > radarEnd.x - 3.0f) px = radarEnd.x - 3.0f;
        if (py < radarPos.y + 3.0f) py = radarPos.y + 3.0f;
        if (py > radarEnd.y - 3.0f) py = radarEnd.y - 3.0f;

        if (outX) *outX = px;
        if (outY) *outY = py;
        return true;
    };
    auto drawDashedRadarCircle = [&](const ImVec2& center, float radius, ImU32 color, float thickness = 1.85f) {
        constexpr int kDashSegments = 32;
        constexpr int kDashLength = 1;
        for (int seg = 0; seg < kDashSegments; seg += 2) {
            const float a0 = (static_cast<float>(seg) / static_cast<float>(kDashSegments)) * (2.0f * std::numbers::pi_v<float>);
            const float a1 = (static_cast<float>(seg + kDashLength) / static_cast<float>(kDashSegments)) * (2.0f * std::numbers::pi_v<float>);
            const ImVec2 p0(center.x + cosf(a0) * radius, center.y + sinf(a0) * radius);
            const ImVec2 p1(center.x + cosf(a1) * radius, center.y + sinf(a1) * radius);
            drawList->AddLine(p0, p1, color, thickness);
        }
    };
    float localMarkerX = radarCenter.x;
    float localMarkerY = radarCenter.y;
    projectRadarPoint(localPos.x, localPos.y, &localMarkerX, &localMarkerY);
    const ImVec2 localOrigin(localMarkerX, localMarkerY);
    
    if (g::radarShowLocalDot) {
        drawList->AddCircleFilled(localOrigin, g::radarDotSize + 1.0f, IM_COL32(255, 255, 255, 240));
        const float localRayLen = 6.0f;
        drawList->AddLine(
            localOrigin,
            ImVec2(localOrigin.x + localDirRight * localRayLen, localOrigin.y - localDirForward * localRayLen),
            IM_COL32(255, 255, 255, 220), 1.2f);
    }

    for (int i = 0; i < 64; ++i) {
        const esp::PlayerData& p = players[i];
        if (!p.valid)
            continue;
        if (snap.localPlayerIndex >= 0 && i == snap.localPlayerIndex)
            continue;
        if (snap.localPawn != 0 && p.pawn == snap.localPawn)
            continue;
        if ((localTeam == 2 || localTeam == 3) && p.team == localTeam)
            continue;
        const Vector3& radarPosPredicted = p.position;

        float px = 0.0f;
        float py = 0.0f;
        if (!projectRadarPoint(radarPosPredicted.x, radarPosPredicted.y, &px, &py))
            continue;

        drawList->AddCircleFilled(ImVec2(px, py), g::radarDotSize, radarDotCol);
        if (g::radarShowBomb && p.hasBomb && !bombState.dropped && !bombState.planted) {
            const float carrierR = g::radarDotSize + 2.4f;
            drawList->AddRect(
                ImVec2(px - carrierR, py - carrierR),
                ImVec2(px + carrierR, py + carrierR),
                radarBombCol,
                1.0f,
                0,
                1.4f);
        }

        if (g::radarShowAngles) {
            const float enemyYawRad = p.eyeYaw * (std::numbers::pi_v<float> / 180.0f);
            float enemyForward = 0.0f;
            float enemyRight = 0.0f;
            if (radarStaticMode) {
                enemyRight = cosf(enemyYawRad) * staticFlipX;
                enemyForward = sinf(enemyYawRad);
            } else {
                const float relYaw = NormalizeYawDeltaRad(enemyYawRad - yawRad);
                enemyForward = cosf(relYaw);
                enemyRight = sinf(relYaw);
            }
            const float calEnemyRight = radarStaticMode
                ? (enemyRight * calibrationCos - enemyForward * calibrationSin)
                : enemyRight;
            const float calEnemyForward = radarStaticMode
                ? (enemyRight * calibrationSin + enemyForward * calibrationCos)
                : enemyForward;
            const float rayLen = 6.0f;
            drawList->AddLine(
                ImVec2(px, py),
                ImVec2(px + calEnemyRight * rayLen, py - calEnemyForward * rayLen),
                radarAngleCol, 1.2f);
        }
    }

    if (g::radarShowBomb) {
        bool bombDrawnOnRadar = false;
        bool bombCarriedByAnyPlayer = localHasBomb;
        if (!bombCarriedByAnyPlayer) {
            for (int i = 0; i < 64; ++i) {
                if (players[i].valid && players[i].hasBomb) {
                    bombCarriedByAnyPlayer = true;
                    break;
                }
            }
        }
        if (!bombCarriedByAnyPlayer && (bombState.planted || bombState.dropped) && isValidWorldPos(bombState.position)) {
            float bpx = 0.0f;
            float bpy = 0.0f;
            if (projectRadarPoint(bombState.position.x, bombState.position.y, &bpx, &bpy)) {
                const bool droppedOnly = bombState.dropped && !bombState.planted;
                const float bombRadius = droppedOnly ? (g::radarDotSize + 1.2f) : (g::radarDotSize + 1.4f);
                if (droppedOnly) {
                    drawList->AddCircleFilled(ImVec2(bpx, bpy), bombRadius, radarBombCol);
                    drawDashedRadarCircle(ImVec2(bpx, bpy), bombRadius + 0.55f, IM_COL32(0, 0, 0, 185), 1.0f);
                } else {
                    drawList->AddCircleFilled(ImVec2(bpx, bpy), bombRadius, radarBombCol);
                    drawList->AddCircle(ImVec2(bpx, bpy), bombRadius + 1.6f, IM_COL32(255, 210, 130, 180), 18, 1.0f);
                }
                bombDrawnOnRadar = true;
            }
        }

        if (!bombDrawnOnRadar && !bombCarriedByAnyPlayer) {
            for (int i = 0; i < worldMarkerCount; ++i) {
                const WorldMarker& marker = worldMarkers[i];
                if (!marker.valid)
                    continue;
                if (marker.expiresUs > 0 && marker.expiresUs < nowUs)
                    continue;
                if (marker.type != WorldMarkerType::DroppedWeapon || marker.weaponId != 49)
                    continue;
                if (!isValidWorldPos(marker.position))
                    continue;

                float bpx = 0.0f;
                float bpy = 0.0f;
                if (!projectRadarPoint(marker.position.x, marker.position.y, &bpx, &bpy))
                    continue;

                const float bombRadius = g::radarDotSize + 1.2f;
                drawList->AddCircleFilled(ImVec2(bpx, bpy), bombRadius, radarBombCol);
                drawDashedRadarCircle(ImVec2(bpx, bpy), bombRadius + 0.55f, IM_COL32(0, 0, 0, 185), 1.0f);
                bombDrawnOnRadar = true;
                break;
            }
        }
    }
}
