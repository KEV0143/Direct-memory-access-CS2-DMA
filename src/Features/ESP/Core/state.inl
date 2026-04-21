


namespace {
    
    static constexpr uint32_t kEntitySlotMask    = 0x1FFu;   
    static constexpr uint32_t kEntityHandleMask  = 0x7FFFu;  
    static constexpr uint32_t kEntitySlotSize    = 0x70u;     
    static constexpr uint16_t kWeaponC4Id        = 49u;

    enum class WorldMarkerType : uint8_t {
        DroppedWeapon = 0,
        Smoke,
        Inferno,
        Decoy,
        Explosive,
        SmokeProjectile,
        MolotovProjectile,
        DecoyProjectile
    };

    struct WorldMarker {
        bool valid = false;
        WorldMarkerType type = WorldMarkerType::DroppedWeapon;
        Vector3 position = {};
        uint16_t weaponId = 0;
        float lifeHint = 0.0f;
        uint64_t expiresUs = 0;
    };

    struct BombState {
        bool planted = false;
        bool ticking = false;
        bool beingDefused = false;
        bool dropped = false;
        uint32_t sourceFlags = 0;
        uint8_t confidence = 0;
        Vector3 position = {};
        Vector3 rawPosition = {};
        Vector3 boundsMins = {};
        Vector3 boundsMaxs = {};
        bool boundsValid = false;
        float blowTime = 0.0f;
        float defuseEndTime = 0.0f;
        float currentGameTime = 0.0f;
    };

    enum class BombResolveKind : uint8_t {
        Hidden = 0,
        Carried,
        DroppedProbable,
        DroppedConfirmed,
        Planted,
    };

    enum BombResolveSourceFlags : uint32_t {
        BombResolveSourceRules        = 1u << 0,
        BombResolveSourceWeaponEntity = 1u << 1,
        BombResolveSourceWorldC4      = 1u << 2,
        BombResolveSourceCarrySignal  = 1u << 3,
        BombResolveSourceCarrySticky  = 1u << 4,
        BombResolveSourceAttachGrace  = 1u << 5,
        BombResolveSourceStickyDrop   = 1u << 6,
        BombResolveSourceStickyState  = 1u << 7,
        BombResolveSourceSpatialVeto  = 1u << 8,
    };

    struct BombResolveResult {
        BombResolveKind kind = BombResolveKind::Hidden;
        uint32_t sourceFlags = 0;
        uint8_t confidence = 0;
        Vector3 position = { NAN, NAN, NAN };
        Vector3 boundsMins = {};
        Vector3 boundsMaxs = {};
        bool boundsValid = false;
    };

    constexpr bool IsDroppedBombResolveKind(BombResolveKind kind)
    {
        return kind == BombResolveKind::DroppedConfirmed ||
               kind == BombResolveKind::DroppedProbable;
    }

    struct EntitySnapshot {
        esp::PlayerData players[64] = {};
        esp::PlayerData prevPlayers[64] = {};
        esp::PlayerData webRadarPlayers[64] = {};
        char       localName[128] = {};
        char       activeMapKey[64] = {};
        int        localPlayerIndex = -1;
        int        localTeam = 0;
        uintptr_t  localPawn = 0;
        Vector3    localPos = {};
        Vector3    prevLocalPos = {};
        bool       localIsDead = false;
        int        localHealth = 0;
        int        localArmor = 0;
        int        localMoney = 0;
        uint16_t   localWeaponId = 0;
        int        localAmmoClip = -1;
        bool       localHasBomb = false;
        bool       localHasDefuser = false;
        uint16_t   localGrenadeIds[esp::PlayerData::kMaxGrenades] = {};
        int        localGrenadeCount = 0;
        Vector3    viewAngles = {};
        view_matrix_t viewMatrix = {};
        Vector3    minimapMins = {};
        Vector3    minimapMaxs = {};
        bool       hasMinimapBounds = false;
        bool       localMaskResolved = false;
        uint64_t   captureTimeUs = 0;
        uint64_t   prevCaptureTimeUs = 0;
        WorldMarker worldMarkers[256] = {};
        int        worldMarkerCount = 0;
        BombState  bombState = {};
    };

