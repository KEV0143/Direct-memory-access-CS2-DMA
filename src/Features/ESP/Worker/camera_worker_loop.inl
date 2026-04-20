namespace {
    void CameraWorkerLoop()
    {
        using Clock = std::chrono::steady_clock;
        const auto tickInterval = std::chrono::microseconds(1000000 / CAMERA_WORKER_HZ);
        auto nextTick = Clock::now() + JitterDuration(tickInterval, kCameraWorkerJitterPercent);
        uint32_t consecutiveViewMisses = 0;
        uint32_t consecutiveLocalPosMisses = 0;
        uintptr_t cachedLocalPawn = 0;
        uintptr_t cachedLocalSceneNode = 0;
        VMMDLL_SCATTER_HANDLE cameraHandle = mem.CreateScatterHandle();
        s_cameraWorkerRunning.store(true, std::memory_order_relaxed);

        while (!s_dataWorkerStopRequested.load(std::memory_order_relaxed)) {
            try {
                const auto cycleStart = Clock::now();
                const auto& ofs = runtime_offsets::Get();
                const uintptr_t clientBase = g::clientBase;
                const uint64_t nowUs = TickNowUs();

                if (clientBase) {
                    view_matrix_t liveViewMatrix = {};
                    Vector3 liveViewAngles = {};
                    uintptr_t liveLocalPawn = 0;
                    uintptr_t liveLocalSceneNode = 0;
                    Vector3 liveLocalPos = {};
                    uintptr_t rawLocalPawn = 0;
                    uintptr_t rawCachedPawnSceneNode = 0;
                    Vector3 cachedSceneNodePos = {};

                    bool gotMatrix = false;
                    bool gotAngles = false;
                    bool gotLocalPos = false;
                    auto recreateCameraHandle = [&]() {
                        if (cameraHandle)
                            mem.CloseScatterHandle(cameraHandle);
                        cameraHandle = mem.CreateScatterHandle();
                    };
                    auto isValidLivePos = [](const Vector3& pos) -> bool {
                        return
                            std::isfinite(pos.x) &&
                            std::isfinite(pos.y) &&
                            std::isfinite(pos.z) &&
                            (std::fabs(pos.x) + std::fabs(pos.y) + std::fabs(pos.z) > 1.0f);
                    };
                    auto sanitizeCameraPointer = [](uintptr_t value) -> uintptr_t {
                        return (value >= 0x1000 && value < 0x7FFFFFFFFFFFULL) ? value : 0;
                    };
                    
                    
                    
                    
                    
                    
                    
                    auto isLikelyViewMatrix = [](const view_matrix_t& matrix) -> bool {
                        float absSum = 0.0f;
                        int nonZeroCount = 0;
                        for (int row = 0; row < 4; ++row) {
                            for (int col = 0; col < 4; ++col) {
                                const float value = matrix[row][col];
                                if (!std::isfinite(value))
                                    return false;
                                absSum += std::fabs(value);
                                if (std::fabs(value) > 0.0001f)
                                    ++nonZeroCount;
                            }
                        }
                        return nonZeroCount >= 6 && absSum > 1.0f;
                    };

                    if (!cameraHandle)
                        cameraHandle = mem.CreateScatterHandle();

                    bool primaryScatterOk = false;
                    if (cameraHandle) {
                        bool queuedPrimary = false;
                        if (ofs.dwViewMatrix > 0) {
                            mem.AddScatterReadRequest(cameraHandle, clientBase + ofs.dwViewMatrix, &liveViewMatrix, sizeof(view_matrix_t));
                            queuedPrimary = true;
                        }
                        if (ofs.dwViewAngles > 0) {
                            mem.AddScatterReadRequest(cameraHandle, clientBase + ofs.dwViewAngles, &liveViewAngles, sizeof(Vector3));
                            queuedPrimary = true;
                        }
                        if (ofs.dwLocalPlayerPawn > 0) {
                            mem.AddScatterReadRequest(cameraHandle, clientBase + ofs.dwLocalPlayerPawn, &rawLocalPawn, sizeof(uintptr_t));
                            queuedPrimary = true;
                        }
                        if (cachedLocalPawn && ofs.C_BaseEntity_m_pGameSceneNode > 0) {
                            mem.AddScatterReadRequest(
                                cameraHandle,
                                cachedLocalPawn + ofs.C_BaseEntity_m_pGameSceneNode,
                                &rawCachedPawnSceneNode,
                                sizeof(uintptr_t));
                            queuedPrimary = true;
                        }
                        if (cachedLocalSceneNode && ofs.CGameSceneNode_m_vecAbsOrigin > 0) {
                            mem.AddScatterReadRequest(
                                cameraHandle,
                                cachedLocalSceneNode + ofs.CGameSceneNode_m_vecAbsOrigin,
                                &cachedSceneNodePos,
                                sizeof(Vector3));
                            queuedPrimary = true;
                        }
                        primaryScatterOk = queuedPrimary && mem.ExecuteReadScatter(cameraHandle);
                        if (!primaryScatterOk)
                            recreateCameraHandle();
                    }

                    if (primaryScatterOk) {
                        gotMatrix = (ofs.dwViewMatrix > 0) && isLikelyViewMatrix(liveViewMatrix);
                        gotAngles = (ofs.dwViewAngles > 0) &&
                            std::isfinite(liveViewAngles.x) &&
                            std::isfinite(liveViewAngles.y) &&
                            std::isfinite(liveViewAngles.z);
                        liveLocalPawn = sanitizeCameraPointer(rawLocalPawn);
                        const uintptr_t freshCachedSceneNode = sanitizeCameraPointer(rawCachedPawnSceneNode);
                        if (liveLocalPawn && liveLocalPawn == cachedLocalPawn) {
                            if (freshCachedSceneNode)
                                liveLocalSceneNode = freshCachedSceneNode;
                            if (liveLocalSceneNode &&
                                liveLocalSceneNode == cachedLocalSceneNode &&
                                isValidLivePos(cachedSceneNodePos)) {
                                liveLocalPos = cachedSceneNodePos;
                                gotLocalPos = true;
                            }
                        }
                    }

                    if (!gotLocalPos &&
                        liveLocalPawn &&
                        ofs.C_BaseEntity_m_pGameSceneNode > 0 &&
                        cameraHandle) {
                        uintptr_t rawLiveSceneNode = 0;
                        mem.AddScatterReadRequest(
                            cameraHandle,
                            liveLocalPawn + ofs.C_BaseEntity_m_pGameSceneNode,
                            &rawLiveSceneNode,
                            sizeof(uintptr_t));
                        if (mem.ExecuteReadScatter(cameraHandle)) {
                            liveLocalSceneNode = sanitizeCameraPointer(rawLiveSceneNode);
                        } else {
                            recreateCameraHandle();
                        }
                    }

                    if (!gotLocalPos &&
                        liveLocalSceneNode &&
                        ofs.CGameSceneNode_m_vecAbsOrigin > 0 &&
                        cameraHandle) {
                        mem.AddScatterReadRequest(
                            cameraHandle,
                            liveLocalSceneNode + ofs.CGameSceneNode_m_vecAbsOrigin,
                            &liveLocalPos,
                            sizeof(Vector3));
                        if (mem.ExecuteReadScatter(cameraHandle)) {
                            gotLocalPos = isValidLivePos(liveLocalPos);
                        } else {
                            recreateCameraHandle();
                        }
                    }

                    if (liveLocalPawn != cachedLocalPawn) {
                        cachedLocalPawn = liveLocalPawn;
                        cachedLocalSceneNode = 0;
                    }
                    if (liveLocalSceneNode)
                        cachedLocalSceneNode = liveLocalSceneNode;
                    if (!liveLocalPawn)
                        cachedLocalSceneNode = 0;

                    if (gotMatrix)
                        consecutiveViewMisses = 0;
                    else
                        ++consecutiveViewMisses;

                    if (gotLocalPos)
                        consecutiveLocalPosMisses = 0;
                    else
                        ++consecutiveLocalPosMisses;

                    {
                        std::lock_guard<std::mutex> lock(s_cameraMutex);
                        if (gotMatrix)
                            memcpy(&s_liveViewMatrix, &liveViewMatrix, sizeof(view_matrix_t));
                        if (gotAngles)
                            s_liveViewAngles = liveViewAngles;
                        if (gotLocalPos)
                            s_liveLocalPos = liveLocalPos;

                        if (gotMatrix) {
                            s_liveViewValid = true;
                            s_liveViewUpdatedUs = nowUs;
                        } else if (consecutiveViewMisses >= kCameraInvalidateMissThreshold) {
                            s_liveViewValid = false;
                        }

                        if (gotLocalPos) {
                            s_liveLocalPosValid = true;
                            s_liveLocalPosUpdatedUs = nowUs;
                        } else if (consecutiveLocalPosMisses >= kCameraInvalidateMissThreshold) {
                            s_liveLocalPosValid = false;
                        }
                    }

                    const auto sceneWarmupState =
                        static_cast<esp::SceneWarmupState>(s_sceneWarmupState.load(std::memory_order_relaxed));
                    const bool cameraRecoveryAllowed =
                        sceneWarmupState == esp::SceneWarmupState::Stable &&
                        s_engineInGame.load(std::memory_order_relaxed);
                    const uint64_t lastPublishUs = s_lastPublishUs.load(std::memory_order_relaxed);
                    const bool snapshotFresh =
                        lastPublishUs > 0 &&
                        nowUs >= lastPublishUs &&
                        (nowUs - lastPublishUs) <= 500000u;
                    if (consecutiveViewMisses >= kCameraRecoveryMissThreshold &&
                        cameraRecoveryAllowed) {
                        static uint64_t s_lastCameraSelfHealUs = 0;
                        if (nowUs - s_lastCameraSelfHealUs >= 250000u) {
                            s_lastCameraSelfHealUs = nowUs;
                            recreateCameraHandle();
                            if (mem.vHandle) {
                                VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_FREQ_FAST, 1);
                                VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_FREQ_TLB_PARTIAL, 1);
                            }
                        }

                        if (!snapshotFresh &&
                            consecutiveViewMisses >= (kCameraRecoveryMissThreshold * 4u)) {
                            RequestDmaRecovery("camera_snapshot_stalled_persistent");
                        }
                    }
                } else {
                    consecutiveViewMisses = 0;
                    consecutiveLocalPosMisses = 0;
                    cachedLocalPawn = 0;
                    cachedLocalSceneNode = 0;
                    ResetCameraSnapshot();
                }

                const auto cycleEnd = Clock::now();
                const uint64_t cycleUs = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(cycleEnd - cycleStart).count());
                s_cameraWorkerCycleUs.store(cycleUs, std::memory_order_relaxed);
                uint64_t prevMax = s_cameraWorkerMaxCycleUs.load(std::memory_order_relaxed);
                while (cycleUs > prevMax &&
                       !s_cameraWorkerMaxCycleUs.compare_exchange_weak(prevMax, cycleUs, std::memory_order_relaxed))
                    ;
            } catch (...) {
                consecutiveViewMisses = kCameraRecoveryMissThreshold;
                consecutiveLocalPosMisses = kCameraInvalidateMissThreshold;
                ResetCameraSnapshot();
                if (cameraHandle)
                    mem.CloseScatterHandle(cameraHandle);
                cameraHandle = mem.CreateScatterHandle();
                DmaLogPrintf("[ERROR] CameraWorkerLoop: camera read exception caught, continuing");
                s_dmaConsecutiveFailures.fetch_add(1, std::memory_order_relaxed);
                s_dmaTotalFailures.fetch_add(1, std::memory_order_relaxed);
                const auto sceneWarmupState =
                    static_cast<esp::SceneWarmupState>(s_sceneWarmupState.load(std::memory_order_relaxed));
                const uint64_t nowUs = TickNowUs();
                const uint64_t lastPublishUs = s_lastPublishUs.load(std::memory_order_relaxed);
                const bool snapshotFresh =
                    lastPublishUs > 0 &&
                    nowUs >= lastPublishUs &&
                    (nowUs - lastPublishUs) <= 500000u;
                if (s_engineInGame.load(std::memory_order_relaxed) &&
                    sceneWarmupState == esp::SceneWarmupState::Stable &&
                    !snapshotFresh)
                    RequestDmaRecovery("camera_worker_exception_persistent");
            }

            nextTick += JitterDuration(tickInterval, kCameraWorkerJitterPercent);
            PreciseSleepUntil<Clock>(nextTick);

            const auto now = Clock::now();
            if (now > nextTick + std::chrono::milliseconds(50))
                nextTick = now;
        }

        if (cameraHandle)
            mem.CloseScatterHandle(cameraHandle);
        s_cameraWorkerRunning.store(false, std::memory_order_relaxed);
    }
}
