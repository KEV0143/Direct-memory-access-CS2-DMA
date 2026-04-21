#include "Game/Offsets/runtime_offsets.h"
#include "app/Config/project_paths.h"
#include "app/Core/build_info.h"

#include <Windows.h>
#include <winhttp.h>
#include <wincrypt.h>

#include <json/json.hpp>

#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "Crypt32.lib")

namespace
{
    using json = nlohmann::json;
    using ordered_json = nlohmann::ordered_json;
    using LegacyIniSection = std::unordered_map<std::string, std::string>;
    using LegacyIniDocument = std::unordered_map<std::string, LegacyIniSection>;

    struct OffsetField
    {
        const char* section;
        const char* key;
        std::ptrdiff_t runtime_offsets::Values::*member;
    };

    struct RemoteField
    {
        const char* remoteKey;
        const char* sourceFile;
        std::ptrdiff_t runtime_offsets::Values::*member;
    };

    struct OutputDirectoryCandidate
    {
        std::filesystem::path directory;
        std::string label;
        std::string timestamp;
        int buildNumber = 0;
        bool hasRequiredFiles = false;
    };

    struct OffsetState
    {
        runtime_offsets::PatchInfo offsetsPatch = {};
        runtime_offsets::PatchInfo lastSeenPatch = {};
        std::string selectedSource;
        std::string selectedSourceTimestamp;
        std::string remoteOutputTimestamp;
        int selectedSourceBuildNumber = 0;
    };

    struct SteamInfSnapshot
    {
        runtime_offsets::PatchInfo patch = {};
        std::string versionDate;
        std::string versionTime;
        int patchBuildNumber = 0;
    };

    #include "runtime_offsets_parts/runtime_offsets_remote_fields.inl"

    runtime_offsets::Values g_values = {};
    #include "runtime_offsets_parts/runtime_offsets_paths_helpers.inl"

    #include "runtime_offsets_parts/runtime_offsets_parse_helpers.inl"

    runtime_offsets::PatchInfo ReadPatchInfo(const json& root, const char* key)
    {
        runtime_offsets::PatchInfo info = {};
        const auto it = root.find(key);
        if (it == root.end() || !it->is_object())
            return info;

        info.etag = JsonStringOrEmpty(*it, "etag");
        info.lastFileSha = JsonStringOrEmpty(*it, "last_file_sha");
        info.clientVersion = JsonIntOrZero(*it, "client_version");
        info.sourceRevision = JsonIntOrZero(*it, "source_revision");
        info.patchVersion = JsonStringOrEmpty(*it, "patch_version");
        return info;
    }

    template <typename JsonT>
    void WritePatchInfo(JsonT& root, const char* key, const runtime_offsets::PatchInfo& info)
    {
        root[key] = {
            { "etag", info.etag },
            { "last_file_sha", info.lastFileSha },
            { "client_version", info.clientVersion },
            { "source_revision", info.sourceRevision },
            { "patch_version", info.patchVersion }
        };
    }

    bool RemoveFileIfExists(const std::filesystem::path& path);
    bool TryParseOffsetsJsonFile(const std::filesystem::path& jsonPath, json& outRoot);
    void LoadValuesFromJson(const json& root,
                            runtime_offsets::Values& outValues,
                            std::vector<std::string>* missingKeys,
                            std::vector<std::string>* invalidKeys);
    std::vector<std::string> ValidateLoadedValues(const runtime_offsets::Values& values, bool requiredOnly = false);