    enum SnapshotPublishMask : uint32_t {
        SnapshotPlayers      = 1u << 0,
        SnapshotLocalView    = 1u << 1,
        SnapshotLocalLoadout = 1u << 2,
        SnapshotTiming       = 1u << 3,
        SnapshotWorld        = 1u << 4,
        SnapshotBomb         = 1u << 5,
        SnapshotWebRadar     = 1u << 6,
        SnapshotLocal        = SnapshotLocalView | SnapshotLocalLoadout,
        SnapshotView         = SnapshotLocalView,
        SnapshotAll          = SnapshotPlayers | SnapshotLocal | SnapshotTiming | SnapshotWorld | SnapshotBomb | SnapshotWebRadar
    };
}

static EntitySnapshot s_entityBuf[2];
static std::atomic<int> s_readIdx{0};

static esp::PlayerData s_players[64];
static esp::PlayerData s_prevPlayers[64];
static esp::PlayerData s_webRadarPlayers[64];
static char       s_localName[128] = {};
static int        s_localPlayerIndex = -1;
static int        s_localTeam = 0;
static uintptr_t  s_localPawn = 0;
static Vector3    s_prevLocalPos = {};
static view_matrix_t s_viewMatrix = {};
static Vector3    s_localPos = {};
static bool       s_localIsDead = false;
static int        s_localHealth = 0;
static int        s_localArmor = 0;
static int        s_localMoney = 0;
static uint16_t   s_localWeaponId = 0;
static int        s_localAmmoClip = -1;
static bool       s_localHasBomb = false;
static bool       s_localHasDefuser = false;
static uint16_t   s_localGrenadeIds[esp::PlayerData::kMaxGrenades] = {};
static int        s_localGrenadeCount = 0;
static Vector3    s_viewAngles = {};
static float      s_sensitivity = 1.0f;
static Vector3    s_minimapMins = {};
static Vector3    s_minimapMaxs = {};
static bool       s_hasMinimapBounds = false;
static bool       s_localMaskResolved = false;
static uint64_t   s_captureTimeUs = 0;
static uint64_t   s_prevCaptureTimeUs = 0;
static uint64_t   s_playerLastSeenMs[64] = {};
static uint8_t    s_playerInvalidReadStreak[64] = {};
static uint8_t    s_playerDeathConfirmCount[64] = {};
static constexpr uint64_t STALE_TIMEOUT_MS = 6000;
static Vector3    s_prevRawPlayerPos[64] = {};
static bool       s_prevRawPlayerPosReady[64] = {};

static std::mutex s_dataMutex;
static std::atomic<uint64_t> s_sceneResetSerial{1};
static std::atomic<uint64_t> s_lastSceneResetUs{0};
static std::atomic<uint8_t> s_sceneWarmupState{static_cast<uint8_t>(esp::SceneWarmupState::ColdAttach)};
static std::atomic<uint64_t> s_sceneWarmupEnteredUs{0};
static std::atomic<uint64_t> s_mapEpoch{1};
static std::atomic<uint64_t> s_bombEpoch{1};
static uint64_t s_mapFingerprint = 0;

static std::mutex    s_cameraMutex;
static view_matrix_t s_liveViewMatrix = {};
static Vector3       s_liveViewAngles = {};
static Vector3       s_liveLocalPos = {};
static bool          s_liveViewValid = false;
static bool          s_liveLocalPosValid = false;
static uint64_t      s_liveViewUpdatedUs = 0;
static uint64_t      s_liveLocalPosUpdatedUs = 0;
static constexpr uint64_t kLiveCameraFreshnessUs = 125000;
static constexpr uint32_t kCameraInvalidateMissThreshold = 40;
static constexpr uint32_t kCameraRecoveryMissThreshold = 180;

