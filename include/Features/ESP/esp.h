#pragma once
#include "Game/Schema/structs.h"
#include <array>
#include <cstdint>

namespace esp {

    inline constexpr int kPlayerStoredBoneIds[] = { 0, 2, 4, 5, 6, 8, 9, 10, 13, 14, 15, 22, 23, 24, 25, 26, 27 };
    inline constexpr int kPlayerStoredBoneCount = static_cast<int>(std::size(kPlayerStoredBoneIds));

    constexpr int PlayerStoredBoneIndex(int boneId)
    {
        switch (boneId) {
        case 0: return 0;
        case 2: return 1;
        case 4: return 2;
        case 5: return 3;
        case 6: return 4;
        case 8: return 5;
        case 9: return 6;
        case 10: return 7;
        case 13: return 8;
        case 14: return 9;
        case 15: return 10;
        case 22: return 11;
        case 23: return 12;
        case 24: return 13;
        case 25: return 14;
        case 26: return 15;
        case 27: return 16;
        default: return -1;
        }
    }

    enum class GameStatus : uint8_t {
        Ok = 0,       
        WaitCs2 = 2,  
    };

    enum class SceneWarmupState : uint8_t {
        ColdAttach = 0,
        SceneTransition,
        HierarchyWarming,
        Stable,
        Recovery,
    };

    struct DmaHealthStats {
        bool     workerRunning = false;
        bool     cameraWorkerRunning = false;
        bool     dataWorkerInFlight = false;
        bool     dataWorkerStalled = false;
        bool     recovering = false;
        bool     recoveryRequested = false;
        uint32_t consecutiveFailures = 0;
        uint64_t totalFailures = 0;
        uint64_t totalSuccesses = 0;
        uint64_t totalRecoveries = 0;
        uint64_t lastSuccessAgeMs = 0;
        uint64_t recoveryRequestAgeMs = 0;
        uint64_t dataWorkerLoopAgeMs = 0;
        uint64_t dataWorkerInFlightAgeMs = 0;
        GameStatus gameStatus = GameStatus::WaitCs2;
    };

    
    struct StageTiming {
        uint64_t engineUs = 0;
        uint64_t baseReadsUs = 0;
        uint64_t playerReadsUs = 0;
        uint64_t playerHierarchyUs = 0;
        uint64_t playerCoreUs = 0;
        uint64_t playerRepairUs = 0;
        uint64_t commitStateUs = 0;
        uint64_t playerAuxUs = 0;
        uint64_t inventoryUs = 0;
        uint64_t boneReadsUs = 0;
        uint64_t bombScanUs = 0;
        uint64_t worldScanUs = 0;
        uint64_t worldScanLastUs = 0;
        uint64_t commitEnrichUs = 0;
        uint64_t playerAuxLastUs = 0;
        uint64_t inventoryLastUs = 0;
        uint64_t boneReadsLastUs = 0;
        uint64_t playerAuxAgeUs = 0;
        uint64_t inventoryAgeUs = 0;
        uint64_t boneReadsAgeUs = 0;
        uint64_t totalUs = 0;
        uint64_t totalHeldUs = 0;
    };

    struct CameraTiming {
        uint64_t cycleUs = 0;
        uint64_t maxCycleUs = 0;
    };

    struct OverlayTiming {
        uint64_t frameUs = 0;
        uint64_t maxFrameUs = 0;
        uint64_t syncUs = 0;
        uint64_t drawUs = 0;
        uint64_t presentUs = 0;
        uint64_t pacingWaitUs = 0;
    };

