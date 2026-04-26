bool esp::GetWebRadarSnapshot(WebRadarSnapshot* outSnapshot)
{
    if (!outSnapshot)
        return false;

    

    
    const int readIdx = s_readIdx.load(std::memory_order_acquire);
    const EntitySnapshot& snap = s_entityBuf[readIdx];

    Vector3 effectiveLocalPos = snap.localPos;
    if (!std::isfinite(effectiveLocalPos.x) ||
        !std::isfinite(effectiveLocalPos.y) ||
        !std::isfinite(effectiveLocalPos.z) ||
        (std::fabs(effectiveLocalPos.x) + std::fabs(effectiveLocalPos.y) + std::fabs(effectiveLocalPos.z) <= 1.0f)) {
        std::lock_guard<std::mutex> lock(s_cameraMutex);
        const uint64_t nowUs = TickNowUs();
        const bool liveLocalPosFresh =
            s_liveLocalPosValid &&
            s_liveLocalPosUpdatedUs > 0 &&
            nowUs >= s_liveLocalPosUpdatedUs &&
            (nowUs - s_liveLocalPosUpdatedUs) <= kLiveCameraFreshnessUs;
        if (liveLocalPosFresh)
            effectiveLocalPos = s_liveLocalPos;
    }
    const bool hasEffectiveLocalPos =
        std::isfinite(effectiveLocalPos.x) &&
        std::isfinite(effectiveLocalPos.y) &&
        std::isfinite(effectiveLocalPos.z) &&
        (std::fabs(effectiveLocalPos.x) + std::fabs(effectiveLocalPos.y) + std::fabs(effectiveLocalPos.z) > 1.0f);
    if (!hasEffectiveLocalPos)
        effectiveLocalPos = { NAN, NAN, NAN };

    WebRadarSnapshot snapshot = {};
    for (size_t i = 0; i < snapshot.players.size(); ++i)
        snapshot.players[i] = snap.webRadarPlayers[i];

    std::copy(std::begin(snap.localName), std::end(snap.localName), std::begin(snapshot.localName));
    std::copy(std::begin(snap.activeMapKey), std::end(snap.activeMapKey), std::begin(snapshot.mapKey));
    snapshot.localPos = effectiveLocalPos;
    snapshot.localIsDead = snap.localIsDead;
    snapshot.localHealth = snap.localHealth;
    snapshot.localArmor = snap.localArmor;
    snapshot.localMoney = snap.localMoney;
    snapshot.localYaw = snap.viewAngles.y;
    snapshot.localWeaponId = snap.localWeaponId;
    snapshot.localAmmoClip = snap.localAmmoClip;
    snapshot.localHasBomb = snap.localHasBomb;
    snapshot.localHasDefuser = snap.localHasDefuser;
    snapshot.localGrenadeCount = snap.localGrenadeCount;
    std::copy(std::begin(snap.localGrenadeIds), std::end(snap.localGrenadeIds), std::begin(snapshot.localGrenadeIds));
    snapshot.minimapMins = snap.minimapMins;
    snapshot.minimapMaxs = snap.minimapMaxs;
    snapshot.localTeam = snap.localTeam;
    snapshot.hasMinimapBounds = snap.hasMinimapBounds;
    snapshot.captureTickMs = TickNowMs();

    snapshot.bomb.planted = snap.bombState.planted;
    snapshot.bomb.ticking = snap.bombState.ticking;
    snapshot.bomb.beingDefused = snap.bombState.beingDefused;
    snapshot.bomb.dropped = snap.bombState.dropped;
    snapshot.bomb.position = snap.bombState.position;
    snapshot.bomb.blowTime = snap.bombState.blowTime;
    snapshot.bomb.timerLength = snap.bombState.timerLength;
    snapshot.bomb.defuseEndTime = snap.bombState.defuseEndTime;
    snapshot.bomb.defuseLength = snap.bombState.defuseLength;
    snapshot.bomb.currentGameTime = snap.bombState.currentGameTime;

    
    {
        const uint64_t nowUs = TickNowUs();
        snapshot.worldMarkerCount = 0;
        if (!g::espWorld) {
            *outSnapshot = snapshot;
            return true;
        }
        for (int i = 0; i < snap.worldMarkerCount && snapshot.worldMarkerCount < WebRadarSnapshot::kMaxWorldMarkers; ++i) {
            const auto& wm = snap.worldMarkers[i];
            if (!wm.valid)
                continue;
            if (wm.expiresUs > 0 && wm.expiresUs < nowUs)
                continue;
            
            if (wm.type == WorldMarkerType::DroppedWeapon ||
                wm.type == WorldMarkerType::SmokeProjectile ||
                wm.type == WorldMarkerType::MolotovProjectile ||
                wm.type == WorldMarkerType::DecoyProjectile)
                continue;
            auto& out = snapshot.worldMarkers[snapshot.worldMarkerCount++];
            out.type = static_cast<uint8_t>(wm.type);
            out.position = wm.position;
            out.weaponId = wm.weaponId;
            out.lifeRemainingSec = (wm.expiresUs > nowUs)
                ? static_cast<float>(wm.expiresUs - nowUs) / 1000000.0f
                : 0.0f;
        }
    }

    *outSnapshot = snapshot;
    return true;
}