static WorldMarker s_worldMarkers[256] = {};
static int s_worldMarkerCount = 0;
static BombState s_bombState = {};
static uint64_t s_lastWorldScanUs = 0;
static constexpr int kMaxTrackedWorldEntities = 8191;
static constexpr int kMaxTrackedWorldBlocks = (kMaxTrackedWorldEntities >> 9) + 1;
static constexpr int kTrackedWorldSubclassSlots = 8;
static uintptr_t s_worldEntityRefs[kMaxTrackedWorldEntities + 1] = {};
static uint32_t s_worldEntitySubclassIds[kMaxTrackedWorldEntities + 1] = {};
static uint16_t s_worldEntityItemIds[kMaxTrackedWorldEntities + 1] = {};
static int s_worldTrackedIndices[kMaxTrackedWorldEntities + 1] = {};
static uint16_t s_worldTrackedIndexPos[kMaxTrackedWorldEntities + 1] = {};
static int s_worldTrackedIndexCount = 0;
static uint32_t s_worldSmokeSubclassIds[kTrackedWorldSubclassSlots] = {};
static uint32_t s_worldMolotovSubclassIds[kTrackedWorldSubclassSlots] = {};
static uint32_t s_worldDecoySubclassIds[kTrackedWorldSubclassSlots] = {};
static uint32_t s_worldHeSubclassIds[kTrackedWorldSubclassSlots] = {};
static uint32_t s_worldInfernoSubclassIds[kTrackedWorldSubclassSlots] = {};
static bool s_worldSmokeLatched[kMaxTrackedWorldEntities + 1] = {};
static bool s_worldInfernoLatched[kMaxTrackedWorldEntities + 1] = {};
static bool s_worldDecoyLatched[kMaxTrackedWorldEntities + 1] = {};
static bool s_worldExplosiveLatched[kMaxTrackedWorldEntities + 1] = {};
static bool s_worldUtilityHasHistory[kMaxTrackedWorldEntities + 1] = {};
static uint8_t s_worldSmokeEvidenceCount[kMaxTrackedWorldEntities + 1] = {};
static uint8_t s_worldInfernoEvidenceCount[kMaxTrackedWorldEntities + 1] = {};
static uint8_t s_worldDecoyEvidenceCount[kMaxTrackedWorldEntities + 1] = {};
static uint8_t s_worldExplosiveEvidenceCount[kMaxTrackedWorldEntities + 1] = {};
static uint64_t s_worldSmokeStartUs[kMaxTrackedWorldEntities + 1] = {};
static uint64_t s_worldInfernoStartUs[kMaxTrackedWorldEntities + 1] = {};
static uint64_t s_worldDecoyStartUs[kMaxTrackedWorldEntities + 1] = {};
static uint64_t s_worldExplosiveStartUs[kMaxTrackedWorldEntities + 1] = {};
static Vector3 s_worldPrevPos[kMaxTrackedWorldEntities + 1] = {};
static int s_worldPrevSmokeTick[kMaxTrackedWorldEntities + 1] = {};
static uint8_t s_worldPrevSmokeActive[kMaxTrackedWorldEntities + 1] = {};
static uint8_t s_worldPrevSmokeVolumeDataReceived[kMaxTrackedWorldEntities + 1] = {};
static uint8_t s_worldPrevSmokeEffectSpawned[kMaxTrackedWorldEntities + 1] = {};
static int s_worldPrevInfernoTick[kMaxTrackedWorldEntities + 1] = {};
static float s_worldPrevInfernoLife[kMaxTrackedWorldEntities + 1] = {};
static int s_worldPrevInfernoFireCount[kMaxTrackedWorldEntities + 1] = {};
static uint8_t s_worldPrevInfernoInPostEffect[kMaxTrackedWorldEntities + 1] = {};
static int s_worldPrevDecoyTick[kMaxTrackedWorldEntities + 1] = {};
static int s_worldPrevDecoyClientTick[kMaxTrackedWorldEntities + 1] = {};
static int s_worldPrevExplodeTick[kMaxTrackedWorldEntities + 1] = {};
static Vector3 s_worldPrevVelocity[kMaxTrackedWorldEntities + 1] = {};
static float s_lastStableIntervalPerTick = 0.015625f;
static float s_lastStableGameTime = 0.0f;
static uint64_t s_lastStableGameTimeUs = 0;