    const std::vector<OffsetField> kOffsetFields = {
        OffsetField{"offsets", "dwEntityList", &runtime_offsets::Values::dwEntityList},
        OffsetField{"offsets", "dwGameEntitySystem_highestEntityIndex", &runtime_offsets::Values::dwGameEntitySystem_highestEntityIndex},
        OffsetField{"offsets", "dwGameRules", &runtime_offsets::Values::dwGameRules},
        OffsetField{"offsets", "dwGlobalVars", &runtime_offsets::Values::dwGlobalVars},
        OffsetField{"offsets", "dwLocalPlayerController", &runtime_offsets::Values::dwLocalPlayerController},
        OffsetField{"offsets", "dwLocalPlayerPawn", &runtime_offsets::Values::dwLocalPlayerPawn},
        OffsetField{"offsets", "dwPlantedC4", &runtime_offsets::Values::dwPlantedC4},
        OffsetField{"offsets", "dwViewMatrix", &runtime_offsets::Values::dwViewMatrix},
        OffsetField{"offsets", "dwViewAngles", &runtime_offsets::Values::dwViewAngles},
        OffsetField{"offsets", "dwWeaponC4", &runtime_offsets::Values::dwWeaponC4},
        OffsetField{"offsets", "dwSensitivity", &runtime_offsets::Values::dwSensitivity},
        OffsetField{"offsets", "dwSensitivity_sensitivity", &runtime_offsets::Values::dwSensitivity_sensitivity},
        OffsetField{"offsets", "dwNetworkGameClient", &runtime_offsets::Values::dwNetworkGameClient},
        OffsetField{"offsets", "dwNetworkGameClient_signOnState", &runtime_offsets::Values::dwNetworkGameClient_signOnState},
        OffsetField{"offsets", "dwNetworkGameClient_localPlayer", &runtime_offsets::Values::dwNetworkGameClient_localPlayer},
        OffsetField{"offsets", "dwNetworkGameClient_maxClients", &runtime_offsets::Values::dwNetworkGameClient_maxClients},
        OffsetField{"offsets", "dwNetworkGameClient_isBackgroundMap", &runtime_offsets::Values::dwNetworkGameClient_isBackgroundMap},
        OffsetField{"schemas", "CBasePlayerController.m_iPing", &runtime_offsets::Values::CBasePlayerController_m_iPing},
        OffsetField{"schemas", "CCSPlayerController.m_pInGameMoneyServices", &runtime_offsets::Values::CCSPlayerController_m_pInGameMoneyServices},
        OffsetField{"schemas", "CCSPlayerController_InGameMoneyServices.m_iAccount", &runtime_offsets::Values::CCSPlayerController_InGameMoneyServices_m_iAccount},
        OffsetField{"schemas", "C_BaseEntity.m_iTeamNum", &runtime_offsets::Values::C_BaseEntity_m_iTeamNum},
        OffsetField{"schemas", "C_BasePlayerPawn.m_vOldOrigin", &runtime_offsets::Values::C_BasePlayerPawn_m_vOldOrigin},
        OffsetField{"schemas", "C_BasePlayerPawn.m_pWeaponServices", &runtime_offsets::Values::C_BasePlayerPawn_m_pWeaponServices},
        OffsetField{"schemas", "CCSPlayerController.m_hPlayerPawn", &runtime_offsets::Values::CCSPlayerController_m_hPlayerPawn},
        OffsetField{"schemas", "CBasePlayerController.m_iszPlayerName", &runtime_offsets::Values::CBasePlayerController_m_iszPlayerName},
        OffsetField{"schemas", "CPlayer_WeaponServices.m_hActiveWeapon", &runtime_offsets::Values::CPlayer_WeaponServices_m_hActiveWeapon},
        OffsetField{"schemas", "CPlayer_WeaponServices.m_hMyWeapons", &runtime_offsets::Values::CPlayer_WeaponServices_m_hMyWeapons},
        OffsetField{"schemas", "C_BaseEntity.m_iHealth", &runtime_offsets::Values::C_BaseEntity_m_iHealth},
        OffsetField{"schemas", "C_CSPlayerPawn.m_ArmorValue", &runtime_offsets::Values::C_CSPlayerPawn_m_ArmorValue},
        OffsetField{"schemas", "C_BaseEntity.m_lifeState", &runtime_offsets::Values::C_BaseEntity_m_lifeState},
        OffsetField{"schemas", "C_BaseEntity.m_CBodyComponent", &runtime_offsets::Values::C_BaseEntity_m_CBodyComponent},
        OffsetField{"schemas", "C_BaseEntity.m_pGameSceneNode", &runtime_offsets::Values::C_BaseEntity_m_pGameSceneNode},
        OffsetField{"schemas", "C_BaseEntity.m_hOwnerEntity", &runtime_offsets::Values::C_BaseEntity_m_hOwnerEntity},
        OffsetField{"schemas", "C_BaseModelEntity.m_Collision", &runtime_offsets::Values::C_BaseModelEntity_m_Collision},
        OffsetField{"schemas", "CCollisionProperty.m_vecMins", &runtime_offsets::Values::CCollisionProperty_m_vecMins},
        OffsetField{"schemas", "CCollisionProperty.m_vecMaxs", &runtime_offsets::Values::CCollisionProperty_m_vecMaxs},
        OffsetField{"schemas", "C_BaseEntity.m_nSubclassID", &runtime_offsets::Values::C_BaseEntity_m_nSubclassID},
        OffsetField{"schemas", "C_BaseEntity.m_vecVelocity", &runtime_offsets::Values::C_BaseEntity_m_vecVelocity},
        OffsetField{"schemas", "C_BasePlayerWeapon.m_iClip1", &runtime_offsets::Values::C_BasePlayerWeapon_m_iClip1},
        OffsetField{"schemas", "C_CSPlayerPawn.m_pClippingWeapon", &runtime_offsets::Values::C_CSPlayerPawn_m_pClippingWeapon},
        OffsetField{"schemas", "C_CSPlayerPawn.m_bIsScoped", &runtime_offsets::Values::C_CSPlayerPawn_m_bIsScoped},
        OffsetField{"schemas", "C_CSPlayerPawn.m_bIsDefusing", &runtime_offsets::Values::C_CSPlayerPawn_m_bIsDefusing},
        OffsetField{"schemas", "C_CSPlayerPawn.m_angEyeAngles", &runtime_offsets::Values::C_CSPlayerPawn_m_angEyeAngles},
        OffsetField{"schemas", "C_CSPlayerPawnBase.m_flFlashDuration", &runtime_offsets::Values::C_CSPlayerPawnBase_m_flFlashDuration},
        OffsetField{"schemas", "C_CSPlayerPawnBase.m_pItemServices", &runtime_offsets::Values::C_CSPlayerPawnBase_m_pItemServices},
        OffsetField{"schemas", "CCSPlayer_ItemServices.m_bHasDefuser", &runtime_offsets::Values::CCSPlayer_ItemServices_m_bHasDefuser},
        OffsetField{"schemas", "C_CSPlayerPawn.m_entitySpottedState", &runtime_offsets::Values::C_CSPlayerPawn_m_entitySpottedState},
        OffsetField{"schemas", "C_EconEntity.m_AttributeManager", &runtime_offsets::Values::C_EconEntity_m_AttributeManager},
        OffsetField{"schemas", "C_AttributeContainer.m_Item", &runtime_offsets::Values::C_AttributeContainer_m_Item},
        OffsetField{"schemas", "C_EconItemView.m_iItemDefinitionIndex", &runtime_offsets::Values::C_EconItemView_m_iItemDefinitionIndex},
        OffsetField{"schemas", "C_CSGameRules.m_vMinimapMins", &runtime_offsets::Values::C_CSGameRules_m_vMinimapMins},
        OffsetField{"schemas", "C_CSGameRules.m_vMinimapMaxs", &runtime_offsets::Values::C_CSGameRules_m_vMinimapMaxs},
        OffsetField{"schemas", "C_CSGameRules.m_bBombPlanted", &runtime_offsets::Values::C_CSGameRules_m_bBombPlanted},
        OffsetField{"schemas", "C_CSGameRules.m_bBombDropped", &runtime_offsets::Values::C_CSGameRules_m_bBombDropped},
        OffsetField{"schemas", "CGameSceneNode.m_vecAbsOrigin", &runtime_offsets::Values::CGameSceneNode_m_vecAbsOrigin},
        OffsetField{"schemas", "EntitySpottedState_t.m_bSpotted", &runtime_offsets::Values::EntitySpottedState_t_m_bSpotted},
        OffsetField{"schemas", "EntitySpottedState_t.m_bSpottedByMask", &runtime_offsets::Values::EntitySpottedState_t_m_bSpottedByMask},
        OffsetField{"schemas", "C_PlantedC4.m_bBombTicking", &runtime_offsets::Values::C_PlantedC4_m_bBombTicking},
        OffsetField{"schemas", "C_PlantedC4.m_flC4Blow", &runtime_offsets::Values::C_PlantedC4_m_flC4Blow},
        OffsetField{"schemas", "C_PlantedC4.m_bBeingDefused", &runtime_offsets::Values::C_PlantedC4_m_bBeingDefused},
        OffsetField{"schemas", "C_PlantedC4.m_flDefuseCountDown", &runtime_offsets::Values::C_PlantedC4_m_flDefuseCountDown},
        OffsetField{"schemas", "C_Inferno.m_nFireEffectTickBegin", &runtime_offsets::Values::C_Inferno_m_nFireEffectTickBegin},
        OffsetField{"schemas", "C_Inferno.m_nFireLifetime", &runtime_offsets::Values::C_Inferno_m_nFireLifetime},
        OffsetField{"schemas", "C_Inferno.m_fireCount", &runtime_offsets::Values::C_Inferno_m_fireCount},
        OffsetField{"schemas", "C_Inferno.m_bInPostEffectTime", &runtime_offsets::Values::C_Inferno_m_bInPostEffectTime},
        OffsetField{"schemas", "C_SmokeGrenadeProjectile.m_nSmokeEffectTickBegin", &runtime_offsets::Values::C_SmokeGrenadeProjectile_m_nSmokeEffectTickBegin},
        OffsetField{"schemas", "C_SmokeGrenadeProjectile.m_bDidSmokeEffect", &runtime_offsets::Values::C_SmokeGrenadeProjectile_m_bDidSmokeEffect},
        OffsetField{"schemas", "C_SmokeGrenadeProjectile.m_bSmokeVolumeDataReceived", &runtime_offsets::Values::C_SmokeGrenadeProjectile_m_bSmokeVolumeDataReceived},
        OffsetField{"schemas", "C_SmokeGrenadeProjectile.m_bSmokeEffectSpawned", &runtime_offsets::Values::C_SmokeGrenadeProjectile_m_bSmokeEffectSpawned},
        OffsetField{"schemas", "C_DecoyProjectile.m_nDecoyShotTick", &runtime_offsets::Values::C_DecoyProjectile_m_nDecoyShotTick},
        OffsetField{"schemas", "C_DecoyProjectile.m_nClientLastKnownDecoyShotTick", &runtime_offsets::Values::C_DecoyProjectile_m_nClientLastKnownDecoyShotTick},
        OffsetField{"schemas", "C_BaseCSGrenadeProjectile.m_nExplodeEffectTickBegin", &runtime_offsets::Values::C_BaseCSGrenadeProjectile_m_nExplodeEffectTickBegin},
        OffsetField{"schemas", "CBodyComponentSkeletonInstance.m_skeletonInstance", &runtime_offsets::Values::CBodyComponentSkeletonInstance_m_skeletonInstance},
        OffsetField{"schemas", "CSkeletonInstance.m_modelState", &runtime_offsets::Values::CSkeletonInstance_m_modelState},
    };

