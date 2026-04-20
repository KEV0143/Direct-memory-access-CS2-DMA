    
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

    #include "bomb_parts/bomb_entity_acquisition.inl"

    #include "bomb_parts/bomb_owner_signals.inl"

    
    
    

    
    #include "bomb_parts/bomb_state_resolve.inl"
