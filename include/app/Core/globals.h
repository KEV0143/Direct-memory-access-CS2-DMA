#pragma once
#include <cstdint>
#include <string>

#include "app/Core/app_state.h"

namespace g {
    using DisplaySettings = app::state::DisplaySettings;
    using RuntimeState = app::state::RuntimeState;
    using EspSettings = app::state::EspSettings;
    using RadarSettings = app::state::RadarSettings;
    using WebRadarSettings = app::state::WebRadarSettings;
    using UiSettings = app::state::UiSettings;
    using FontState = app::state::FontState;
    using AppState = app::state::AppState;

    inline AppState& state = app::state::globalState;
    inline DisplaySettings& displaySettings = state.display;
    inline RuntimeState& runtimeState = state.runtime;
    inline EspSettings& espSettings = state.esp;
    inline RadarSettings& radarSettings = state.radar;
    inline WebRadarSettings& webRadarSettings = state.webRadar;
    inline UiSettings& uiSettings = state.ui;
    inline FontState& fontState = state.fonts;

    inline int& screenWidth = displaySettings.width;
    inline int& screenHeight = displaySettings.height;
    inline uintptr_t& clientBase = runtimeState.clientBase;
    inline uintptr_t& engine2Base = runtimeState.engine2Base;
    inline bool& running = runtimeState.running;
    inline bool& menuOpen = runtimeState.menuOpen;

    inline bool& espEnabled = espSettings.enabled;
    inline bool& espBox = espSettings.box;
    inline bool& espHealth = espSettings.health;
    inline bool& espHealthText = espSettings.healthText;
    inline bool& espArmor = espSettings.armor;
    inline bool& espArmorText = espSettings.armorText;
    inline bool& espName = espSettings.name;
    inline bool& espWeapon = espSettings.weapon;
    inline bool& espWeaponText = espSettings.weaponText;
    inline float& espWeaponTextSize = espSettings.weaponTextSize;
    inline auto& espWeaponTextColor = espSettings.weaponTextColor;
    inline bool& espWeaponIcon = espSettings.weaponIcon;
    inline bool& espWeaponIconNoKnife = espSettings.weaponIconNoKnife;
    inline float& espWeaponIconSize = espSettings.weaponIconSize;
    inline auto& espWeaponIconColor = espSettings.weaponIconColor;
    inline bool& espWeaponAmmo = espSettings.weaponAmmo;
    inline float& espWeaponAmmoSize = espSettings.weaponAmmoSize;
    inline auto& espWeaponAmmoColor = espSettings.weaponAmmoColor;
    inline bool& espDistance = espSettings.distance;
    inline bool& espFlagBlind = espSettings.flagBlind;
    inline auto& espFlagBlindColor = espSettings.flagBlindColor;
    inline float& espFlagBlindSize = espSettings.flagBlindSize;
    inline bool& espFlagScoped = espSettings.flagScoped;
    inline auto& espFlagScopedColor = espSettings.flagScopedColor;
    inline float& espFlagScopedSize = espSettings.flagScopedSize;
    inline bool& espFlagDefusing = espSettings.flagDefusing;
    inline auto& espFlagDefusingColor = espSettings.flagDefusingColor;
    inline float& espFlagDefusingSize = espSettings.flagDefusingSize;
    inline bool& espFlagKit = espSettings.flagKit;
    inline auto& espFlagKitColor = espSettings.flagKitColor;
    inline float& espFlagKitSize = espSettings.flagKitSize;
    inline bool& espFlagMoney = espSettings.flagMoney;
    inline auto& espFlagMoneyColor = espSettings.flagMoneyColor;
    inline float& espFlagMoneySize = espSettings.flagMoneySize;
    inline float& espDistanceSize = espSettings.distanceSize;
    inline bool& espSkeleton = espSettings.skeleton;
    inline bool& espSkeletonDots = espSettings.skeletonDots;
    inline bool& espSnaplines = espSettings.snaplines;
    inline bool& espSnaplineFromTop = espSettings.snaplineFromTop;
    inline bool& espVisibilityColoring = espSettings.visibilityColoring;
    inline bool& espOffscreenArrows = espSettings.offscreenArrows;
    inline bool& espFlags = espSettings.flags;
    inline bool& espItem = espSettings.item;
    inline bool& espWorld = espSettings.world;
    inline bool& espWorldProjectiles = espSettings.worldProjectiles;
    inline bool& espWorldSmokeTimer = espSettings.worldSmokeTimer;
    inline bool& espWorldInfernoTimer = espSettings.worldInfernoTimer;
    inline bool& espWorldDecoyTimer = espSettings.worldDecoyTimer;
    inline bool& espWorldExplosiveTimer = espSettings.worldExplosiveTimer;
    inline bool& espBombInfo = espSettings.bombInfo;
    inline bool& espSound = espSettings.sound;
    inline auto& espItemEnabledMask = espSettings.itemEnabledMask;
    inline auto& espBoxColor = espSettings.boxColor;
    inline auto& espHealthColor = espSettings.healthColor;
    inline auto& espVisibleColor = espSettings.visibleColor;
    inline auto& espHiddenColor = espSettings.hiddenColor;
    inline auto& espArmorColor = espSettings.armorColor;
    inline auto& espNameColor = espSettings.nameColor;
    inline auto& espDistanceColor = espSettings.distanceColor;
    inline auto& espSkeletonColor = espSettings.skeletonColor;
    inline auto& espSnaplineColor = espSettings.snaplineColor;
    inline auto& espOffscreenColor = espSettings.offscreenColor;
    inline auto& espFlagColor = espSettings.flagColor;
    inline auto& espWorldColor = espSettings.worldColor;
    inline auto& espBombColor = espSettings.bombColor;
    inline float& espBombTextSize = espSettings.bombTextSize;
    inline bool& espBombText = espSettings.bombText;
    inline auto& espSoundColor = espSettings.soundColor;
    inline float& espOffscreenSize = espSettings.offscreenSize;
    inline bool& espPreviewOpen = espSettings.previewOpen;
    inline float& espNameFontSize = espSettings.nameFontSize;

