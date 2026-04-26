
if (g::espWeapon) {
    bottomTextY += 2.0f;
    
    
    
    const uint16_t weaponIconId = p.weaponIconId;
    const char* weaponIcon = WeaponIconFromItemId(weaponIconId);
    const bool hasWeaponVisualAsset = WeaponVisualKeyFromItemId(weaponIconId) != nullptr;
    const char* weaponIconFallback = WeaponIconFallbackTokenFromItemId(weaponIconId);
    const char* weaponName = WeaponNameFromItemId(p.weaponId);
    if (g::espWeaponIcon && weaponName) {
        if (weaponIcon && g::fontWeaponIcons) {
            drawBottomLabel(weaponIcon, ColorToImU32(g::espWeaponIconColor), false, true, g::fontWeaponIcons, g::espWeaponIconSize);
        } else if (hasWeaponVisualAsset && weaponIconFallback) {
            drawBottomLabel(weaponIconFallback, ColorToImU32(g::espWeaponIconColor), false, true, g::fontSegoeBold, g::espWeaponIconSize - 1.0f);
        }
    }
    if (g::espWeaponText)
        drawBottomLabel(weaponName, ColorToImU32(g::espWeaponTextColor), false, true, g::fontComicSans, g::espWeaponTextSize);
    if (g::espBombInfo && g::espBombText && p.hasBomb && !bombState.dropped && !bombState.planted)
        drawBottomLabel("Bomb", bombCol, false, true, nullptr, g::espBombTextSize);
}
