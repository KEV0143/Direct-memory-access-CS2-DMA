#pragma once

#include <cstddef>
#include <string>

namespace runtime_offsets
{
    struct PatchInfo
    {
        std::string etag;
        std::string lastFileSha;
        int clientVersion = 0;
        int sourceRevision = 0;
        std::string patchVersion;
    };

    struct AutoUpdateReport
    {
        PatchInfo currentPatch = {};
        PatchInfo previousOffsetsPatch = {};
        PatchInfo previousLastSeenPatch = {};
        bool patchChanged = false;
        bool offsetsUpdated = false;
        bool offsetsCompatibleWithCurrentPatch = false;
        bool offsetSourcePredatesCurrentPatch = false;
        std::string offsetSource;
        std::string offsetSourceTimestamp;
        int offsetSourceBuildNumber = 0;
        std::string currentPatchVersionDate;
        std::string currentPatchVersionTime;
    };

    struct StateView
    {
        PatchInfo offsetsPatch = {};
        PatchInfo lastSeenPatch = {};
        std::string selectedSource;
        std::string selectedSourceTimestamp;
        int selectedSourceBuildNumber = 0;
    };

    struct Values
    {
        
        std::ptrdiff_t dwEntityList = 0;
        std::ptrdiff_t dwGameEntitySystem_highestEntityIndex = 0;
        std::ptrdiff_t dwGameRules = 0;
        std::ptrdiff_t dwGlobalVars = 0;
        std::ptrdiff_t dwLocalPlayerController = 0;
        std::ptrdiff_t dwLocalPlayerPawn = 0;
        std::ptrdiff_t dwPlantedC4 = 0;
        std::ptrdiff_t dwViewMatrix = 0;
        std::ptrdiff_t dwViewAngles = 0;
        std::ptrdiff_t dwWeaponC4 = 0;
        std::ptrdiff_t dwSensitivity = 0;
        std::ptrdiff_t dwSensitivity_sensitivity = 0;
        std::ptrdiff_t dwNetworkGameClient = 0;
        std::ptrdiff_t dwNetworkGameClient_signOnState = 0;
        std::ptrdiff_t dwNetworkGameClient_localPlayer = 0;
        std::ptrdiff_t dwNetworkGameClient_maxClients = 0;
        std::ptrdiff_t dwNetworkGameClient_isBackgroundMap = 0;

        
        std::ptrdiff_t CBasePlayerController_m_iPing = 0;
        std::ptrdiff_t CCSPlayerController_m_pInGameMoneyServices = 0;
        std::ptrdiff_t CCSPlayerController_InGameMoneyServices_m_iAccount = 0;
        std::ptrdiff_t C_BaseEntity_m_iTeamNum = 0;
        std::ptrdiff_t C_BasePlayerPawn_m_vOldOrigin = 0;
        std::ptrdiff_t C_BasePlayerPawn_m_pWeaponServices = 0;
        std::ptrdiff_t CCSPlayerController_m_hPlayerPawn = 0;
        std::ptrdiff_t CBasePlayerController_m_iszPlayerName = 0;
        std::ptrdiff_t CPlayer_WeaponServices_m_hActiveWeapon = 0;
        std::ptrdiff_t CPlayer_WeaponServices_m_hMyWeapons = 0;
        std::ptrdiff_t C_BaseEntity_m_iHealth = 0;
        std::ptrdiff_t C_CSPlayerPawn_m_ArmorValue = 0;
        std::ptrdiff_t C_BaseEntity_m_lifeState = 0;
        std::ptrdiff_t C_BaseEntity_m_pGameSceneNode = 0;
        std::ptrdiff_t C_BaseEntity_m_hOwnerEntity = 0;
        std::ptrdiff_t C_BaseModelEntity_m_Collision = 0;
        std::ptrdiff_t CCollisionProperty_m_vecMins = 0;
        std::ptrdiff_t CCollisionProperty_m_vecMaxs = 0;
        std::ptrdiff_t C_BaseEntity_m_nSubclassID = 0;
        std::ptrdiff_t C_BaseEntity_m_vecVelocity = 0;
        std::ptrdiff_t C_BasePlayerWeapon_m_iClip1 = 0;
        std::ptrdiff_t C_CSPlayerPawn_m_pClippingWeapon = 0;
        std::ptrdiff_t C_CSPlayerPawn_m_bIsScoped = 0;
        std::ptrdiff_t C_CSPlayerPawn_m_bIsDefusing = 0;
        std::ptrdiff_t C_CSPlayerPawn_m_angEyeAngles = 0;
        std::ptrdiff_t C_CSPlayerPawnBase_m_iCompTeammateColor = 0;
        std::ptrdiff_t C_CSPlayerPawnBase_m_flFlashDuration = 0;
        std::ptrdiff_t C_CSPlayerPawnBase_m_pItemServices = 0;
        std::ptrdiff_t CCSPlayer_ItemServices_m_bHasDefuser = 0;
        std::ptrdiff_t C_CSPlayerPawn_m_entitySpottedState = 0;
        std::ptrdiff_t C_EconEntity_m_AttributeManager = 0;
        std::ptrdiff_t C_AttributeContainer_m_Item = 0;
        std::ptrdiff_t C_EconItemView_m_iItemDefinitionIndex = 0;
        std::ptrdiff_t C_CSGameRules_m_vMinimapMins = 0;
        std::ptrdiff_t C_CSGameRules_m_vMinimapMaxs = 0;
        std::ptrdiff_t C_CSGameRules_m_bBombPlanted = 0;
        std::ptrdiff_t C_CSGameRules_m_bBombDropped = 0;
        std::ptrdiff_t CGameSceneNode_m_vecAbsOrigin = 0;
        std::ptrdiff_t EntitySpottedState_t_m_bSpottedByMask = 0;
        std::ptrdiff_t EntitySpottedState_t_m_bSpotted = 0;
        std::ptrdiff_t C_PlantedC4_m_bBombTicking = 0;
        std::ptrdiff_t C_PlantedC4_m_flC4Blow = 0;
        std::ptrdiff_t C_PlantedC4_m_bBeingDefused = 0;
        std::ptrdiff_t C_PlantedC4_m_flDefuseCountDown = 0;
        std::ptrdiff_t C_Inferno_m_nFireEffectTickBegin = 0;
        std::ptrdiff_t C_Inferno_m_nFireLifetime = 0;
        std::ptrdiff_t C_Inferno_m_fireCount = 0;
        std::ptrdiff_t C_Inferno_m_bInPostEffectTime = 0;
        std::ptrdiff_t C_SmokeGrenadeProjectile_m_nSmokeEffectTickBegin = 0;
        std::ptrdiff_t C_SmokeGrenadeProjectile_m_bDidSmokeEffect = 0;
        std::ptrdiff_t C_SmokeGrenadeProjectile_m_bSmokeVolumeDataReceived = 0;
        std::ptrdiff_t C_SmokeGrenadeProjectile_m_bSmokeEffectSpawned = 0;
        std::ptrdiff_t C_DecoyProjectile_m_nDecoyShotTick = 0;
        std::ptrdiff_t C_DecoyProjectile_m_nClientLastKnownDecoyShotTick = 0;
        std::ptrdiff_t C_BaseCSGrenadeProjectile_m_nExplodeEffectTickBegin = 0;
        std::ptrdiff_t CSkeletonInstance_m_modelState = 0;
    };

    const Values& Get();
    StateView GetStateView();
    bool HasOffsetsFile();
    bool AutoUpdateFromGitHub(std::string* message = nullptr, AutoUpdateReport* report = nullptr);
    bool Load(std::string* message = nullptr);
}