static std::thread s_dataWorker;
static std::thread s_cameraWorker;
static std::atomic<bool> s_dataWorkerRunning = false;
static std::atomic<bool> s_cameraWorkerRunning = false;
static std::atomic<bool> s_dataWorkerStopRequested = false;
static constexpr int CAMERA_WORKER_HZ = 500;
static std::atomic<bool> s_dmaRecovering = false;
static std::atomic<bool> s_dmaRecoveryRequested = false;
static std::atomic<uint64_t> s_dmaRecoveryRequestedAtUs = 0;
static std::atomic<uint32_t> s_dmaConsecutiveFailures = 0;
static std::atomic<uint64_t> s_dmaTotalFailures = 0;
static std::atomic<uint64_t> s_dmaTotalSuccesses = 0;
static std::atomic<uint64_t> s_dmaTotalRecoveries = 0;
static std::atomic<uint64_t> s_dmaLastSuccessTick = 0;
static std::atomic<uint64_t> s_publishCount = 0;
static std::atomic<uint64_t> s_lastPublishUs = 0;
static std::atomic<uint64_t> s_sessionStartUs = 0;
static std::atomic<uint64_t> s_dataWorkerCycleUs = 0;     
static std::atomic<uint64_t> s_dataWorkerMaxCycleUs = 0;   
static std::atomic<uint64_t> s_dataWorkerLastLoopStartUs = 0;
static std::atomic<uint64_t> s_dataWorkerLastLoopEndUs = 0;
static std::atomic<uint64_t> s_dataWorkerInFlightSinceUs = 0;
static std::atomic<bool> s_dataWorkerUpdateInFlight = false;
static std::atomic<uint64_t> s_cameraWorkerCycleUs = 0;
static std::atomic<uint64_t> s_cameraWorkerMaxCycleUs = 0;
static std::atomic<int32_t>  s_activePlayerCount = 0;
static std::atomic<int32_t>  s_playerSlotScanLimitStat = 64;
static std::atomic<int32_t>  s_playerHierarchyHighWaterSlot = 0;
static std::atomic<int32_t>  s_highestEntityIdxStat = 0;
static std::atomic<int32_t>  s_worldMarkerCountStat = 0;
static std::atomic<uint64_t> s_lastWorldScanCommittedUs = 0;
static std::atomic<uint32_t> s_bombDebugFlags = 0;
static std::atomic<uint32_t> s_bombDebugSourceFlags = 0;
static std::atomic<uint8_t>  s_bombDebugConfidence = 0;

static std::atomic<uint64_t> s_stageEngineUs = 0;
static std::atomic<uint64_t> s_stageBaseReadsUs = 0;
static std::atomic<uint64_t> s_stagePlayerReadsUs = 0;
static std::atomic<uint64_t> s_stagePlayerHierarchyUs = 0;
static std::atomic<uint64_t> s_stagePlayerCoreUs = 0;
static std::atomic<uint64_t> s_stagePlayerRepairUs = 0;
static std::atomic<uint64_t> s_stageCommitStateUs = 0;
static std::atomic<uint64_t> s_stagePlayerAuxUs = 0;
static std::atomic<uint64_t> s_stageInventoryUs = 0;
static std::atomic<uint64_t> s_stageBoneReadsUs = 0;
static std::atomic<uint64_t> s_stageBombScanUs = 0;
static std::atomic<uint64_t> s_stageWorldScanUs = 0;
static std::atomic<uint64_t> s_stageWorldScanLastUs = 0;
static std::atomic<uint64_t> s_stageCommitEnrichUs = 0;
static std::atomic<uint64_t> s_stagePlayerAuxLastUs = 0;
static std::atomic<uint64_t> s_stageInventoryLastUs = 0;
static std::atomic<uint64_t> s_stageBoneReadsLastUs = 0;
static std::atomic<uint64_t> s_stagePlayerAuxLastAtUs = 0;
static std::atomic<uint64_t> s_stageInventoryLastAtUs = 0;
static std::atomic<uint64_t> s_stageBoneReadsLastAtUs = 0;
static std::atomic<uint8_t> s_gameStatus = static_cast<uint8_t>(esp::GameStatus::WaitCs2);
static std::atomic<bool> s_engineStatusResolved = false;
static std::atomic<int32_t> s_engineSignOnState = -1;
static std::atomic<int32_t> s_engineLocalPlayerSlot = -1;
static std::atomic<int32_t> s_engineMaxClients = 0;
static std::atomic<bool> s_engineBackgroundMap = false;
static std::atomic<bool> s_engineMenu = false;
static std::atomic<bool> s_engineInGame = false;
static constexpr uint64_t kDataWorkerStallUs = 250000;


