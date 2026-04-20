if (g::espEnabled && (g::espWorld || g::espItem)) {
    for (int i = 0; i < worldMarkerCount; ++i) {
        const WorldMarker& marker = worldMarkers[i];
        if (!marker.valid)
            continue;
        if (marker.expiresUs > 0 && marker.expiresUs < nowUs)
            continue;
        if (!isFiniteVec(marker.position))
            continue;
        
        if (std::fabs(marker.position.x) < 1.0f && std::fabs(marker.position.y) < 1.0f && std::fabs(marker.position.z) < 1.0f)
            continue;

        const bool isDroppedWeapon = (marker.type == WorldMarkerType::DroppedWeapon);
        const bool isUtilityEffect =
            marker.type == WorldMarkerType::Smoke ||
            marker.type == WorldMarkerType::Inferno ||
            marker.type == WorldMarkerType::Decoy ||
            marker.type == WorldMarkerType::Explosive;
        const bool isUtilityProjectile =
            marker.type == WorldMarkerType::SmokeProjectile ||
            marker.type == WorldMarkerType::MolotovProjectile ||
            marker.type == WorldMarkerType::DecoyProjectile;

        if (isDroppedWeapon) {
            if (marker.weaponId == 0 || marker.weaponId >= 1200)
                continue;
            if (marker.weaponId == 49) {
                bool bombCarriedByAnyPlayer = localHasBomb;
                if (!bombCarriedByAnyPlayer) {
                    for (int pi = 0; pi < 64; ++pi) {
                        if (players[pi].valid && players[pi].hasBomb) {
                            bombCarriedByAnyPlayer = true;
                            break;
                        }
                    }
                }
                if (bombCarriedByAnyPlayer)
                    continue;
                const bool bombEspAlreadyVisible =
                    g::espBombInfo &&
                    (bombState.planted || bombState.dropped) &&
                    isFiniteVec(bombState.position);
                if (bombEspAlreadyVisible)
                    continue;
                if (!g::espBombInfo)
                    continue;
            } else {
                if (!g::espItem)
                    continue;
            }
            if (IsKnifeItemId(marker.weaponId))
                continue;
            if (marker.weaponId != 49 && !g::espItemEnabledMask.test(marker.weaponId))
                continue;
        } else if (isUtilityEffect) {
            if (!g::espWorld)
                continue;
        } else if (isUtilityProjectile) {
            if (!g::espWorld || !g::espWorldProjectiles)
                continue;
        } else {
            continue;
        }

        Vector3 drawPos = marker.position;
        drawPos.z += 8.0f;
        const ScreenPos markerScreen = WorldToScreen(drawPos, viewMatrix, screenW, screenH);
        if (!markerScreen.onScreen)
            continue;

        const char* markerName = WorldMarkerName(marker.type, marker.weaponId);
        if (!markerName || markerName[0] == '\0')
            continue;

        ImU32 markerColor = worldCol;
        
        if (marker.type == WorldMarkerType::Smoke)
            markerColor = IM_COL32(180, 180, 220, 255);
        else if (marker.type == WorldMarkerType::Inferno)
            markerColor = IM_COL32(255, 120, 40, 255);
        else if (marker.type == WorldMarkerType::Decoy)
            markerColor = IM_COL32(200, 200, 80, 255);
        else if (marker.type == WorldMarkerType::Explosive)
            markerColor = IM_COL32(255, 80, 80, 255);
        else if (marker.type == WorldMarkerType::SmokeProjectile)
            markerColor = IM_COL32(180, 180, 220, 255);
        else if (marker.type == WorldMarkerType::MolotovProjectile)
            markerColor = IM_COL32(255, 140, 60, 255);
        else if (marker.type == WorldMarkerType::DecoyProjectile)
            markerColor = IM_COL32(210, 210, 110, 255);

        const float lifeLeft = (marker.expiresUs > nowUs)
            ? static_cast<float>(marker.expiresUs - nowUs) / 1000000.0f
            : 0.0f;
        const bool showTimer =
            (marker.type == WorldMarkerType::Smoke && g::espWorldSmokeTimer) ||
            (marker.type == WorldMarkerType::Inferno && g::espWorldInfernoTimer) ||
            (marker.type == WorldMarkerType::Decoy && g::espWorldDecoyTimer) ||
            (marker.type == WorldMarkerType::Explosive && g::espWorldExplosiveTimer);
        char markerText[96] = {};
        if (showTimer && lifeLeft > 0.0f)
            std::snprintf(markerText, sizeof(markerText), "%s %.1fs", markerName, static_cast<double>(lifeLeft));
        else
            std::snprintf(markerText, sizeof(markerText), "%s", markerName);

        const char* icon = nullptr;
        ImFont* iconFont = g::fontWeaponIcons;
        const char* iconFallbackToken = nullptr;
        if (isDroppedWeapon || isUtilityProjectile) {
            
            
            const char* iconCandidate = WeaponIconFromItemId(marker.weaponId);
            if (WeaponNameFromItemId(marker.weaponId)) {
                icon = iconCandidate;
                if (!icon && WeaponVisualKeyFromItemId(marker.weaponId))
                    iconFallbackToken = WeaponIconFallbackTokenFromItemId(marker.weaponId);
            }
        }
        if ((icon && g::fontWeaponIcons) || iconFallbackToken) {
            const char* iconText = icon ? icon : iconFallbackToken;
            if (!icon)
                iconFont = g::fontSegoeBold ? g::fontSegoeBold : ImGui::GetFont();
            const float iconSize = icon ? 16.0f : 13.5f;
            const ImVec2 iconBounds = iconFont->CalcTextSizeA(iconSize, FLT_MAX, 0.0f, iconText, nullptr);
            const float iconX = markerScreen.x - iconBounds.x * 0.5f;
            const float iconY = markerScreen.y - iconBounds.y - 8.0f;
            DrawTextShadow(drawList, iconFont, iconSize, ImVec2(iconX, iconY), IM_COL32(240, 240, 240, 240), iconText);
        }

        const ImVec2 textSize = ImGui::CalcTextSize(markerText);
        DrawTextShadow(
            drawList,
            nullptr,
            0.0f,
            ImVec2(markerScreen.x - textSize.x * 0.5f, markerScreen.y - textSize.y * 0.5f),
            markerColor,
            markerText);
    }
}
