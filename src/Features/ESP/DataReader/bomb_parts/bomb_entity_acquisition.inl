    if (bombPlantedByRules) {
        uintptr_t resolvedPlantedC4 = resolvePlantedC4Entity(plantedC4Entity);
        if (!resolvedPlantedC4 && ofs.dwPlantedC4 > 0) {
            uintptr_t plantedRoot = 0;
            if (readPointer(g::clientBase + ofs.dwPlantedC4, &plantedRoot))
                resolvedPlantedC4 = resolvePlantedC4Entity(plantedRoot);
        }
        plantedC4Entity = resolvedPlantedC4;
    } else {
        plantedC4Entity = 0;
    }

    static uint64_t s_lastWeaponC4ProbeUs = 0;
    const bool cachedCarryEvidence =
        s_cachedBombCarryOwnerSlot >= 0 &&
        s_cachedBombCarryOwnerUs > 0 &&
        nowUs >= s_cachedBombCarryOwnerUs &&
        (nowUs - s_cachedBombCarryOwnerUs) <= 300000u;
    const bool canDeferWeaponC4Probe =
        !bombPlantedByRules &&
        !bombDroppedByRules &&
        (inventoryC4CarrierEvidence || cachedCarryEvidence) &&
        !s_bombState.planted &&
        !s_bombState.dropped;
    const bool weaponC4ProbeDue =
        !canDeferWeaponC4Probe ||
        s_lastWeaponC4ProbeUs == 0 ||
        (nowUs - s_lastWeaponC4ProbeUs) >= 100000u;

    if (!bombPlantedByRules && weaponC4ProbeDue) {
        s_lastWeaponC4ProbeUs = nowUs;
        uintptr_t resolvedWeaponC4 = sanitizePointer(weaponC4Entity);
        if (!resolvedWeaponC4 && ofs.dwWeaponC4 > 0) {
            uintptr_t weaponRoot = 0;
            if (readPointer(g::clientBase + ofs.dwWeaponC4, &weaponRoot))
                resolvedWeaponC4 = sanitizePointer(weaponRoot);
        }
        weaponC4Entity = resolvedWeaponC4;
    } else {
        weaponC4Entity = 0;
    }

    const bool plantedNodeCached =
        plantedC4Entity && plantedC4Entity == s_mergedPlantedEntity && s_mergedBombSceneNode;
    bool weaponNodeCached =
        weaponC4Entity && weaponC4Entity == s_mergedWeaponEntity && s_mergedWeaponC4SceneNode;
    if (weaponNodeCached &&
        !bombDroppedByRules &&
        !bombPlantedByRules &&
        (weaponC4OwnerHandle == 0u || weaponC4OwnerHandle == 0xFFFFFFFFu) &&
        !weaponC4PosValid) {
        s_mergedWeaponEntity = 0;
        s_mergedWeaponC4SceneNode = 0;
        weaponNodeCached = false;
    }
    const bool plantedMetaEntityStable =
        plantedC4Entity && plantedC4Entity == s_cachedPlantedMetaEntity;
    const bool plantedPosEntityStable =
        plantedC4Entity &&
        plantedC4Entity == s_cachedPlantedPosEntity &&
        isValidWorldPos(s_cachedPlantedWorldPos);
    if (plantedMetaEntityStable) {
        bombTicking = s_cachedPlantedTicking;
        bombBeingDefused = s_cachedPlantedBeingDefused;
        bombBlowTime = s_cachedPlantedBlowTime;
        bombTimerLength = s_cachedPlantedTimerLength;
        bombDefuseEndTime = s_cachedPlantedDefuseEndTime;
        bombDefuseLength = s_cachedPlantedDefuseLength;
    }
    if (plantedPosEntityStable)
        bombWorldPos = s_cachedPlantedWorldPos;
    const uint64_t plantedMetaRefreshUs = s_cachedPlantedBeingDefused ? 20000u : 40000u;
    const bool plantedMetaDue =
        plantedC4Entity &&
        (!plantedMetaEntityStable ||
         s_cachedPlantedMetaUs == 0 ||
         (nowUs - s_cachedPlantedMetaUs) >= plantedMetaRefreshUs);
    const bool plantedPosDue =
        plantedC4Entity &&
        !plantedPosEntityStable;
    const bool mergeScatters =
        (plantedNodeCached || !plantedC4Entity) &&
        (weaponNodeCached || !weaponC4Entity);

    {
        uintptr_t rawBombSceneNode = plantedNodeCached ? s_mergedBombSceneNode : 0;
        uintptr_t rawWeaponC4SceneNode = weaponNodeCached ? s_mergedWeaponC4SceneNode : 0;
        bool queuedBatch1 = false;

        bombCollisionMins = {};
        bombCollisionMaxs = {};
        weaponC4CollisionMins = {};
        weaponC4CollisionMaxs = {};

        if (plantedC4Entity) {
            if (!plantedNodeCached && ofs.C_BaseEntity_m_pGameSceneNode > 0) {
                mem.AddScatterReadRequest(handle, plantedC4Entity + ofs.C_BaseEntity_m_pGameSceneNode, &rawBombSceneNode, sizeof(rawBombSceneNode));
                queuedBatch1 = true;
            }
            if (plantedMetaDue && ofs.C_PlantedC4_m_bBombTicking > 0) {
                mem.AddScatterReadRequest(handle, plantedC4Entity + ofs.C_PlantedC4_m_bBombTicking, &bombTicking, sizeof(bombTicking));
                queuedBatch1 = true;
            }
            if (plantedMetaDue && ofs.C_PlantedC4_m_bBeingDefused > 0) {
                mem.AddScatterReadRequest(handle, plantedC4Entity + ofs.C_PlantedC4_m_bBeingDefused, &bombBeingDefused, sizeof(bombBeingDefused));
                queuedBatch1 = true;
            }
            if (plantedMetaDue && ofs.C_PlantedC4_m_flC4Blow > 0) {
                mem.AddScatterReadRequest(handle, plantedC4Entity + ofs.C_PlantedC4_m_flC4Blow, &bombBlowTime, sizeof(bombBlowTime));
                queuedBatch1 = true;
            }
            if (plantedMetaDue && ofs.C_PlantedC4_m_flTimerLength > 0) {
                mem.AddScatterReadRequest(handle, plantedC4Entity + ofs.C_PlantedC4_m_flTimerLength, &bombTimerLength, sizeof(bombTimerLength));
                queuedBatch1 = true;
            }
            if (plantedMetaDue && ofs.C_PlantedC4_m_flDefuseCountDown > 0) {
                mem.AddScatterReadRequest(handle, plantedC4Entity + ofs.C_PlantedC4_m_flDefuseCountDown, &bombDefuseEndTime, sizeof(bombDefuseEndTime));
                queuedBatch1 = true;
            }
            if (plantedMetaDue && ofs.C_PlantedC4_m_flDefuseLength > 0) {
                mem.AddScatterReadRequest(handle, plantedC4Entity + ofs.C_PlantedC4_m_flDefuseLength, &bombDefuseLength, sizeof(bombDefuseLength));
                queuedBatch1 = true;
            }
            if (mergeScatters && s_mergedBombSceneNode && plantedPosDue && ofs.CGameSceneNode_m_vecAbsOrigin > 0) {
                mem.AddScatterReadRequest(handle, s_mergedBombSceneNode + ofs.CGameSceneNode_m_vecAbsOrigin, &bombWorldPos, sizeof(bombWorldPos));
                queuedBatch1 = true;
            }
        }

        if (weaponC4Entity) {
            if (!weaponNodeCached && ofs.C_BaseEntity_m_pGameSceneNode > 0) {
                mem.AddScatterReadRequest(handle, weaponC4Entity + ofs.C_BaseEntity_m_pGameSceneNode, &rawWeaponC4SceneNode, sizeof(rawWeaponC4SceneNode));
                queuedBatch1 = true;
            }
            if (ofs.C_BaseEntity_m_hOwnerEntity > 0) {
                mem.AddScatterReadRequest(handle, weaponC4Entity + ofs.C_BaseEntity_m_hOwnerEntity, &weaponC4OwnerHandle, sizeof(weaponC4OwnerHandle));
                queuedBatch1 = true;
            }
            if (mergeScatters && s_mergedWeaponC4SceneNode && ofs.CGameSceneNode_m_vecAbsOrigin > 0) {
                mem.AddScatterReadRequest(handle, s_mergedWeaponC4SceneNode + ofs.CGameSceneNode_m_vecAbsOrigin, &weaponC4WorldPos, sizeof(weaponC4WorldPos));
                queuedBatch1 = true;
            }
        }

        if (queuedBatch1)
            executeOptionalScatterRead();

        bombSceneNode = sanitizePointer(rawBombSceneNode);
        weaponC4SceneNode = sanitizePointer(rawWeaponC4SceneNode);
    }

    if (!mergeScatters) {
        bool queuedBatch2 = false;
        if (bombSceneNode && plantedPosDue && ofs.CGameSceneNode_m_vecAbsOrigin > 0) {
            mem.AddScatterReadRequest(handle, bombSceneNode + ofs.CGameSceneNode_m_vecAbsOrigin, &bombWorldPos, sizeof(bombWorldPos));
            queuedBatch2 = true;
        }
        if (weaponC4SceneNode && ofs.CGameSceneNode_m_vecAbsOrigin > 0) {
            mem.AddScatterReadRequest(handle, weaponC4SceneNode + ofs.CGameSceneNode_m_vecAbsOrigin, &weaponC4WorldPos, sizeof(weaponC4WorldPos));
            queuedBatch2 = true;
        }
        if (queuedBatch2)
            executeOptionalScatterRead();
    }

    if (weaponC4SceneNode) {
        weaponC4PosValid = isDroppedC4PositionPlausible(weaponC4WorldPos);
    }

    if (bombPlantedByRules &&
        plantedC4Entity &&
        !bombSceneNode &&
        ofs.C_BaseEntity_m_pGameSceneNode > 0) {
        uintptr_t retrySceneNode = 0;
        if (readValue(plantedC4Entity + ofs.C_BaseEntity_m_pGameSceneNode, &retrySceneNode, sizeof(retrySceneNode)))
            bombSceneNode = sanitizePointer(retrySceneNode);
    }
    if (bombPlantedByRules &&
        bombSceneNode &&
        !isValidWorldPos(bombWorldPos) &&
        ofs.CGameSceneNode_m_vecAbsOrigin > 0) {
        readValue(bombSceneNode + ofs.CGameSceneNode_m_vecAbsOrigin, &bombWorldPos, sizeof(bombWorldPos));
    }
    if (!bombPlantedByRules &&
        weaponC4Entity &&
        !weaponC4SceneNode &&
        ofs.C_BaseEntity_m_pGameSceneNode > 0) {
        uintptr_t retrySceneNode = 0;
        if (readValue(weaponC4Entity + ofs.C_BaseEntity_m_pGameSceneNode, &retrySceneNode, sizeof(retrySceneNode)))
            weaponC4SceneNode = sanitizePointer(retrySceneNode);
    }
    if (!bombPlantedByRules &&
        weaponC4SceneNode &&
        !weaponC4PosValid &&
        ofs.CGameSceneNode_m_vecAbsOrigin > 0 &&
        readValue(weaponC4SceneNode + ofs.CGameSceneNode_m_vecAbsOrigin, &weaponC4WorldPos, sizeof(weaponC4WorldPos))) {
        weaponC4PosValid = isDroppedC4PositionPlausible(weaponC4WorldPos);
    }

    s_mergedPlantedEntity = plantedC4Entity;
    s_mergedWeaponEntity = weaponC4Entity;
    s_mergedBombSceneNode = bombSceneNode;
    s_mergedWeaponC4SceneNode = weaponC4SceneNode;
    if (plantedC4Entity) {
        if (plantedMetaDue) {
            s_cachedPlantedMetaEntity = plantedC4Entity;
            s_cachedPlantedMetaUs = nowUs;
            s_cachedPlantedTicking = bombTicking;
            s_cachedPlantedBeingDefused = bombBeingDefused;
            s_cachedPlantedBlowTime = bombBlowTime;
            s_cachedPlantedTimerLength = bombTimerLength;
            s_cachedPlantedDefuseEndTime = bombDefuseEndTime;
            s_cachedPlantedDefuseLength = bombDefuseLength;
        }
        if (isValidWorldPos(bombWorldPos)) {
            s_cachedPlantedPosEntity = plantedC4Entity;
            s_cachedPlantedWorldPos = bombWorldPos;
        }
    } else {
        s_cachedPlantedMetaEntity = 0;
        s_cachedPlantedMetaUs = 0;
        s_cachedPlantedTicking = 0;
        s_cachedPlantedBeingDefused = 0;
        s_cachedPlantedBlowTime = 0.0f;
        s_cachedPlantedTimerLength = 0.0f;
        s_cachedPlantedDefuseEndTime = 0.0f;
        s_cachedPlantedDefuseLength = 0.0f;
        s_cachedPlantedPosEntity = 0;
        s_cachedPlantedWorldPos = { NAN, NAN, NAN };
    }