static std::string s_activeMapKey;
static float s_lastSavedMapRotation = 0.0f;
static float s_lastSavedMapScale = 1.0f;
static float s_lastSavedMapOffsetX = 0.0f;
static float s_lastSavedMapOffsetY = 0.0f;
static float s_activeMapBaseOffsetX = 0.0f;
static float s_activeMapBaseOffsetY = 0.0f;
static bool s_activeMapOverviewAvailable = false;
static float s_activeMapOverviewPosX = 0.0f;
static float s_activeMapOverviewPosY = 0.0f;
static float s_activeMapOverviewScale = 0.0f;
static auto s_lastMapPersistTime = std::chrono::steady_clock::time_point();

struct BonePair { int from, to; };

static const BonePair skeletonPairs[] = {
    { esp::PELVIS,     esp::SPINE1 },
    { esp::SPINE1,     esp::SPINE2 },
    { esp::SPINE2,     esp::CHEST },
    { esp::CHEST,      esp::NECK },
    { esp::NECK,       esp::HEAD },
    { esp::NECK,       esp::SHOULDER_L },
    { esp::SHOULDER_L, esp::ELBOW_L },
    { esp::ELBOW_L,    esp::HAND_L },
    { esp::NECK,       esp::SHOULDER_R },
    { esp::SHOULDER_R, esp::ELBOW_R },
    { esp::ELBOW_R,    esp::HAND_R },
    { esp::PELVIS,     esp::HIP_L },
    { esp::HIP_L,      esp::KNEE_L },
    { esp::KNEE_L,     esp::FOOT_HEEL_L },
    { esp::PELVIS,     esp::HIP_R },
    { esp::HIP_R,      esp::KNEE_R },
    { esp::KNEE_R,     esp::FOOT_HEEL_R },
};
static constexpr int skeletonPairCount = static_cast<int>(std::size(skeletonPairs));

static constexpr int DATA_WORKER_HZ = 300;

