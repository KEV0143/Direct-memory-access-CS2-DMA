#include "Features/ESP/DataReader/intervals.h"

bool esp::UpdateData()
{
    struct ScopedDirectReadWarningSuppression {
        Memory& memory;

        explicit ScopedDirectReadWarningSuppression(Memory& memRef) : memory(memRef)
        {
            memory.SetDirectReadWarningSuppressed(true);
        }

        ~ScopedDirectReadWarningSuppression()
        {
            memory.SetDirectReadWarningSuppressed(false);
        }
    } directReadWarningScope(mem);

#include "update_parts/update_bootstrap.inl"
#include "update_parts/update_engine_state.inl"

    uintptr_t entityList = 0;
    uintptr_t localPawn = 0;
    uintptr_t localController = 0;
    uintptr_t gameRules = 0;
    uintptr_t globalVars = 0;
    uintptr_t plantedC4Entity = 0;
    uintptr_t weaponC4Entity = 0;
    uintptr_t sensPtr = 0;
    uintptr_t listEntry = 0;
    int highestEntityIndex = 0;
    std::string liveMapKey;
    view_matrix_t viewMatrix = {};
    Vector3 viewAngles = {};
    auto handle = mem.CreateScatterHandle();
    if (!handle) {
        MarkDmaReadFailure();
        return false;
    }

    struct ScopedScatterWarningSuppression {
        Memory& memory;

        explicit ScopedScatterWarningSuppression(Memory& memRef) : memory(memRef)
        {
            memory.SetScatterReadWarningSuppressed(true);
        }

        ~ScopedScatterWarningSuppression()
        {
            memory.SetScatterReadWarningSuppressed(false);
        }
    } scatterWarningScope(mem);

    auto executeOptionalScatterRead = [&]() -> bool {
        return mem.ExecuteReadScatter(handle);
    };

    auto logUpdateDataIssue = [&](const char* stage, const char* reason) {
        const std::string_view reasonSv = reason ? reason : "";
        if (reasonSv.starts_with("optional_failed_"))
            return;
        if (reasonSv.find("using_cached_snapshot") != std::string_view::npos)
            return;
        const auto curStatus = static_cast<esp::GameStatus>(s_gameStatus.load(std::memory_order_relaxed));
        if (curStatus != esp::GameStatus::Ok)
            return;
        static uint32_t s_detailCount = 0;
        ++s_detailCount;
        if (!(s_detailCount == 1 || (s_detailCount % 25u) == 0u))
            return;

        const std::string_view stageSv = stage ? stage : "unknown";
        const char* stageLabel = "unknown";
        if (stageSv == "engine_signon") stageLabel = "engine network client sign-on state";
        else if (stageSv == "scatter_1") stageLabel = "base pointers (entityList/localPawn/view/sensitivity)";
        else if (stageSv == "scatter_2") stageLabel = "local state (team/pos/listEntry/gameRules/globalVars)";
        else if (stageSv == "scatter_3") stageLabel = "planted C4 state";
        else if (stageSv == "scatter_4") stageLabel = "planted C4 world position";
        else if (stageSv == "scatter_5") stageLabel = "controller array";
        else if (stageSv == "scatter_6") stageLabel = "controller data (pawn/name/ping/moneyService)";
        else if (stageSv == "scatter_7") stageLabel = "money service account";
        else if (stageSv == "scatter_8") stageLabel = "pawn entries";
        else if (stageSv == "scatter_9") stageLabel = "pawn pointers";
        else if (stageSv == "scatter_10" || stageSv == "scatter_10_core") stageLabel = "player state batch";
        else if (stageSv == "scatter_11") stageLabel = "defuser flags";
        else if (stageSv == "scatter_11_spotted") stageLabel = "spotted state flags";
        else if (stageSv == "scatter_12") stageLabel = "active weapon handles";
        else if (stageSv == "scatter_12_inv") stageLabel = "inventory weapon handles";
        else if (stageSv == "scatter_13") stageLabel = "active weapon entries";
        else if (stageSv == "scatter_14") stageLabel = "active weapon entities";
        else if (stageSv == "scatter_15") stageLabel = "weapon id/ammo";
        else if (stageSv == "scatter_16") stageLabel = "bone array pointers";
        else if (stageSv == "scatter_17") stageLabel = "bone positions";
        else if (stageSv == "scatter_18") stageLabel = "world blocks";
        else if (stageSv == "scatter_19") stageLabel = "world entity pointers";
        else if (stageSv == "scatter_20") stageLabel = "world entity details";
        else if (stageSv == "scatter_21") stageLabel = "world entity positions";
        else if (stageSv == "scatter_bomb_1") stageLabel = "bomb scan blocks";
        else if (stageSv == "scatter_bomb_2") stageLabel = "bomb scan entity pointers";
        else if (stageSv == "scatter_bomb_3") stageLabel = "bomb scan entity details";
        else if (stageSv == "scatter_bomb_4") stageLabel = "bomb scan entity positions";
        else if (stageSv == "base_ptrs") stageLabel = "required base pointers";
        else if (stageSv == "entity_list_entry") stageLabel = "entity list entry";

        DmaLogPrintf(
            "[WARN] UpdateData stage=%s (%s) reason=%s",
            stage ? stage : "unknown",
            stageLabel,
            reason ? reason : "unknown");

        if (stageSv == "scatter_1" || stageSv == "base_ptrs") {
            DmaLogPrintf(
                "[WARN] UpdateData details: ptrs client=0x%llX entityList=0x%llX localPawn=0x%llX",
                static_cast<unsigned long long>(g::clientBase),
                static_cast<unsigned long long>(entityList),
                static_cast<unsigned long long>(localPawn));
        } else if (stageSv == "scatter_10" || stageSv == "scatter_10_core") {
            DmaLogPrintf(
                "[WARN] UpdateData details: player-batch listEntry=0x%llX localPawn=0x%llX",
                static_cast<unsigned long long>(listEntry),
                static_cast<unsigned long long>(localPawn));
        } else if (stageSv == "scatter_13" || stageSv == "scatter_12" || stageSv == "scatter_14") {
            DmaLogPrintf(
                "[WARN] UpdateData details: weapon-chain entityList=0x%llX listEntry=0x%llX",
                static_cast<unsigned long long>(entityList),
                static_cast<unsigned long long>(listEntry));
        }
    };

    auto failRequiredRead = [&](const char* stage, const char* reason) -> bool {
        ++s_requiredReadFailureCount;
        mem.CloseScatterHandle(handle);

        if (s_requiredReadFailureCount >= RECOVERY_FAILURE_THRESHOLD) {
            const DWORD liveCs2Pid = mem.GetPidFromName("cs2.exe");
            const bool processStillPresent = liveCs2Pid != 0;
            if (!processStillPresent) {
                logUpdateDataIssue(stage, reason);
                enterWaitCs2();
                MarkDmaReadFailure();
                return true;
            }

            g::clientBase = 0;
            g::engine2Base = 0;
            logUpdateDataIssue(stage, reason);
            RequestDmaRecovery("required_reads_persistent");
            MarkDmaReadFailure();
            return true;
        }

        MarkDmaReadSuccess();
        return true;
    };
    auto failScatter = [&](const char* stage) -> bool {
        return failRequiredRead(stage, "scatter_read_failed");
    };

    auto failMissing = [&](const char* stage, const char* what) -> bool {
        return failRequiredRead(stage, what);
    };

    enum : unsigned {
        kNarrowDebugWeaponChain = 1u << 0,
        kNarrowDebugBones = 1u << 1,
        kNarrowDebugC4 = 1u << 2,
        kNarrowDebugWorld = 1u << 3,
    };

    const unsigned narrowDebugMask = []() -> unsigned {
        static const unsigned s_mask = []() -> unsigned {
            char raw[256] = {};
            const DWORD rawLen = GetEnvironmentVariableA("KEVQDMA_DEBUG_UPDATE", raw, static_cast<DWORD>(sizeof(raw)));
            if (rawLen == 0 || rawLen >= sizeof(raw))
                return 0u;

            std::string value(raw, rawLen);
            std::transform(value.begin(), value.end(), value.begin(), [](char ch) {
                if (ch >= 'A' && ch <= 'Z')
                    return static_cast<char>(ch - 'A' + 'a');
                return ch;
            });

            unsigned mask = 0u;
            if (value == "1" || value.find("all") != std::string::npos)
                mask = kNarrowDebugWeaponChain | kNarrowDebugBones | kNarrowDebugC4 | kNarrowDebugWorld;
            if (value.find("weapon") != std::string::npos)
                mask |= kNarrowDebugWeaponChain;
            if (value.find("bone") != std::string::npos)
                mask |= kNarrowDebugBones;
            if (value.find("c4") != std::string::npos || value.find("bomb") != std::string::npos)
                mask |= kNarrowDebugC4;
            if (value.find("world") != std::string::npos || value.find("utility") != std::string::npos)
                mask |= kNarrowDebugWorld;
            return mask;
        }();
        return s_mask;
    }();

    auto narrowDebugEnabled = [&](unsigned flag) -> bool {
        return (narrowDebugMask & flag) != 0u;
    };

    auto narrowDebugTick = [&](unsigned flag, uint32_t& counter, uint32_t period) -> bool {
        if (!narrowDebugEnabled(flag))
            return false;
        ++counter;
        if (counter == 1u)
            return true;
        return period > 0u && (counter % period) == 0u;
    };

#include "update_parts/update_scheduler_state.inl"

    const uint64_t _stageEngineEnd = TickNowUs();
#include "base_reads.inl"
    const uint64_t _stageBaseEnd = TickNowUs();
#include "player_reads.inl"
    const uint64_t _stagePlayerEnd = TickNowUs();
{
#include "commit_state.inl"
}
    const uint64_t _stageCommitEnd = TickNowUs();
#include "player_aux_reads.inl"
    const uint64_t _stagePlayerAuxEnd = TickNowUs();
#include "inventory_reads.inl"
    const uint64_t _stageInvEnd = TickNowUs();
#include "bone_reads.inl"
    const uint64_t _stageBoneEnd = TickNowUs();
    const uint64_t nowUs = TickNowUs();
    [[maybe_unused]] const uint64_t nowMs = nowUs / 1000u;
#include "world_reads.inl"
    const uint64_t _stageWorldEnd = TickNowUs();
#include "bomb_reads.inl"
    const uint64_t _stageBombEnd = TickNowUs();
{
#include "commit_enrichment_state.inl"
}
    const uint64_t _stageEnrichEnd = TickNowUs();
#include "update_parts/update_stage_metrics.inl"
    mem.CloseScatterHandle(handle);
    return true;
}
