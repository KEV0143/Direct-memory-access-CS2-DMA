    
    #include "bomb_parts/bomb_cache_reset.inl"

    auto isValidBombBounds = [](const Vector3& mins, const Vector3& maxs) -> bool {
        if (!std::isfinite(mins.x) || !std::isfinite(mins.y) || !std::isfinite(mins.z) ||
            !std::isfinite(maxs.x) || !std::isfinite(maxs.y) || !std::isfinite(maxs.z))
            return false;

        const float spanX = maxs.x - mins.x;
        const float spanY = maxs.y - mins.y;
        const float spanZ = maxs.z - mins.z;
        
        
        return spanX > 0.1f && spanY > 0.1f && spanZ > 0.1f &&
               spanX < 40.0f && spanY < 40.0f && spanZ < 40.0f;
    };

    auto tryResolveSceneNode = [&](uintptr_t ent, uintptr_t* outSceneNode) -> bool {
        if (!ent || ofs.C_BaseEntity_m_pGameSceneNode <= 0)
            return false;
        uintptr_t rawSceneNode = 0;
        if (!readValue(ent + ofs.C_BaseEntity_m_pGameSceneNode, &rawSceneNode, sizeof(rawSceneNode)))
            return false;
        rawSceneNode = sanitizePointer(rawSceneNode);
        if (!rawSceneNode)
            return false;
        if (outSceneNode)
            *outSceneNode = rawSceneNode;
        return true;
    };

    auto isDroppedC4PositionPlausible = [&](const Vector3& pos) -> bool {
        if (!isValidWorldPos(pos))
            return false;

        if (minimapBoundsValid) {
            const float minX = (std::min)(minimapMins.x, minimapMaxs.x) - 1024.0f;
            const float maxX = (std::max)(minimapMins.x, minimapMaxs.x) + 1024.0f;
            const float minY = (std::min)(minimapMins.y, minimapMaxs.y) - 1024.0f;
            const float maxY = (std::max)(minimapMins.y, minimapMaxs.y) + 1024.0f;
            if (pos.x < minX || pos.x > maxX || pos.y < minY || pos.y > maxY)
                return false;
        }

        int sampleCount = 0;
        float minPlayerZ = FLT_MAX;
        float maxPlayerZ = -FLT_MAX;
        for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
            const int i = playerResolvedSlots[resolvedIdx];
            if (!coreReadFresh[i] ||
                !coreReadAlive[i] ||
                healths[i] <= 0 ||
                lifeStates[i] != 0 ||
                !isValidWorldPos(positions[i]))
                continue;
            minPlayerZ = (std::min)(minPlayerZ, positions[i].z);
            maxPlayerZ = (std::max)(maxPlayerZ, positions[i].z);
            ++sampleCount;
        }

        if (sampleCount >= 2 && (pos.z < minPlayerZ - 220.0f || pos.z > maxPlayerZ + 220.0f))
            return false;

        return true;
    };

    auto isDroppedC4WeakPositionPlausible = [&](const Vector3& pos) -> bool {
        if (!isDroppedC4PositionPlausible(pos))
            return false;

        int sampleCount = 0;
        float nearestDist2 = FLT_MAX;
        float nearestDz = FLT_MAX;
        float minPlayerZ = FLT_MAX;
        float maxPlayerZ = -FLT_MAX;
        for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
            const int i = playerResolvedSlots[resolvedIdx];
            if (!coreReadFresh[i] ||
                !coreReadAlive[i] ||
                healths[i] <= 0 ||
                lifeStates[i] != 0 ||
                !isValidWorldPos(positions[i]))
                continue;
            const float dx = pos.x - positions[i].x;
            const float dy = pos.y - positions[i].y;
            const float dist2 = dx * dx + dy * dy;
            if (dist2 < nearestDist2) {
                nearestDist2 = dist2;
                nearestDz = std::fabs(pos.z - positions[i].z);
            }
            minPlayerZ = (std::min)(minPlayerZ, positions[i].z);
            maxPlayerZ = (std::max)(maxPlayerZ, positions[i].z);
            ++sampleCount;
        }

        if (sampleCount == 0)
            return true;
        if (pos.z < minPlayerZ - 160.0f || pos.z > maxPlayerZ + 160.0f)
            return false;
        if (nearestDist2 <= (384.0f * 384.0f) && nearestDz > 128.0f)
            return false;
        return true;
    };

    auto looksLikePlantedC4Entity = [&](uintptr_t ent) -> bool {
        if (!ent)
            return false;
        uintptr_t sceneNode = 0;
        if (tryResolveSceneNode(ent, &sceneNode))
            return true;
        if (ofs.C_PlantedC4_m_bBombTicking > 0) {
            uint8_t ticking = 0;
            if (readValue(ent + ofs.C_PlantedC4_m_bBombTicking, &ticking, sizeof(ticking)))
                return true;
        }
        if (ofs.C_PlantedC4_m_flC4Blow > 0) {
            float blowTime = 0.0f;
            if (readValue(ent + ofs.C_PlantedC4_m_flC4Blow, &blowTime, sizeof(blowTime)) &&
                std::isfinite(blowTime) && blowTime > 0.0f)
                return true;
        }
        return false;
    };

    auto resolvePlantedC4Entity = [&](uintptr_t candidate) -> uintptr_t {
        candidate = sanitizePointer(candidate);
        if (!candidate)
            return 0;
        if (looksLikePlantedC4Entity(candidate))
            return candidate;
        uintptr_t deref = 0;
        if (readPointer(candidate, &deref) && looksLikePlantedC4Entity(deref))
            return deref;
        return 0;
    };

    bool inventoryC4CarrierEvidence = false;
    int inventoryC4CarrierSlot = -1;
    for (int resolvedIdx = 0; resolvedIdx < playerResolvedSlotCount; ++resolvedIdx) {
        const int i = playerResolvedSlots[resolvedIdx];
        if (!pawns[i] ||
            healths[i] <= 0 ||
            lifeStates[i] != 0 ||
            teams[i] != 2) {
            continue;
        }
        const uint16_t liveWeaponId = (weaponIds[i] < 20000u) ? weaponIds[i] : 0u;
        if (!inventoryHasBombBySlot[i] && liveWeaponId != kWeaponC4Id)
            continue;
        bombCarrierBySlot[i] = true;
        inventoryC4CarrierEvidence = true;
        if (inventoryC4CarrierSlot < 0)
            inventoryC4CarrierSlot = i;
    }

    #include "bomb_parts/bomb_entity_acquisition.inl"

    #include "bomb_parts/bomb_owner_signals.inl"

    
    
    

    
    #include "bomb_parts/bomb_state_resolve.inl"
