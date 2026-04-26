#pragma once

#include <cstdint>

namespace esp::intervals {
    
    constexpr uint64_t kBaseHighestEntityRefreshUs = 100000;  
    constexpr uint64_t kBaseMinimapRefreshUs       = 500000;  
    constexpr uint64_t kBaseSensitivityRefreshUs   = 750000;  
    constexpr uint64_t kBaseIntervalRefreshUs      = 500000;  

    
    constexpr uint64_t kPlayerIdentityAuxUs   = 350000;
    constexpr uint64_t kPlayerMoneyAuxUs      = 125000;
    constexpr uint64_t kPlayerStatusAuxUs     = 40000;
    constexpr uint64_t kPlayerDefuserAuxUs    = 100000;
    constexpr uint64_t kPlayerEyeAuxUs        = 40000;
    constexpr uint64_t kPlayerVisibilityAuxUs = 30000;
    constexpr uint64_t kPlayerSpectatorAuxUs  = 250000;

    constexpr uint64_t kInventoryActiveWeaponLaneUs      = 10000;
    constexpr uint64_t kInventoryFullInventoryLaneUs     = 50000;
    constexpr uint64_t kInventoryWeaponServicesRefreshUs = 120000;

    constexpr uint64_t kBoneReadsUs = 16000;

    constexpr uint64_t kBombStickyDroppedUs = 1800000;
    constexpr uint64_t kBombStickyVisibleUs = 900000;
}