static void PublishCurrentSnapshot(uint32_t mask = SnapshotAll)
{
    const int readIdx = s_readIdx.load(std::memory_order_acquire);
    const int writeIdx = 1 - readIdx;
    const EntitySnapshot& base = s_entityBuf[readIdx];
    EntitySnapshot& snap = s_entityBuf[writeIdx];

    if (mask != SnapshotAll)
        snap = base;

    if ((mask & SnapshotPlayers) != 0u) {
        memcpy(snap.players, s_players, sizeof(snap.players));
        memcpy(snap.prevPlayers, s_prevPlayers, sizeof(snap.prevPlayers));
    }

    if ((mask & SnapshotWebRadar) != 0u)
        memcpy(snap.webRadarPlayers, s_webRadarPlayers, sizeof(snap.webRadarPlayers));

    if ((mask & SnapshotLocalView) != 0u) {
        memcpy(snap.localName, s_localName, sizeof(snap.localName));
        memset(snap.activeMapKey, 0, sizeof(snap.activeMapKey));
        if (!s_activeMapKey.empty())
            strncpy_s(snap.activeMapKey, sizeof(snap.activeMapKey), s_activeMapKey.c_str(), _TRUNCATE);
        snap.localPlayerIndex = s_localPlayerIndex;
        snap.localTeam = s_localTeam;
        snap.localPawn = s_localPawn;
        snap.localPos = s_localPos;
        snap.prevLocalPos = s_prevLocalPos;
        snap.localIsDead = s_localIsDead;
        snap.localHealth = s_localHealth;
        snap.localArmor = s_localArmor;
        snap.localMoney = s_localMoney;
        memcpy(&snap.viewMatrix, &s_viewMatrix, sizeof(view_matrix_t));
        snap.viewAngles = s_viewAngles;
        snap.minimapMins = s_minimapMins;
        snap.minimapMaxs = s_minimapMaxs;
        snap.hasMinimapBounds = s_hasMinimapBounds;
        snap.localMaskResolved = s_localMaskResolved;
    }

    if ((mask & SnapshotLocalLoadout) != 0u) {
        snap.localWeaponId = s_localWeaponId;
        snap.localAmmoClip = s_localAmmoClip;
        snap.localHasBomb = s_localHasBomb;
        snap.localHasDefuser = s_localHasDefuser;
        snap.localGrenadeCount = s_localGrenadeCount;
        memcpy(snap.localGrenadeIds, s_localGrenadeIds, sizeof(snap.localGrenadeIds));
    }

    if ((mask & SnapshotTiming) != 0u) {
        snap.captureTimeUs = s_captureTimeUs;
        snap.prevCaptureTimeUs = s_prevCaptureTimeUs;
    }

    if ((mask & SnapshotWorld) != 0u) {
        snap.worldMarkerCount = s_worldMarkerCount;
        if (snap.worldMarkerCount > 256)
            snap.worldMarkerCount = 256;
        memcpy(snap.worldMarkers, s_worldMarkers, sizeof(snap.worldMarkers));
    }

    if ((mask & SnapshotBomb) != 0u)
        snap.bombState = s_bombState;

    s_readIdx.store(writeIdx, std::memory_order_release);
    s_publishCount.fetch_add(1, std::memory_order_relaxed);
    
}

static void ResetCameraSnapshot()
{
    std::lock_guard<std::mutex> lock(s_cameraMutex);
    memset(&s_liveViewMatrix, 0, sizeof(s_liveViewMatrix));
    s_liveViewAngles = {};
    s_liveLocalPos = {};
    s_liveViewValid = false;
    s_liveLocalPosValid = false;
    s_liveViewUpdatedUs = 0;
    s_liveLocalPosUpdatedUs = 0;
}

static int ResolveLocalPlayerIndex(int localControllerMaskBit, int localMaskBit)
{
    if (localMaskBit >= 0 && localMaskBit < 64)
        return localMaskBit;
    if (localControllerMaskBit > 0 && localControllerMaskBit <= 64)
        return localControllerMaskBit - 1;
    const int engineLocalPlayerSlot = s_engineLocalPlayerSlot.load(std::memory_order_relaxed);
    return (engineLocalPlayerSlot >= 0 && engineLocalPlayerSlot < 64)
        ? engineLocalPlayerSlot
        : -1;
}

enum class LocalPlayerIndexSource : uint8_t
{
    None = 0,
    PawnMatch,
    ControllerMatch,
    EngineFallback,
};

static LocalPlayerIndexSource ResolveLocalPlayerIndexSource(int localControllerMaskBit, int localMaskBit)
{
    if (localMaskBit >= 0 && localMaskBit < 64)
        return LocalPlayerIndexSource::PawnMatch;
    if (localControllerMaskBit > 0 && localControllerMaskBit <= 64)
        return LocalPlayerIndexSource::ControllerMatch;
    const int engineLocalPlayerSlot = s_engineLocalPlayerSlot.load(std::memory_order_relaxed);
    return (engineLocalPlayerSlot >= 0 && engineLocalPlayerSlot < 64)
        ? LocalPlayerIndexSource::EngineFallback
        : LocalPlayerIndexSource::None;
}

