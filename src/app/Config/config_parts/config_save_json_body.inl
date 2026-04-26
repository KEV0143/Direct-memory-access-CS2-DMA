        json root = json::object();

        json& esp = EnsureSection(root, "ESP");
        esp["Enabled"] = g::espEnabled;
        esp["Box"] = g::espBox;
        esp["Health"] = g::espHealth;
        esp["HealthText"] = g::espHealthText;
        esp["Armor"] = g::espArmor;
        esp["ArmorText"] = g::espArmorText;
        esp["Name"] = g::espName;
        esp["NameFontSize"] = g::espNameFontSize;
        esp["Weapon"] = g::espWeapon;
        esp["WeaponText"] = g::espWeaponText;
        esp["WeaponTextSize"] = g::espWeaponTextSize;
        SaveColor(esp, "WeaponTextColor", g::espWeaponTextColor);
        esp["WeaponIcon"] = g::espWeaponIcon;
        esp["WeaponIconNoKnife"] = g::espWeaponIconNoKnife;
        esp["WeaponIconSize"] = g::espWeaponIconSize;
        SaveColor(esp, "WeaponIconColor", g::espWeaponIconColor);
        esp["WeaponAmmo"] = g::espWeaponAmmo;
        esp["WeaponAmmoSize"] = g::espWeaponAmmoSize;
        SaveColor(esp, "WeaponAmmoColor", g::espWeaponAmmoColor);
        esp["Distance"] = g::espDistance;
        esp["DistanceSize"] = g::espDistanceSize;
        esp["Skeleton"] = g::espSkeleton;
        esp["SkeletonDots"] = g::espSkeletonDots;
        esp["Snaplines"] = g::espSnaplines;
        esp["SnapFromTop"] = g::espSnaplineFromTop;
        esp["VisibilityColoring"] = g::espVisibilityColoring;
        esp["OffscreenArrows"] = g::espOffscreenArrows;
        esp["Sound"] = g::espSound;
        esp["Flags"] = g::espFlags;
        esp["Item"] = g::espItem;
        esp["FlagBlind"] = g::espFlagBlind;
        SaveColor(esp, "FlagBlindColor", g::espFlagBlindColor);
        esp["FlagBlindSize"] = g::espFlagBlindSize;
        esp["FlagScoped"] = g::espFlagScoped;
        SaveColor(esp, "FlagScopedColor", g::espFlagScopedColor);
        esp["FlagScopedSize"] = g::espFlagScopedSize;
        esp["FlagDefusing"] = g::espFlagDefusing;
        SaveColor(esp, "FlagDefusingColor", g::espFlagDefusingColor);
        esp["FlagDefusingSize"] = g::espFlagDefusingSize;
        esp["FlagKit"] = g::espFlagKit;
        SaveColor(esp, "FlagKitColor", g::espFlagKitColor);
        esp["FlagKitSize"] = g::espFlagKitSize;
        esp["FlagMoney"] = g::espFlagMoney;
        SaveColor(esp, "FlagMoneyColor", g::espFlagMoneyColor);
        esp["FlagMoneySize"] = g::espFlagMoneySize;
        esp["World"] = g::espWorld;
        esp["WorldProjectiles"] = g::espWorldProjectiles;
        esp["WorldSmokeTimer"] = g::espWorldSmokeTimer;
        esp["WorldInfernoTimer"] = g::espWorldInfernoTimer;
        esp["WorldDecoyTimer"] = g::espWorldDecoyTimer;
        esp["WorldExplosiveTimer"] = g::espWorldExplosiveTimer;
        esp["BombInfo"] = g::espBombInfo;
        esp["BombText"] = g::espBombText;
        esp["BombTime"] = g::espBombTime;
        esp["BombTextSize"] = g::espBombTextSize;
        esp["BombTimerX"] = g::espBombTimerX;
        esp["BombTimerY"] = g::espBombTimerY;
        esp["OffscreenSize"] = g::espOffscreenSize;
        SaveColor(esp, "BoxColor", g::espBoxColor);
        SaveColor(esp, "HealthColor", g::espHealthColor);
        SaveColor(esp, "VisibleColor", g::espVisibleColor);
        SaveColor(esp, "HiddenColor", g::espHiddenColor);
        SaveColor(esp, "ArmorColor", g::espArmorColor);
        SaveColor(esp, "NameColor", g::espNameColor);
        SaveColor(esp, "DistanceColor", g::espDistanceColor);
        SaveColor(esp, "SkeletonColor", g::espSkeletonColor);
        SaveColor(esp, "SnaplineColor", g::espSnaplineColor);
        SaveColor(esp, "OffscreenColor", g::espOffscreenColor);
        SaveColor(esp, "FlagColor", g::espFlagColor);
        SaveColor(esp, "WorldColor", g::espWorldColor);
        SaveColor(esp, "BombColor", g::espBombColor);
        SaveColor(esp, "SoundColor", g::espSoundColor);
        {
            json hiddenIds = json::array();
            for (size_t itemId = 1; itemId < 1200; ++itemId) {
                if (!g::espItemEnabledMask.test(itemId))
                    hiddenIds.push_back(static_cast<int>(itemId));
            }
            esp["ItemHiddenIds"] = std::move(hiddenIds);
        }

        json& radar = EnsureSection(root, "Radar");
        radar["Enabled"] = g::radarEnabled;
        radar["Mode"] = g::radarMode;
        radar["ShowLocalDot"] = g::radarShowLocalDot;
        radar["ShowAngles"] = g::radarShowAngles;
        radar["ShowCrosshair"] = g::radarShowCrosshair;
        radar["ShowBomb"] = g::radarShowBomb;
        radar["Size"] = g::radarSize;
        radar["DotSize"] = g::radarDotSize;
        radar["WorldRotationDeg"] = g::radarWorldRotationDeg;
        radar["WorldScale"] = g::radarWorldScale;
        radar["WorldOffsetX"] = g::radarWorldOffsetX;
        radar["WorldOffsetY"] = g::radarWorldOffsetY;
        radar["StaticFlipX"] = g::radarStaticFlipX;
        SaveColor(radar, "BgColor", g::radarBgColor);
        SaveColor(radar, "BorderColor", g::radarBorderColor);
        SaveColor(radar, "DotColor", g::radarDotColor);
        SaveColor(radar, "BombColor", g::radarBombColor);
        SaveColor(radar, "AngleColor", g::radarAngleColor);
        radar["SpectatorList"] = g::radarSpectatorList;
        radar["SpectatorListX"] = g::radarSpectatorListX;
        radar["SpectatorListY"] = g::radarSpectatorListY;

        json& webRadar = EnsureSection(root, "WEBRadar");
        webRadar["Enabled"] = g::webRadarEnabled;
        webRadar["Port"] = g::webRadarPort;
        webRadar["MapOverride"] = g::webRadarMapOverride;

        json& screen = EnsureSection(root, "Screen");
        screen["VSync"] = g::vsyncEnabled;
        screen["FPSLimit"] = g::fpsLimit;

        json& ui = EnsureSection(root, "UI");
        ui["EspPreviewOpen"] = g::espPreviewOpen;
        ui["RadarCalibrationOpen"] = g::radarCalibrationOpen;
        ui["WebRadarQrOpen"] = g::webRadarQrOpen;
        ui["WebRadarDebugOpen"] = g::webRadarDebugOpen;
        ui["MenuToggleKey"] = g::menuToggleKey;

        std::filesystem::path path(jsonPath);
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file.is_open())
            return false;

        file << root.dump(4);
        return file.good();