    bool HasMeaningfulOffsetState(const OffsetState& state)
    {
        return !state.offsetsPatch.patchVersion.empty() ||
               !state.offsetsPatch.etag.empty() ||
               !state.offsetsPatch.lastFileSha.empty() ||
               state.offsetsPatch.clientVersion != 0 ||
               state.offsetsPatch.sourceRevision != 0 ||
               !state.lastSeenPatch.patchVersion.empty() ||
               !state.lastSeenPatch.etag.empty() ||
               !state.lastSeenPatch.lastFileSha.empty() ||
               state.lastSeenPatch.clientVersion != 0 ||
               state.lastSeenPatch.sourceRevision != 0 ||
               !state.selectedSource.empty() ||
               !state.selectedSourceTimestamp.empty() ||
               !state.remoteOutputTimestamp.empty() ||
               state.selectedSourceBuildNumber != 0;
    }

    OffsetState ReadOffsetStateFromRoot(const json& root)
    {
        OffsetState state = {};

        const json* stateRoot = &root;
        const auto stateIt = root.find("state");
        if (stateIt != root.end() && stateIt->is_object())
            stateRoot = &(*stateIt);

        state.offsetsPatch = ReadPatchInfo(*stateRoot, "offsets_patch");
        state.lastSeenPatch = ReadPatchInfo(*stateRoot, "last_seen_patch");
        state.selectedSource = JsonStringOrEmpty(*stateRoot, "selected_source");
        state.selectedSourceTimestamp = JsonStringOrEmpty(*stateRoot, "selected_source_timestamp");
        state.remoteOutputTimestamp = JsonStringOrEmpty(*stateRoot, "remote_output_timestamp");
        state.selectedSourceBuildNumber = JsonIntOrZero(*stateRoot, "selected_source_build_number");
        return state;
    }

    OffsetState ReadOffsetState(const std::filesystem::path& jsonPath, const std::filesystem::path& legacyStatePath)
    {
        OffsetState state = {};
        if (!jsonPath.empty())
        {
            std::ifstream file(jsonPath, std::ios::binary);
            if (file.is_open())
            {
                json root = json::parse(file, nullptr, false);
                if (!root.is_discarded() && root.is_object()) {
                    state = ReadOffsetStateFromRoot(root);
                    if (HasMeaningfulOffsetState(state))
                        return state;
                }
            }
        }

        if (legacyStatePath.empty())
            return state;

        std::ifstream legacyFile(legacyStatePath, std::ios::binary);
        if (!legacyFile.is_open())
            return state;

        json legacyRoot = json::parse(legacyFile, nullptr, false);
        if (legacyRoot.is_discarded() || !legacyRoot.is_object())
            return state;

        state = ReadOffsetStateFromRoot(legacyRoot);
        return state;
    }

    template <typename JsonT>
    void WriteOffsetStateSection(JsonT& root, const OffsetState& state)
    {
        if (!HasMeaningfulOffsetState(state)) {
            root.erase("state");
            return;
        }

        JsonT stateRoot = JsonT::object();
        WritePatchInfo(stateRoot, "offsets_patch", state.offsetsPatch);
        WritePatchInfo(stateRoot, "last_seen_patch", state.lastSeenPatch);
        stateRoot["selected_source"] = state.selectedSource;
        stateRoot["selected_source_timestamp"] = state.selectedSourceTimestamp;
        stateRoot["remote_output_timestamp"] = state.remoteOutputTimestamp;
        stateRoot["selected_source_build_number"] = state.selectedSourceBuildNumber;
        root["state"] = std::move(stateRoot);
    }