    inline bool& radarEnabled = radarSettings.enabled;
    inline int& radarMode = radarSettings.mode;
    inline bool& radarShowLocalDot = radarSettings.showLocalDot;
    inline bool& radarShowAngles = radarSettings.showAngles;
    inline bool& radarShowCrosshair = radarSettings.showCrosshair;
    inline bool& radarShowBomb = radarSettings.showBomb;
    inline float& radarSize = radarSettings.size;
    inline float& radarDotSize = radarSettings.dotSize;
    inline float& radarWorldRotationDeg = radarSettings.worldRotationDeg;
    inline float& radarWorldScale = radarSettings.worldScale;
    inline float& radarWorldOffsetX = radarSettings.worldOffsetX;
    inline float& radarWorldOffsetY = radarSettings.worldOffsetY;
    inline bool& radarStaticFlipX = radarSettings.staticFlipX;
    inline auto& radarBgColor = radarSettings.bgColor;
    inline auto& radarBorderColor = radarSettings.borderColor;
    inline auto& radarDotColor = radarSettings.dotColor;
    inline auto& radarBombColor = radarSettings.bombColor;
    inline auto& radarAngleColor = radarSettings.angleColor;
    inline bool& radarCalibrationOpen = radarSettings.calibrationOpen;

    inline bool& webRadarEnabled = webRadarSettings.enabled;
    inline int& webRadarIntervalMs = webRadarSettings.intervalMs;
    inline int& webRadarPort = webRadarSettings.port;
    inline std::string& webRadarMapOverride = webRadarSettings.mapOverride;
    inline bool& webRadarQrOpen = webRadarSettings.qrOpen;
    inline bool& webRadarDebugOpen = webRadarSettings.debugOpen;

    inline int& menuToggleKey = uiSettings.menuToggleKey;
    inline bool& vsyncEnabled = displaySettings.vsyncEnabled;
    inline int& fpsLimit = displaySettings.fpsLimit;

    inline ImFont*& fontDefault = fontState.fontDefault;
    inline ImFont*& fontSegoeBold = fontState.fontSegoeBold;
    inline ImFont*& fontWeaponIcons = fontState.fontWeaponIcons;
}
