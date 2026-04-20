if (g::espWeapon && g::espWeaponAmmo && p.weaponId != 0 && p.ammoClip >= 0) {
    const bool grenadeLike = (p.weaponId >= 43 && p.weaponId <= 48);
    if (grenadeLike) {
        drawBottomLabel("GRENADE", IM_COL32(220, 220, 220, 245), false, true, nullptr, 0.0f);
    } else {
        const int maxClip = std::max(1, WeaponMaxClipFromItemId(p.weaponId));
        const int clip = std::clamp(p.ammoClip, 0, maxClip);
        const bool lowAmmo = clip <= std::max(1, maxClip / 5);
        char ammoText[32] = {};
        std::snprintf(ammoText, sizeof(ammoText), "Ammo %d/%d", clip, maxClip);
        drawBottomLabel(
            ammoText,
            lowAmmo ? IM_COL32(255, 120, 105, 245) : ColorToImU32(g::espWeaponAmmoColor),
            false,
            true,
            nullptr,
            g::espWeaponAmmoSize);
        if (lowAmmo)
            drawBottomLabel("LOW", IM_COL32(255, 85, 70, 245), false, true, nullptr, 0.0f);
    }
}