    bool WriteOffsetsJson(
        const std::filesystem::path& jsonPath,
        const runtime_offsets::Values& values,
        const OffsetState* state = nullptr)
    {
        try
        {
            if (jsonPath.empty())
                return false;

            std::filesystem::create_directories(jsonPath.parent_path());
            std::ofstream out(jsonPath, std::ios::out | std::ios::trunc);
            if (!out.is_open())
                return false;

            ordered_json root = ordered_json::object();
            for (const auto& field : kOffsetFields)
                root[field.section][field.key] = ToHex(values.*(field.member));

            if (state != nullptr)
                WriteOffsetStateSection(root, *state);

            out << root.dump(4) << '\n';
            RemoveFileIfExists(GetOffsetsStatePath());
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool WriteOffsetState(const std::filesystem::path& jsonPath, const OffsetState& state)
    {
        if (jsonPath.empty())
            return false;

        json root;
        if (!TryParseOffsetsJsonFile(jsonPath, root))
            return false;

        runtime_offsets::Values values = {};
        LoadValuesFromJson(root, values, nullptr, nullptr);
        if (!ValidateLoadedValues(values, true).empty())
            return false;
        return WriteOffsetsJson(jsonPath, values, &state);
    }

    bool RemoveFileIfExists(const std::filesystem::path& path)
    {
        if (path.empty())
            return false;

        std::error_code ec;
        if (!std::filesystem::exists(path, ec))
            return false;

        ec.clear();
        return std::filesystem::remove(path, ec) && !ec;
    }

    void CleanupObsoleteOffsetsIniFiles()
    {
        std::unordered_set<std::string> seen;
        const std::filesystem::path removablePaths[] = {
            GetOffsetsDirectory() / "offsets.ini",
            GetLegacyProfilesDirectory() / "offsets.ini",
            GetExeDirectory() / "offsets.ini",
        };

        for (const auto& path : removablePaths)
        {
            const std::string key = path.lexically_normal().generic_string();
            if (!seen.insert(key).second)
                continue;
            RemoveFileIfExists(path);
        }
    }

    enum class ReadResult
    {
        Parsed,
        Missing,
        Invalid
    };

    LegacyIniDocument ParseLegacyIniFile(const std::filesystem::path& path)
    {
        LegacyIniDocument ini;
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            return ini;

        std::string currentSection;
        std::string line;
        while (std::getline(file, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF)
            {
                line.erase(0, 3);
            }

            const std::string trimmed = Trim(line);
            if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#')
                continue;

            if (trimmed.front() == '[' && trimmed.back() == ']')
            {
                currentSection = Trim(trimmed.substr(1, trimmed.size() - 2));
                continue;
            }

            const size_t equals = trimmed.find('=');
            if (equals == std::string::npos || currentSection.empty())
                continue;

            const std::string key = Trim(trimmed.substr(0, equals));
            if (key.empty())
                continue;

            ini[currentSection][key] = Trim(trimmed.substr(equals + 1));
        }

        return ini;
    }

    const std::string* FindLegacyIniValue(const LegacyIniDocument& ini,
                                          std::string_view section,
                                          std::string_view key)
    {
        const auto secIt = ini.find(std::string(section));
        if (secIt == ini.end())
            return nullptr;

        const auto keyIt = secIt->second.find(std::string(key));
        if (keyIt == secIt->second.end())
            return nullptr;

        return &keyIt->second;
    }

    std::wstring ToWide(std::string_view text)
    {
        if (text.empty())
            return {};

        const int length =
            MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (length <= 0)
            return {};

        std::wstring result(static_cast<size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
        return result;
    }

    const std::wstring& HttpUserAgentWide()
    {
        static const std::wstring kUserAgent = ToWide(app::build_info::HttpUserAgent());
        return kUserAgent;
    }

    const std::wstring& HttpUserAgentHeader()
    {
        static const std::wstring kHeader = L"User-Agent: " + HttpUserAgentWide();
        return kHeader;
    }

    bool HasHeaderName(const std::vector<std::wstring>& headers, std::wstring_view headerName)
    {
        for (const std::wstring& header : headers)
        {
            if (header.size() < headerName.size() + 1)
                continue;
            if (_wcsnicmp(header.c_str(), headerName.data(), headerName.size()) != 0)
                continue;
            if (header[headerName.size()] == L':')
                return true;
        }

        return false;
    }

    std::vector<std::wstring> BuildRequestHeaders(const std::vector<std::wstring>& extraHeaders = {})
    {
        std::vector<std::wstring> headers = extraHeaders;
        if (!HasHeaderName(headers, L"User-Agent"))
            headers.push_back(HttpUserAgentHeader());
        return headers;
    }

    ReadResult ReadLegacyIniOffset(const LegacyIniDocument& ini,
                                   const char* section,
                                   const char* key,
                                   std::ptrdiff_t& outValue)
    {
        const std::string* raw = FindLegacyIniValue(ini, section, key);
        if (raw == nullptr)
            return ReadResult::Missing;

        if (!TryParseOffset(*raw, outValue))
            return ReadResult::Invalid;

        return ReadResult::Parsed;
    }

    ReadResult ReadJsonOffset(const json& root,
                              const char* section,
                              const char* key,
                              std::ptrdiff_t& outValue)
    {
        const auto secIt = root.find(section);
        if (secIt == root.end() || !secIt->is_object())
            return ReadResult::Missing;

        const auto keyIt = secIt->find(key);
        if (keyIt == secIt->end())
            return ReadResult::Missing;

        if (keyIt->is_number_integer())
        {
            outValue = static_cast<std::ptrdiff_t>(keyIt->get<long long>());
            return ReadResult::Parsed;
        }

        if (keyIt->is_number_unsigned())
        {
            outValue = static_cast<std::ptrdiff_t>(keyIt->get<unsigned long long>());
            return ReadResult::Parsed;
        }

        if (keyIt->is_string() && TryParseOffset(keyIt->get_ref<const std::string&>(), outValue))
            return ReadResult::Parsed;

        return ReadResult::Invalid;
    }

    bool IsRequiredOffsetMember(std::ptrdiff_t runtime_offsets::Values::*member)
    {
        for (const auto& field : kRequiredRemoteFields)
        {
            if (field.member == member)
                return true;
        }
        return false;
    }

    void LoadValuesFromSource(runtime_offsets::Values& outValues,
                              std::vector<std::string>* missingKeys,
                              std::vector<std::string>* invalidKeys,
                              auto&& reader)
    {
        outValues = {};
        for (const auto& field : kOffsetFields)
        {
            std::ptrdiff_t value = 0;
            const ReadResult result = reader(field.section, field.key, value);
            if (result == ReadResult::Parsed)
                outValues.*(field.member) = value;
            else
                outValues.*(field.member) = 0;

            if (result == ReadResult::Missing && missingKeys)
                missingKeys->emplace_back(std::string(field.section) + "." + field.key);
            else if (result == ReadResult::Invalid && invalidKeys)
                invalidKeys->emplace_back(std::string(field.section) + "." + field.key);
        }
    }

    bool TryParseOffsetsJsonFile(const std::filesystem::path& jsonPath, json& outRoot)
    {
        std::ifstream file(jsonPath, std::ios::binary);
        if (!file.is_open())
            return false;

        outRoot = json::parse(file, nullptr, false);
        return !outRoot.is_discarded() && outRoot.is_object();
    }

    void LoadValuesFromJson(const json& root,
                            runtime_offsets::Values& outValues,
                            std::vector<std::string>* missingKeys,
                            std::vector<std::string>* invalidKeys)
    {
        LoadValuesFromSource(
            outValues,
            missingKeys,
            invalidKeys,
            [&](const char* section, const char* key, std::ptrdiff_t& outValue) {
                return ReadJsonOffset(root, section, key, outValue);
            });
    }

    void LoadValuesFromLegacyIni(const LegacyIniDocument& ini,
                                 runtime_offsets::Values& outValues,
                                 std::vector<std::string>* missingKeys,
                                 std::vector<std::string>* invalidKeys)
    {
        LoadValuesFromSource(
            outValues,
            missingKeys,
            invalidKeys,
            [&](const char* section, const char* key, std::ptrdiff_t& outValue) {
                return ReadLegacyIniOffset(ini, section, key, outValue);
            });
    }

    std::vector<std::string> ValidateLoadedValues(const runtime_offsets::Values& values, bool requiredOnly)
    {
        std::vector<std::string> zeroFields;
        for (const auto& field : kOffsetFields)
        {
            if (requiredOnly && !IsRequiredOffsetMember(field.member))
                continue;
            if (values.*(field.member) <= 0)
                zeroFields.emplace_back(std::string(field.section) + "." + field.key);
        }
        return zeroFields;
    }

    size_t CountValueDifferences(const runtime_offsets::Values& a, const runtime_offsets::Values& b)
    {
        size_t count = 0;
        for (const auto& field : kOffsetFields)
        {
            if (a.*(field.member) != b.*(field.member))
                ++count;
        }
        return count;
    }

    std::string JoinKeys(const std::vector<std::string>& keys)
    {
        std::ostringstream oss;
        for (size_t i = 0; i < keys.size(); ++i)
        {
            if (i > 0)
                oss << ", ";
            oss << keys[i];
        }
        return oss.str();
    }

    bool DownloadFile(const char* url, const std::filesystem::path& destination, std::string* error)
    {
        try
        {
            std::filesystem::create_directories(destination.parent_path());
        }
        catch (...)
        {
            if (error)
                *error = "Cannot create temp directory for download.";
            return false;
        }

        
        std::string urlStr(url);
        bool useHttps = (urlStr.rfind("https://", 0) == 0);
        size_t hostStart = urlStr.find("://");
        if (hostStart == std::string::npos) {
            if (error) *error = "Invalid URL: " + urlStr;
            return false;
        }
        hostStart += 3;
        size_t pathStart = urlStr.find('/', hostStart);
        std::string hostStr = (pathStart != std::string::npos) ? urlStr.substr(hostStart, pathStart - hostStart) : urlStr.substr(hostStart);
        std::string pathStr = (pathStart != std::string::npos) ? urlStr.substr(pathStart) : "/";

        std::wstring wHost(hostStr.begin(), hostStr.end());
        std::wstring wPath(pathStr.begin(), pathStr.end());

        HINTERNET hSession = WinHttpOpen(
            HttpUserAgentWide().c_str(),
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);
        if (!hSession) {
            if (error) *error = "WinHttpOpen failed";
            return false;
        }

        INTERNET_PORT port = useHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
        HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), port, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            if (error) *error = "WinHttpConnect failed for " + hostStr;
            return false;
        }

        DWORD flags = useHttps ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wPath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            if (error) *error = "WinHttpOpenRequest failed";
            return false;
        }

        for (const std::wstring& header : BuildRequestHeaders()) {
            WinHttpAddRequestHeaders(
                hRequest,
                header.c_str(),
                static_cast<DWORD>(header.size()),
                WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
        }

        if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(hRequest, nullptr)) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            if (error) *error = "Download failed for " + urlStr;
            return false;
        }

        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        if (!WinHttpQueryHeaders(
                hRequest,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &statusCode,
                &statusCodeSize,
                WINHTTP_NO_HEADER_INDEX)) {
            statusCode = 0;
        }