    struct DebugStats {
        uint64_t publishCount = 0;
        uint64_t lastPublishUs = 0;
        uint64_t cycleUs = 0;
        uint64_t maxCycleUs = 0;
        int32_t  activePlayers = 0;
        int32_t  playerSlotBudget = 64;
        int32_t  engineMaxClients = 0;
        int32_t  highestEntityIdx = 0;
        int32_t  worldMarkerCount = 0;
        uint64_t uptimeUs = 0;
        uint64_t sessionUptimeUs = 0;
        uint64_t worldScanAgeUs = 0;
        uint64_t worldScanTargetIntervalUs = 0;
        uint64_t cameraViewAgeUs = 0;
        uint64_t cameraLocalPosAgeUs = 0;
        bool     liveViewValid = false;
        bool     liveViewFresh = false;
        bool     liveLocalPosValid = false;
        bool     liveLocalPosFresh = false;
        bool     engineMenu = false;
        bool     engineInGame = false;
        uint64_t sceneEpoch = 0;
        uint64_t mapEpoch = 0;
        uint64_t bombEpoch = 0;
        uint64_t warmupAgeUs = 0;
        SceneWarmupState warmupState = SceneWarmupState::ColdAttach;
        bool     bombPlanted = false;
        bool     bombTicking = false;
        bool     bombBeingDefused = false;
        bool     bombDropped = false;
        bool     bombBoundsValid = false;
        uint32_t bombSourceFlags = 0;
        uint8_t  bombConfidence = 0;
        CameraTiming camera = {};
        OverlayTiming overlay = {};
        StageTiming stages = {};
    };

    struct PlayerData {
        bool     valid = false;
        uintptr_t pawn = 0;
        int      health = 0;
        int      armor = 0;
        int      team = 0;
        int      money = 0;
        int      ping = 0;
        Vector3  position;
        Vector3  velocity;
        char     name[128] = {};
        Vector3  bones[kPlayerStoredBoneCount] = {};
        bool     hasBones = false;
        bool     visible = false;
        bool     scoped = false;
        bool     defusing = false;
        bool     hasDefuser = false;
        bool     flashed = false;
        uint16_t weaponId = 0;
        uint16_t weaponIconId = 0;
        int      ammoClip = -1;
        bool     hasBomb = false;
        float    flashDuration = 0.0f;
        float    eyeYaw = 0.0f;
        uint64_t spottedMask = 0;
        uint64_t soundUntilMs = 0;
        int      staleFrames = 0;
        
        static constexpr int kMaxGrenades = 4;
        uint16_t grenadeIds[kMaxGrenades] = {};
        int      grenadeCount = 0;
    };

    struct BombSnapshot {
        bool     planted = false;
        bool     ticking = false;
        bool     beingDefused = false;
        bool     dropped = false;
        Vector3  position = {};
        float    blowTime = 0.0f;
        float    defuseEndTime = 0.0f;
        float    currentGameTime = 0.0f;
    };

    struct WebRadarWorldMarker {
        uint8_t  type = 0;        
        Vector3  position = {};
        uint16_t weaponId = 0;
        float    lifeRemainingSec = 0.0f;
    };

    struct WebRadarSnapshot {
        std::array<PlayerData, 64> players = {};
        char localName[128] = {};
        Vector3 localPos = {};
        bool localIsDead = false;
        int localHealth = 0;
        int localArmor = 0;
        int localMoney = 0;
        float localYaw = 0.0f;
        uint16_t localWeaponId = 0;
        int localAmmoClip = -1;
        bool localHasBomb = false;
        bool localHasDefuser = false;
        uint16_t localGrenadeIds[PlayerData::kMaxGrenades] = {};
        int localGrenadeCount = 0;
        Vector3 minimapMins = {};
        Vector3 minimapMaxs = {};
        BombSnapshot bomb = {};
        int localTeam = 0;
        bool hasMinimapBounds = false;
        uint64_t captureTickMs = 0;
        static constexpr int kMaxWorldMarkers = 64;
        WebRadarWorldMarker worldMarkers[kMaxWorldMarkers] = {};
        int worldMarkerCount = 0;
    };
    void RequestCacheRefresh();

    bool UpdateData();

    void StartDataWorker();
    void StopDataWorker();
    DmaHealthStats GetDmaHealthStats();
    DebugStats     GetDebugStats();

    void Draw();

    float       GetSensitivity();
    uint64_t    GetPublishCount();
    bool        GetWebRadarSnapshot(WebRadarSnapshot* outSnapshot);
}
