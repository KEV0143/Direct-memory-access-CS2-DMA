    static Vector3 s_lastDroppedBombPos = { NAN, NAN, NAN };
    static uint64_t s_lastDroppedBombPosUs = 0;
    static Vector3 s_lastVisibleBombPos = { NAN, NAN, NAN };
    static Vector3 s_lastVisibleBombBoundsMins = {};
    static Vector3 s_lastVisibleBombBoundsMaxs = {};
    static bool s_lastVisibleBombBoundsValid = false;
    static uint64_t s_lastVisibleBombPosUs = 0;
    static Vector3 s_lastWorldC4Pos = { NAN, NAN, NAN };
    static uint64_t s_lastWorldC4PosUs = 0;
    static bool s_lastWorldC4NoOwner = false;
    static int s_lastWorldC4OwnerIdx = -1;
    static bool s_lastWorldC4OwnerAlive = false;
    static bool s_lastWorldC4OwnerNearby = false;
    static int s_cachedBombCarryOwnerSlot = -1;
    static uint64_t s_cachedBombCarryOwnerUs = 0;
    static int s_cachedBombAttachedOwnerSlot = -1;
    static uint64_t s_cachedBombAttachedOwnerUs = 0;
    static int s_lastObservedBombOwnerSlot = -1;
    static BombState s_lastConfirmedBombState = {};
    static uint64_t s_lastConfirmedBombStateUs = 0;
    static uintptr_t s_cachedPlantedMetaEntity = 0;
    static uint64_t s_cachedPlantedMetaUs = 0;
    static uint8_t s_cachedPlantedTicking = 0;
    static uint8_t s_cachedPlantedBeingDefused = 0;
    static float s_cachedPlantedBlowTime = 0.0f;
    static float s_cachedPlantedDefuseEndTime = 0.0f;
    static uintptr_t s_cachedPlantedPosEntity = 0;
    static Vector3 s_cachedPlantedWorldPos = { NAN, NAN, NAN };
    static uintptr_t s_mergedPlantedEntity = 0;
    static uintptr_t s_mergedWeaponEntity = 0;
    static uintptr_t s_mergedBombSceneNode = 0;
    static uintptr_t s_mergedWeaponC4SceneNode = 0;
    static uint64_t s_bombCacheResetEpoch = 0;
    static bool s_prevBombPlantedByRules = false;
    static bool s_prevBombDroppedByRules = false;
    static float s_prevBombGameTime = 0.0f;
    static uint64_t s_lastBombSceneResetSerial = 0;
    static uint64_t s_bombDropResetGraceUntilUs = 0;

    
    
    
    
    
    
    
    static uint8_t s_prevBombCachedTicking = 0;
    static float s_prevBombCachedBlowTime = 0.0f;

    
    
    
    
    
    static uintptr_t s_explodedEntityTaintPtr = 0;
    static uint64_t s_explodedEntityTaintUntilUs = 0;

    const uint64_t bombCacheSceneResetSerial = s_sceneResetSerial.load(std::memory_order_relaxed);
    if (s_lastBombSceneResetSerial != bombCacheSceneResetSerial) {
        s_lastBombSceneResetSerial = bombCacheSceneResetSerial;
        s_explodedEntityTaintPtr = 0;
        s_explodedEntityTaintUntilUs = 0;
        s_bombDropResetGraceUntilUs = 0;
        s_bombEpoch.fetch_add(1, std::memory_order_relaxed);
    }
    
    
    
    if (s_prevBombPlantedByRules && !bombPlantedByRules) {
        s_bombEpoch.fetch_add(1, std::memory_order_relaxed);
        if (s_cachedPlantedMetaEntity != 0) {
            s_explodedEntityTaintPtr = s_cachedPlantedMetaEntity;
            s_explodedEntityTaintUntilUs = nowUs + 8000000;  
        }
    }
    
    
    
    
    
    if (s_prevBombDroppedByRules && !bombDroppedByRules && !bombPlantedByRules) {
        s_bombEpoch.fetch_add(1, std::memory_order_relaxed);
        
        
        
        
        
        
        
        s_bombDropResetGraceUntilUs = nowUs + 2000000;
    }
    
    
    
    
    
    {
        const bool hadLiveBombLastFrame =
            (s_prevBombCachedTicking != 0) ||
            (std::isfinite(s_prevBombCachedBlowTime) && s_prevBombCachedBlowTime > 0.0f);
        const bool blowTimeElapsed =
            std::isfinite(s_prevBombCachedBlowTime) &&
            s_prevBombCachedBlowTime > 0.0f &&
            std::isfinite(currentGameTime) &&
            s_prevBombCachedBlowTime <= currentGameTime;
        if (hadLiveBombLastFrame && blowTimeElapsed) {
            s_bombEpoch.fetch_add(1, std::memory_order_relaxed);
            if (s_cachedPlantedMetaEntity != 0) {
                s_explodedEntityTaintPtr = s_cachedPlantedMetaEntity;
                s_explodedEntityTaintUntilUs = nowUs + 8000000;  
            }
        }
    }
    if (std::isfinite(currentGameTime) &&
        std::isfinite(s_prevBombGameTime) &&
        currentGameTime + 1.0f < s_prevBombGameTime) {
        s_bombEpoch.fetch_add(1, std::memory_order_relaxed);
    }
    s_prevBombPlantedByRules = bombPlantedByRules;
    s_prevBombDroppedByRules = bombDroppedByRules;
    
    
    if (bombDroppedByRules)
        s_bombDropResetGraceUntilUs = 0;
    if (std::isfinite(currentGameTime))
        s_prevBombGameTime = currentGameTime;

    const uint64_t bombCacheEpoch = s_bombEpoch.load(std::memory_order_relaxed);
    if (s_bombCacheResetEpoch != bombCacheEpoch) {
        s_bombCacheResetEpoch = bombCacheEpoch;
        s_lastDroppedBombPos = { NAN, NAN, NAN };
        s_lastDroppedBombPosUs = 0;
        s_lastVisibleBombPos = { NAN, NAN, NAN };
        s_lastVisibleBombBoundsMins = {};
        s_lastVisibleBombBoundsMaxs = {};
        s_lastVisibleBombBoundsValid = false;
        s_lastVisibleBombPosUs = 0;
        s_lastWorldC4Pos = { NAN, NAN, NAN };
        s_lastWorldC4PosUs = 0;
        s_lastWorldC4NoOwner = false;
        s_lastWorldC4OwnerIdx = -1;
        s_lastWorldC4OwnerAlive = false;
        s_lastWorldC4OwnerNearby = false;
        s_cachedBombCarryOwnerSlot = -1;
        s_cachedBombCarryOwnerUs = 0;
        s_cachedBombAttachedOwnerSlot = -1;
        s_cachedBombAttachedOwnerUs = 0;
        s_lastObservedBombOwnerSlot = -1;
        s_lastConfirmedBombState = {};
        s_lastConfirmedBombStateUs = 0;
        s_cachedPlantedMetaEntity = 0;
        s_cachedPlantedMetaUs = 0;
        s_cachedPlantedTicking = 0;
        s_cachedPlantedBeingDefused = 0;
        s_cachedPlantedBlowTime = 0.0f;
        s_cachedPlantedDefuseEndTime = 0.0f;
        s_cachedPlantedPosEntity = 0;
        s_cachedPlantedWorldPos = { NAN, NAN, NAN };
        s_mergedPlantedEntity = 0;
        s_mergedWeaponEntity = 0;
        s_mergedBombSceneNode = 0;
        s_mergedWeaponC4SceneNode = 0;
        s_explodedEntityTaintPtr = 0;
        s_explodedEntityTaintUntilUs = 0;
        s_bombDropResetGraceUntilUs = 0;
    }

    
    
    
    
    
    
    
    
    
    s_prevBombCachedTicking = s_cachedPlantedTicking;
    s_prevBombCachedBlowTime = s_cachedPlantedBlowTime;