        if (statusCode < 200 || statusCode >= 300) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            if (error)
                *error = "Download returned status " + std::to_string(statusCode) + " for " + urlStr;
            return false;
        }

        std::ofstream out(destination, std::ios::binary);
        if (!out.is_open()) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            if (error) *error = "Cannot open output file";
            return false;
        }

        DWORD bytesRead = 0;
        char buf[8192];
        while (WinHttpReadData(hRequest, buf, sizeof(buf), &bytesRead) && bytesRead > 0) {
            out.write(buf, bytesRead);
        }

        out.close();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return true;
    }

    struct HttpTextResponse
    {
        DWORD statusCode = 0;
        std::string body;
        std::string etag;
    };

    bool HttpGetText(const std::string& url,
                     const std::vector<std::wstring>& extraHeaders,
                     HttpTextResponse& out,
                     std::string* error)
    {
        std::string urlStr(url);
        const bool useHttps = (urlStr.rfind("https://", 0) == 0);
        size_t hostStart = urlStr.find("://");
        if (hostStart == std::string::npos)
        {
            if (error)
                *error = "Invalid URL: " + urlStr;
            return false;
        }
        hostStart += 3;
        const size_t pathStart = urlStr.find('/', hostStart);
        const std::string hostStr =
            (pathStart != std::string::npos) ? urlStr.substr(hostStart, pathStart - hostStart) : urlStr.substr(hostStart);
        const std::string pathStr =
            (pathStart != std::string::npos) ? urlStr.substr(pathStart) : "/";

        const std::wstring wHost(hostStr.begin(), hostStr.end());
        const std::wstring wPath(pathStr.begin(), pathStr.end());

        HINTERNET hSession = WinHttpOpen(
            HttpUserAgentWide().c_str(),
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);
        if (!hSession)
        {
            if (error)
                *error = "WinHttpOpen failed";
            return false;
        }

        const INTERNET_PORT port = useHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
        HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), port, 0);
        if (!hConnect)
        {
            WinHttpCloseHandle(hSession);
            if (error)
                *error = "WinHttpConnect failed for " + hostStr;
            return false;
        }

        const DWORD flags = useHttps ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wPath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hRequest)
        {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            if (error)
                *error = "WinHttpOpenRequest failed";
            return false;
        }

        for (const std::wstring& header : BuildRequestHeaders(extraHeaders))
        {
            if (!header.empty())
                WinHttpAddRequestHeaders(hRequest, header.c_str(), static_cast<DWORD>(header.size()), WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
        }

        const bool ok =
            WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(hRequest, nullptr);
        if (!ok)
        {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            if (error)
                *error = "HTTP GET failed for " + urlStr;
            return false;
        }

        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);
        out.statusCode = statusCode;

        std::wstring etagHeaderName = L"ETag";
        DWORD etagBytes = 0;
        if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CUSTOM, etagHeaderName.data(), WINHTTP_NO_OUTPUT_BUFFER, &etagBytes, WINHTTP_NO_HEADER_INDEX) &&
            GetLastError() == ERROR_INSUFFICIENT_BUFFER &&
            etagBytes > sizeof(wchar_t))
        {
            std::wstring etagWide(etagBytes / sizeof(wchar_t), L'\0');
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CUSTOM, etagHeaderName.data(), etagWide.data(), &etagBytes, WINHTTP_NO_HEADER_INDEX))
            {
                etagWide.resize((etagBytes / sizeof(wchar_t)) ? ((etagBytes / sizeof(wchar_t)) - 1) : 0);
                out.etag.clear();
                out.etag.reserve(etagWide.size());
                for (const wchar_t ch : etagWide)
                    out.etag.push_back(static_cast<char>(ch));
            }
        }

        out.body.clear();
        DWORD bytesRead = 0;
        char buffer[8192];
        while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0)
            out.body.append(buffer, buffer + bytesRead);

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        if (out.statusCode < 200 || out.statusCode >= 300)
        {
            if (error)
                *error = "HTTP GET returned status " + std::to_string(out.statusCode) + " for " + urlStr;
            return false;
        }

        return true;
    }

    std::string StripQuotes(std::string text)
    {
        text = Trim(text);
        if (text.size() >= 2 && text.front() == '"' && text.back() == '"')
            return text.substr(1, text.size() - 2);
        return text;
    }

    bool Base64Decode(std::string_view input, std::string& output)
    {
        std::string compact;
        compact.reserve(input.size());
        for (const char ch : input)
        {
            if (!std::isspace(static_cast<unsigned char>(ch)))
                compact.push_back(ch);
        }

        DWORD decodedSize = 0;
        if (!CryptStringToBinaryA(compact.c_str(), static_cast<DWORD>(compact.size()), CRYPT_STRING_BASE64, nullptr, &decodedSize, nullptr, nullptr))
            return false;

        std::string decoded(decodedSize, '\0');
        if (!CryptStringToBinaryA(compact.c_str(), static_cast<DWORD>(compact.size()), CRYPT_STRING_BASE64, reinterpret_cast<BYTE*>(decoded.data()), &decodedSize, nullptr, nullptr))
            return false;

        decoded.resize(decodedSize);
        output = std::move(decoded);
        return true;
    }

    bool TryParseInt(const std::string& text, int& outValue)
    {
        try
        {
            outValue = std::stoi(Trim(text));
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool FetchSteamInfSnapshot(SteamInfSnapshot& snapshot, std::string* error)
    {
        HttpTextResponse response = {};
        if (!HttpGetText(
                "https://raw.githubusercontent.com/SteamTracking/GameTracking-CS2/master/game/csgo/steam.inf",
                {},
                response,
                error))
        {
            return false;
        }

        std::unordered_map<std::string, std::string> kv;
        std::istringstream stream(response.body);
        std::string line;
        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            const size_t equals = line.find('=');
            if (equals == std::string::npos)
                continue;

            const std::string key = Trim(line.substr(0, equals));
            if (key.empty())
                continue;
            kv[key] = Trim(line.substr(equals + 1));
        }

        snapshot = {};
        snapshot.patch.etag = StripQuotes(response.etag);
        snapshot.patch.lastFileSha = snapshot.patch.etag;
        snapshot.patch.patchVersion = kv["PatchVersion"];
        snapshot.versionDate = kv["VersionDate"];
        snapshot.versionTime = kv["VersionTime"];
        TryParseInt(kv["ClientVersion"], snapshot.patch.clientVersion);
        TryParseInt(kv["SourceRevision"], snapshot.patch.sourceRevision);
        snapshot.patchBuildNumber = ParsePatchBuildNumber(snapshot.patch.patchVersion);

        if (snapshot.patch.patchVersion.empty() || snapshot.patch.clientVersion <= 0 || snapshot.patch.sourceRevision <= 0)
        {
            if (error)
                *error = "steam.inf is missing PatchVersion, ClientVersion, or SourceRevision.";
            return false;
        }

        return true;
    }

    bool ParseHeaderConstants(const std::filesystem::path& filePath,
                              std::unordered_map<std::string, std::ptrdiff_t>& outMap,
                              std::string* error)
    {
        std::ifstream file(filePath, std::ios::in | std::ios::binary);
        if (!file.is_open())
        {
            if (error)
                *error = "Unable to open " + filePath.string();
            return false;
        }

        static const std::regex kConstRegex(
            R"(^\s*constexpr\s+std::ptrdiff_t\s+([A-Za-z_]\w*)\s*=\s*(0x[0-9A-Fa-f]+|\d+)\s*;)");

        std::vector<std::string> namespaceStack;
        std::vector<int> namespaceDepths;
        int braceDepth = 0;
        std::string line;
        std::smatch match;

        while (std::getline(file, line))
        {
            if (std::regex_search(line, match, kConstRegex))
            {
                const std::string name = match[1].str();
                const std::string valueText = match[2].str();

                std::ptrdiff_t value = 0;
                try
                {
                    const int base = (valueText.size() > 2 && valueText[0] == '0' &&
                                      (valueText[1] == 'x' || valueText[1] == 'X'))
                        ? 16
                        : 10;
                    value = static_cast<std::ptrdiff_t>(std::stoll(valueText, nullptr, base));
                }
                catch (...)
                {
                    if (error)
                        *error = "Invalid constexpr value in " + filePath.string();
                    return false;
                }

                std::ostringstream keyBuilder;
                for (size_t i = 0; i < namespaceStack.size(); ++i)
                {
                    if (i > 0)
                        keyBuilder << "::";
                    keyBuilder << namespaceStack[i];
                }
                if (!namespaceStack.empty())
                    keyBuilder << "::";
                keyBuilder << name;

                outMap[keyBuilder.str()] = value;
            }

            for (size_t i = 0; i < line.size(); ++i)
            {
                if (line.compare(i, 9, "namespace") == 0)
                {
                    const bool hasBoundaryBefore = (i == 0) ||
                        !(std::isalnum(static_cast<unsigned char>(line[i - 1])) || line[i - 1] == '_');
                    if (!hasBoundaryBefore)
                        continue;

                    size_t j = i + 9;
                    while (j < line.size() && std::isspace(static_cast<unsigned char>(line[j])))
                        ++j;

                    const size_t nameStart = j;
                    while (j < line.size() &&
                        (std::isalnum(static_cast<unsigned char>(line[j])) || line[j] == '_'))
                    {
                        ++j;
                    }
                    const size_t nameEnd = j;

                    if (j > nameStart)
                    {
                        while (j < line.size() && std::isspace(static_cast<unsigned char>(line[j])))
                            ++j;

                        if (j < line.size() && line[j] == '{')
                        {
                            ++braceDepth;
                            namespaceStack.push_back(line.substr(nameStart, nameEnd - nameStart));
                            namespaceDepths.push_back(braceDepth);
                            i = j;
                            continue;
                        }
                    }
                }

                if (line[i] == '{')
                {
                    ++braceDepth;
                }
                else if (line[i] == '}')
                {
                    if (braceDepth > 0)
                        --braceDepth;
                    while (!namespaceDepths.empty() && braceDepth < namespaceDepths.back())
                    {
                        namespaceDepths.pop_back();
                        namespaceStack.pop_back();
                    }
                }
            }
        }

        return true;
    }

    bool AssignRequired(const std::unordered_map<std::string, std::ptrdiff_t>& map,
                        const char* key,
                        std::ptrdiff_t& outValue,
                        std::vector<std::string>& missingKeys)
    {
        const auto it = map.find(key);
        if (it == map.end())
        {
            missingKeys.emplace_back(key);
            return false;
        }

        outValue = it->second;
        return true;
    }

    bool ExtractRequiredValues(const std::unordered_map<std::string, std::ptrdiff_t>& parsedMap,
                               runtime_offsets::Values& outValues,
                               std::string* error)
    {
        std::vector<std::string> missingKeys;
        for (const auto& field : kRequiredRemoteFields)
            AssignRequired(parsedMap, field.remoteKey, outValues.*(field.member), missingKeys);

        if (!missingKeys.empty())
        {
            if (error)
                *error = "Missing keys in remote output: " + JoinKeys(missingKeys);
            return false;
        }

        return true;
    }

    void ExtractOptionalValues(const std::unordered_map<std::string, std::ptrdiff_t>& parsedMap,
                               runtime_offsets::Values& outValues)
    {
        for (const auto& field : kOptionalRemoteFields)
        {
            const auto it = parsedMap.find(field.remoteKey);
            if (it != parsedMap.end())
                outValues.*(field.member) = it->second;
        }
    }

    std::string BuildRawOutputUrl(const std::string& sourceFile)
    {
        return "https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/" + sourceFile;
    }

    std::filesystem::path GetPackagedOutputDirectory()
    {
        const auto exeDir = GetExeDirectory();
        if (exeDir.empty())
            return {};
        return exeDir / "output";
    }

    std::filesystem::path GetProjectOutputDirectory()
    {
        const auto projectRoot = FindProjectRoot();
        if (projectRoot.empty())
            return {};
        return projectRoot / "output";
    }

    std::filesystem::path NormalizeDirectoryPath(const std::filesystem::path& path)
    {
        if (path.empty())
            return {};

        std::error_code ec;
        const auto absolutePath = std::filesystem::absolute(path, ec);
        if (ec)
            return path.lexically_normal();

        return absolutePath.lexically_normal();
    }

    std::filesystem::path ReadOverrideDirectoryPath(const std::filesystem::path& filePath)
    {
        try
        {
            if (filePath.empty() || !std::filesystem::exists(filePath))
                return {};

            std::ifstream file(filePath, std::ios::in | std::ios::binary);
            if (!file.is_open())
                return {};

            std::ostringstream buffer;
            buffer << file.rdbuf();
            const std::string raw = Trim(buffer.str());
            if (raw.empty())
                return {};

            return std::filesystem::path(raw);
        }
        catch (...)
        {
            return {};
        }
    }

    std::filesystem::path FindLegacyOffsetsSourceDirectory()
    {
        char envBuffer[MAX_PATH * 4] = {};
        const DWORD envLen = GetEnvironmentVariableA(
            "KEVQDMA_OFFSETS_LOCAL_DIR",
            envBuffer,
            static_cast<DWORD>(sizeof(envBuffer)));
        if (envLen > 0 && envLen < sizeof(envBuffer))
        {
            const std::filesystem::path envPath = Trim(std::string(envBuffer, envLen));
            if (!envPath.empty())
                return envPath;
        }

        const auto offsetsDir = GetOffsetsDirectory();
        const auto newOverrideFile = offsetsDir / "offsets_source.txt";
        const auto newOverridePath = ReadOverrideDirectoryPath(newOverrideFile);
        if (!newOverridePath.empty())
            return newOverridePath;

        const auto legacyProfilesDir = GetLegacyProfilesDirectory();
        const auto projectRoot = FindProjectRoot();
        const std::filesystem::path legacyOverrideFiles[] = {
            legacyProfilesDir / "offsets_source.txt",
            projectRoot / "sdk" / "offsets_source.txt",
            projectRoot / "include" / "sdk" / "offsets_source.txt",
        };

        for (const auto& legacyFile : legacyOverrideFiles)
        {
            const auto legacyPath = ReadOverrideDirectoryPath(legacyFile);
            if (legacyPath.empty())
                continue;

            CopyFileToTargetIfPresent(legacyFile, newOverrideFile);
            return legacyPath;
        }

        return {};
    }

    std::unordered_set<std::string> CollectRemoteSourceFiles()
    {
        std::unordered_set<std::string> requiredFiles;
        for (const auto& field : kRequiredRemoteFields)
            requiredFiles.insert(field.sourceFile);
        for (const auto& field : kOptionalRemoteFields)
            requiredFiles.insert(field.sourceFile);
        return requiredFiles;
    }

    bool ParseOutputDirectory(const std::filesystem::path& directory,
                              std::unordered_map<std::string, std::ptrdiff_t>& parsedMap,
                              std::string* error)
    {
        const auto requiredFiles = CollectRemoteSourceFiles();
        for (const std::string& sourceFile : requiredFiles)
        {
            const std::filesystem::path sourcePath = directory / sourceFile;
            if (!std::filesystem::exists(sourcePath))
            {
                if (error)
                    *error = "Missing local dump file: " + sourcePath.string();
                return false;
            }

            if (!ParseHeaderConstants(sourcePath, parsedMap, error))
                return false;
        }

        return true;
    }

    bool HasRequiredOutputFiles(const std::filesystem::path& directory)
    {
        if (directory.empty() || !std::filesystem::exists(directory))
            return false;

        const auto requiredFiles = CollectRemoteSourceFiles();
        for (const std::string& sourceFile : requiredFiles)
        {
            if (!std::filesystem::exists(directory / sourceFile))
                return false;
        }

        return true;
    }

    std::string ReadFileText(const std::filesystem::path& filePath)
    {
        try
        {
            std::ifstream file(filePath, std::ios::in | std::ios::binary);
            if (!file.is_open())
                return {};

            std::ostringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        }
        catch (...)
        {
            return {};
        }
    }

    std::string CanonicalizeTimestamp(const std::string& text)
    {
        static const std::regex kTimestampRegex(
            R"((\d{4})-(\d{2})-(\d{2})[ T](\d{2}):(\d{2}):(\d{2})(?:\.(\d+))?(?:\s*(?:UTC|Z))?)");

        std::smatch match;
        if (!std::regex_search(text, match, kTimestampRegex))
            return {};

        std::string fraction = match[7].matched ? match[7].str() : "";
        if (fraction.size() < 9)
            fraction.append(9 - fraction.size(), '0');
        else if (fraction.size() > 9)
            fraction.resize(9);

        std::ostringstream canonical;
        canonical << match[1].str()
                  << match[2].str()
                  << match[3].str()
                  << match[4].str()
                  << match[5].str()
                  << match[6].str()
                  << fraction;
        return canonical.str();
    }

    std::string ReadOutputDirectoryTimestamp(const std::filesystem::path& directory)
    {
        if (directory.empty() || !std::filesystem::exists(directory))
            return {};

        const std::string infoText = ReadFileText(directory / "info.json");
        if (!infoText.empty())
        {
            static const std::regex kInfoTimestampRegex(R"json("timestamp"\s*:\s*"([^"]+)")json");
            std::smatch match;
            if (std::regex_search(infoText, match, kInfoTimestampRegex))
            {
                const std::string canonical = CanonicalizeTimestamp(match[1].str());
                if (!canonical.empty())
                    return canonical;
            }

            const std::string canonical = CanonicalizeTimestamp(infoText);
            if (!canonical.empty())
                return canonical;
        }

        for (const char* headerFile : { "offsets.hpp", "client_dll.hpp" })
        {
            const std::string headerText = ReadFileText(directory / headerFile);
            const std::string canonical = CanonicalizeTimestamp(headerText);
            if (!canonical.empty())
                return canonical;
        }

        return {};
    }

    int ReadOutputDirectoryBuildNumber(const std::filesystem::path& directory)
    {
        if (directory.empty() || !std::filesystem::exists(directory))
            return 0;

        const std::string infoText = ReadFileText(directory / "info.json");
        if (infoText.empty())
            return 0;

        static const std::regex kBuildRegex(R"json("build_number"\s*:\s*(\d+))json");
        std::smatch match;
        if (!std::regex_search(infoText, match, kBuildRegex))
            return 0;

        try
        {
            return std::stoi(match[1].str());
        }
        catch (...)
        {
            return 0;
        }
    }

    void AddOutputDirectoryCandidate(std::vector<OutputDirectoryCandidate>& candidates,
                                     std::unordered_set<std::string>& seenDirectories,
                                     const std::filesystem::path& directory,
                                     const char* label)
    {
        const auto normalizedDirectory = NormalizeDirectoryPath(directory);
        if (normalizedDirectory.empty())
            return;

        const std::string key = normalizedDirectory.generic_string();
        if (!seenDirectories.insert(key).second)
            return;

        OutputDirectoryCandidate candidate;
        candidate.directory = normalizedDirectory;
        candidate.label = label;
        candidate.hasRequiredFiles = HasRequiredOutputFiles(normalizedDirectory);
        if (candidate.hasRequiredFiles)
        {
            candidate.timestamp = ReadOutputDirectoryTimestamp(normalizedDirectory);
            candidate.buildNumber = ReadOutputDirectoryBuildNumber(normalizedDirectory);
        }

        candidates.push_back(candidate);
    }

    std::vector<OutputDirectoryCandidate> CollectLocalOutputCandidates()
    {
        std::vector<OutputDirectoryCandidate> candidates;
        std::unordered_set<std::string> seenDirectories;

        AddOutputDirectoryCandidate(
            candidates,
            seenDirectories,
            FindLegacyOffsetsSourceDirectory(),
            "override output");
        AddOutputDirectoryCandidate(
            candidates,
            seenDirectories,
            GetProjectOutputDirectory(),
            "project output");
        AddOutputDirectoryCandidate(
            candidates,
            seenDirectories,
            GetPackagedOutputDirectory(),
            "packaged output");

        return candidates;
    }

    const OutputDirectoryCandidate* FindPreferredLocalOutputCandidate(
        const std::vector<OutputDirectoryCandidate>& candidates)
    {
        const OutputDirectoryCandidate* bestCandidate = nullptr;
        for (const auto& candidate : candidates)
        {
            if (!candidate.hasRequiredFiles)
                continue;

            if (bestCandidate == nullptr)
            {
                bestCandidate = &candidate;
                continue;
            }

            if (!candidate.timestamp.empty() &&
                (bestCandidate->timestamp.empty() || candidate.timestamp > bestCandidate->timestamp))
            {
                bestCandidate = &candidate;
            }
        }

        return bestCandidate;
    }

    std::string DescribeOutputDirectoryCandidate(const OutputDirectoryCandidate& candidate)
    {
        const std::string folderName =
            candidate.directory.filename().empty()
            ? candidate.directory.generic_string()
            : candidate.directory.filename().generic_string();
        return folderName.empty()
            ? candidate.label
            : (candidate.label + " (" + folderName + ")");
    }

    bool DownloadFileWithRetry(const std::string& url,
                               const std::filesystem::path& destination,
                               std::string* error)
    {
        for (int attempt = 0; attempt < 2; ++attempt)
        {
            if (DownloadFile(url.c_str(), destination, error))
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }

        return false;
    }

    bool DownloadOutputDirectoryFiles(const std::filesystem::path& directory,
                                      bool includeInfoJson,
                                      std::string* error)
    {
        auto sourceFiles = CollectRemoteSourceFiles();
        if (includeInfoJson)
            sourceFiles.insert("info.json");

        for (const std::string& sourceFile : sourceFiles)
        {
            if (!DownloadFileWithRetry(BuildRawOutputUrl(sourceFile), directory / sourceFile, error))
                return false;
        }

        return true;
    }

    bool DownloadOutputInfoFile(const std::filesystem::path& directory, std::string* error)
    {
        try
        {
            std::filesystem::create_directories(directory);
        }
        catch (...)
        {
            if (error)
                *error = "Cannot create temporary output directory: " + directory.string();
            return false;
        }

        return DownloadFileWithRetry(BuildRawOutputUrl("info.json"), directory / "info.json", error);
    }
}

const runtime_offsets::Values& runtime_offsets::Get()
{
    return g_values;
}

bool runtime_offsets::AutoUpdateFromGitHub(std::string* message, runtime_offsets::AutoUpdateReport* report)
{
    #include "runtime_offsets_parts/runtime_offsets_autoupdate_body.inl"
}

bool runtime_offsets::HasOffsetsFile()
{
    std::error_code ec;
    const auto jsonPath = FindOffsetsJsonPath(false);
    if (!jsonPath.empty() && std::filesystem::exists(jsonPath, ec))
        return true;

    return !FindLegacyOffsetsPath("offsets.json").empty() ||
           !FindLegacyOffsetsPath("offsets.ini").empty();
}

bool runtime_offsets::Load(std::string* message)
{
    #include "runtime_offsets_parts/runtime_offsets_load_body.inl"
}

runtime_offsets::StateView runtime_offsets::GetStateView()
{
    const auto jsonPath = FindOffsetsJsonPath(false);
    const OffsetState state = ReadOffsetState(jsonPath, GetOffsetsStatePath());

    StateView view = {};
    view.offsetsPatch = state.offsetsPatch;
    view.lastSeenPatch = state.lastSeenPatch;
    view.selectedSource = state.selectedSource;
    view.selectedSourceTimestamp = state.selectedSourceTimestamp;
    view.selectedSourceBuildNumber = state.selectedSourceBuildNumber;
    return view;
}