static bool IsLiveLocalPlayerIndexSource(LocalPlayerIndexSource source)
{
    return source == LocalPlayerIndexSource::PawnMatch ||
           source == LocalPlayerIndexSource::ControllerMatch;
}

static bool IsValidLocalPlayerIndex(int localPlayerIndex)
{
    return localPlayerIndex >= 0 && localPlayerIndex < 64;
}

static void ResolveSharedLocalIdentityFromSlot(
    int localPlayerIndex,
    bool localPlayerIndexValid,
    const char (&names)[64][128],
    const int (&healths)[64],
    const uint8_t (&lifeStates)[64],
    const int (&armors)[64],
    const int (&moneys)[64],
    const uint8_t (&hasDefuserFlags)[64],
    char (&localNameResolved)[128],
    bool& localIsDeadResolved,
    int& localHealthResolved,
    int& localArmorResolved,
    int& localMoneyResolved,
    bool& localHasDefuserResolved)
{
    if (!localPlayerIndexValid)
        return;

    memcpy(localNameResolved, names[localPlayerIndex], sizeof(localNameResolved));
    localNameResolved[127] = '\0';
    localIsDeadResolved = (healths[localPlayerIndex] <= 0) || (lifeStates[localPlayerIndex] != 0);
    localHealthResolved = localIsDeadResolved ? 0 : std::clamp(healths[localPlayerIndex], 0, 100);
    localArmorResolved = std::clamp(armors[localPlayerIndex], 0, 100);
    localMoneyResolved = std::max(0, moneys[localPlayerIndex]);
    localHasDefuserResolved = hasDefuserFlags[localPlayerIndex] != 0;
}

static void ApplySharedLocalIdentityState(
    const char (&localNameResolved)[128],
    bool localIsDeadResolved,
    int localHealthResolved,
    int localArmorResolved,
    int localMoneyResolved,
    bool localHasDefuserResolved)
{
    std::copy(std::begin(localNameResolved), std::end(localNameResolved), std::begin(s_localName));
    s_localIsDead = localIsDeadResolved;
    s_localHealth = localHealthResolved;
    s_localArmor = localArmorResolved;
    s_localMoney = localMoneyResolved;
    s_localHasDefuser = localHasDefuserResolved;
}

float esp::GetSensitivity()
{
    std::lock_guard<std::mutex> lock(s_dataMutex);
    return s_sensitivity;
}

uint64_t esp::GetPublishCount()
{
    return s_publishCount.load(std::memory_order_acquire);
}

static void RequestDmaRecovery(const char* reason)
{
    const uint64_t nowUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    s_sceneWarmupState.store(static_cast<uint8_t>(esp::SceneWarmupState::Recovery), std::memory_order_relaxed);
    s_sceneWarmupEnteredUs.store(nowUs, std::memory_order_relaxed);
    s_dmaRecoveryRequested.store(true, std::memory_order_release);
    s_dmaRecoveryRequestedAtUs.store(nowUs, std::memory_order_relaxed);

    static uint64_t s_lastRecoveryRequestLogUs = 0;
    if (nowUs - s_lastRecoveryRequestLogUs >= 750000) {
        s_lastRecoveryRequestLogUs = nowUs;
        DmaLogPrintf("[INFO] DMA recovery requested (%s)", reason ? reason : "unspecified");
    }
}

void esp::RequestCacheRefresh()
{
    
    
    
    
    
    
    
    
    s_bombEpoch.fetch_add(1, std::memory_order_relaxed);
    s_sceneResetSerial.fetch_add(1, std::memory_order_relaxed);
    RequestDmaRecovery("user_requested_refresh");
}

static bool IsDmaRecoveryRequested()
{
    return s_dmaRecoveryRequested.load(std::memory_order_acquire);
}

static void ClearDmaRecoveryRequest()
{
    s_dmaRecoveryRequested.store(false, std::memory_order_release);
    s_dmaRecoveryRequestedAtUs.store(0, std::memory_order_relaxed);
}
static constexpr uint32_t RECOVERY_FAILURE_THRESHOLD = 120; 
