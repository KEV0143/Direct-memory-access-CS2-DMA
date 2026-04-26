#pragma once

#include <bitset>
#include <cstdint>
#include <string>

struct ImFont;

namespace app::state {
    inline bool IsKnifeItemId(uint16_t id)
    {
        switch (id) {
        case 42:
        case 59:
            return true;
        default:
            break;
        }
        return id >= 500 && id <= 526;
    }

    inline std::bitset<1200> CreateDefaultItemEspMask()
    {
        std::bitset<1200> mask;
        mask.set();
        mask.reset(0);
        mask.reset(42);
        mask.reset(59);
        for (uint16_t id = 500; id <= 526; ++id)
            mask.reset(id);
        return mask;
    }

    struct DisplaySettings {
        int width = 1920;
        int height = 1080;
        bool vsyncEnabled = true;
        int fpsLimit = 0;
    };

    struct RuntimeState {
        uintptr_t clientBase = 0;
        uintptr_t engine2Base = 0;
        bool running = true;
        bool menuOpen = true;
    };

    struct EspSettings {
        bool enabled = true;
        bool box = true;
        bool health = true;
        bool healthText = true;
        bool armor = false;
        bool armorText = true;
        bool name = false;
        bool weapon = true;
        bool weaponText = false;
        float weaponTextSize = 0.0f;
        float weaponTextColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        bool weaponIcon = true;
        bool weaponIconNoKnife = false;
        float weaponIconSize = 10.0f;
        float weaponIconColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        bool weaponAmmo = false;
        float weaponAmmoSize = 0.0f;
        float weaponAmmoColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        bool distance = false;
        bool flagBlind = true;
        float flagBlindColor[4] = { 0.80f, 0.92f, 1.0f, 1.0f };
        float flagBlindSize = 0.0f;
        bool flagScoped = true;
        float flagScopedColor[4] = { 0.80f, 0.92f, 1.0f, 1.0f };
        float flagScopedSize = 0.0f;
        bool flagDefusing = true;
        float flagDefusingColor[4] = { 0.80f, 0.92f, 1.0f, 1.0f };
        float flagDefusingSize = 0.0f;
        bool flagKit = true;
        float flagKitColor[4] = { 0.80f, 0.92f, 1.0f, 1.0f };
        float flagKitSize = 0.0f;
        bool flagMoney = false;
        float flagMoneyColor[4] = { 0.80f, 0.92f, 1.0f, 1.0f };
        float flagMoneySize = 0.0f;
        float distanceSize = 0.0f;
        bool skeleton = true;
        bool skeletonDots = false;
        bool snaplines = false;
        bool snaplineFromTop = false;
        bool visibilityColoring = true;
        bool offscreenArrows = false;
        bool flags = true;
        bool item = false;
        bool world = false;
        bool worldProjectiles = false;
        bool worldSmokeTimer = true;
        bool worldInfernoTimer = true;
        bool worldDecoyTimer = true;
        bool worldExplosiveTimer = true;
        bool bombInfo = true;
        bool sound = false;
        std::bitset<1200> itemEnabledMask = CreateDefaultItemEspMask();
        float boxColor[4] = { 1.0f, 0.20f, 0.20f, 1.0f };
        float healthColor[4] = { 0.25f, 0.95f, 0.35f, 1.0f };
        float visibleColor[4] = { 1.0f, 0.20f, 0.20f, 1.0f };
        float hiddenColor[4] = { 0.65f, 0.65f, 0.65f, 1.0f };
        float armorColor[4] = { 0.35f, 0.65f, 1.0f, 1.0f };
        float nameColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        float distanceColor[4] = { 0.80f, 0.92f, 1.0f, 1.0f };
        float skeletonColor[4] = { 0.0f, 1.0f, 0.5f, 1.0f };
        float snaplineColor[4] = { 1.0f, 1.0f, 0.0f, 0.80f };
        float offscreenColor[4] = { 1.0f, 0.30f, 0.20f, 1.0f };
        float flagColor[4] = { 0.80f, 0.92f, 1.0f, 1.0f };
        float worldColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        float bombColor[4] = { 1.0f, 0.65f, 0.20f, 1.0f };
        float bombTextSize = 0.0f;
        bool bombText = false;
        bool bombTime = false;
        float bombTimerX = 780.0f;
        float bombTimerY = 86.0f;
        float soundColor[4] = { 0.35f, 0.75f, 1.0f, 1.0f };
        float offscreenSize = 14.0f;
        bool previewOpen = false;
        float nameFontSize = 16.0f;
    };

    struct RadarSettings {
        bool enabled = true;
        int mode = 1;
        bool showLocalDot = false;
        bool showAngles = true;
        bool showCrosshair = false;
        bool showBomb = true;
        float size = 290.0f;
        float dotSize = 4.0f;
        float worldRotationDeg = 0.0f;
        float worldScale = 1.0f;
        float worldOffsetX = 0.0f;
        float worldOffsetY = 0.0f;
        bool staticFlipX = false;
        float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.75f };
        float borderColor[4] = { 0.0f, 0.0f, 0.0f, 0.35f };
        float dotColor[4] = { 1.0f, 0.13207549f, 0.13207549f, 1.0f };
        float bombColor[4] = { 1.0f, 0.74716979f, 0.0f, 1.0f };
        float angleColor[4] = { 1.0f, 1.0f, 1.0f, 0.95f };
        bool calibrationOpen = false;
        bool spectatorList = false;
        float spectatorListX = 24.0f;
        float spectatorListY = 300.0f;
    };

    struct WebRadarSettings {
        bool enabled = true;
        int intervalMs = 4;
        int port = 22006;
        std::string mapOverride;
        bool qrOpen = true;
        bool debugOpen = false;
    };

    struct UiSettings {
        int menuToggleKey = 'P';
    };

    struct FontState {
        ImFont* fontDefault = nullptr;
        ImFont* fontSegoeBold = nullptr;
        ImFont* fontComicSans = nullptr;
        ImFont* fontWeaponIcons = nullptr;
    };

    struct EspUiIconState {
        uintptr_t enableEsp = 0;
        uintptr_t espPreview = 0;
        uintptr_t cornerBox = 0;
        uintptr_t healthBar = 0;
        uintptr_t armorBar = 0;
        uintptr_t visibilityColors = 0;
        uintptr_t weaponLabel = 0;
        uintptr_t skeleton = 0;
        uintptr_t snapLines = 0;
        uintptr_t playerFlags = 0;
        uintptr_t worldEsp = 0;
        uintptr_t bombEsp = 0;
    };

    struct AppState {
        DisplaySettings display = {};
        RuntimeState runtime = {};
        EspSettings esp = {};
        RadarSettings radar = {};
        WebRadarSettings webRadar = {};
        UiSettings ui = {};
        FontState fonts = {};
        EspUiIconState espUiIcons = {};
    };

    inline AppState globalState = {};
}
