#include <Windows.h>
#include <TlHelp32.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#include <DMALibrary/Memory/Memory.h>

#include "app/Bootstrap/runtime_console.h"
#include "app/Bootstrap/version_update.h"
#include "app/Config/config.h"
#include "app/Core/build_info.h"
#include "app/Core/globals.h"
#include "app/Platform/overlay.h"
#include "Features/ESP/esp.h"
#include "Features/WebRadar/webradar.h"
#include "Game/Offsets/runtime_offsets.h"

#include <algorithm>
#include <cctype>
#include <atomic>
#include <ctime>
#include <conio.h>
#include <cstring>
#include <format>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

namespace
{
    DWORD FindLocalPid(const char* processName)
    {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE)
            return 0;

        PROCESSENTRY32W pe = {};
        pe.dwSize = sizeof(pe);

        const int nameLen = MultiByteToWideChar(CP_UTF8, 0, processName, -1, nullptr, 0);
        std::wstring wName(nameLen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, processName, -1, wName.data(), nameLen);
        if (!wName.empty() && wName.back() == L'\0')
            wName.pop_back();

        DWORD pid = 0;
        if (Process32FirstW(snap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, wName.c_str()) == 0) {
                    pid = pe.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
        return pid;
    }

    bool HasFlag(int argc, char* argv[], const char* flag)
    {
        for (int i = 1; i < argc; ++i) {
            if (argv[i] && std::strcmp(argv[i], flag) == 0)
                return true;
        }
        return false;
    }

    bool SafeAutoUpdateOffsets(std::string* message, runtime_offsets::AutoUpdateReport* report)
    {
        try
        {
            return runtime_offsets::AutoUpdateFromGitHub(message, report);
        }
        catch (const std::exception& e)
        {
            if (message)
                *message = std::string("Offset sync failed: ") + e.what();
            return false;
        }
        catch (...)
        {
            if (message)
                *message = "Offset sync failed. Using local offsets.";
            return false;
        }
    }

    bool SafeLoadOffsets(std::string* message)
    {
        try
        {
            return runtime_offsets::Load(message);
        }
        catch (const std::exception& e)
        {
            if (message)
                *message = std::string("Offsets load failed: ") + e.what();
            return false;
        }
        catch (...)
        {
            if (message)
                *message = "Offsets load failed: runtime exception.";
            return false;
        }
    }

    bool SafeInitDma(std::string* message)
    {
        try
        {
            return mem.InitDma(true, false);
        }
        catch (const std::exception& e)
        {
            if (message)
                *message = std::string("DMA initialization failed: ") + e.what();
            return false;
        }
        catch (...)
        {
            if (message)
                *message = "DMA initialization failed: runtime exception.";
            return false;
        }
    }

    std::string BuildPatchDisplay(const runtime_offsets::PatchInfo& patch)
    {
        if (patch.patchVersion.empty())
            return {};
        if (patch.clientVersion <= 0)
            return patch.patchVersion;

        const int lastThreeDigits = patch.clientVersion % 1000;
        return std::format("{} ({:03d})", patch.patchVersion, lastThreeDigits);
    }

    std::string BuildGamePatchDisplay(const runtime_offsets::AutoUpdateReport& report)
    {
        const std::string patchDisplay = BuildPatchDisplay(report.currentPatch);
        if (patchDisplay.empty())
            return {};

        std::string result = patchDisplay;
        if (report.currentPatch.clientVersion > 0)
            result += " | ClientVersion: " + std::to_string(report.currentPatch.clientVersion);
        if (report.currentPatch.sourceRevision > 0)
            result += " | SourceRevision: " + std::to_string(report.currentPatch.sourceRevision);
        if (!report.currentPatchVersionDate.empty() && !report.currentPatchVersionTime.empty())
            result += " | Steam raw: " + report.currentPatchVersionDate + " " + report.currentPatchVersionTime;
        return result;
    }

    std::string BuildOffsetCheckLabel(const runtime_offsets::AutoUpdateReport& report)
    {
        (void)report;
        return "Offset check update";
    }

    std::string BuildPatchTransitionDisplay(const runtime_offsets::AutoUpdateReport& report)
    {
        const runtime_offsets::PatchInfo& previous = report.previousOffsetsPatch;
        const std::string previousDisplay = BuildPatchDisplay(previous);
        const std::string currentDisplay = BuildPatchDisplay(report.currentPatch);
        if (previousDisplay.empty() || currentDisplay.empty() || previousDisplay == currentDisplay)
            return {};
        return "Last: " + previousDisplay + " -> New Last: " + currentDisplay;
    }

    void WarmUpEspSnapshot()
    {
        constexpr auto kWarmupBudget = std::chrono::milliseconds(450);
        constexpr auto kWarmupRetryDelay = std::chrono::milliseconds(18);

        const auto deadline = std::chrono::steady_clock::now() + kWarmupBudget;
        do {
            if (esp::UpdateData())
                return;
            std::this_thread::sleep_for(kWarmupRetryDelay);
        } while (std::chrono::steady_clock::now() < deadline);
    }

    void WaitForExitAcknowledge(const bootstrap::RuntimeConsole& console)
    {
        HANDLE inputHandle = GetStdHandle(STD_INPUT_HANDLE);
        if (inputHandle == INVALID_HANDLE_VALUE)
            return;
        if (GetFileType(inputHandle) != FILE_TYPE_CHAR)
            return;

        console.PrintInfoLine("Press any key to exit.");
        _getch();
    }

    void PrintCommunityLinks(const bootstrap::RuntimeConsole& console)
    {
        console.PrintInfoLine("Menu: P | Telegram: @ne_sravnim | Discord: CoraKevq");
        console.PrintLine("GitHub", app::build_info::RepositoryUrl() + " [ " + app::build_info::VersionTag() + " ]");
    }

    bool PromptToInstallVersionUpdate(const bootstrap::VersionUpdateInfo& updateInfo)
    {
        HANDLE inputHandle = GetStdHandle(STD_INPUT_HANDLE);
        if (inputHandle == INVALID_HANDLE_VALUE)
            return false;
        if (GetFileType(inputHandle) != FILE_TYPE_CHAR)
            return false;

        std::cout
            << "  | GitHub | New version available: "
            << updateInfo.latestVersion
            << " (current "
            << updateInfo.currentVersion
            << ")"
            << '\n';
        std::cout
            << "  | GitHub | Install new version now? [Y/n]: "
            << std::flush;

        std::string answer;
        if (!std::getline(std::cin, answer))
            return false;

        answer.erase(
            std::remove_if(
                answer.begin(),
                answer.end(),
                [](unsigned char ch) { return std::isspace(ch) != 0; }),
            answer.end());
        std::transform(
            answer.begin(),
            answer.end(),
            answer.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

        return answer.empty() || answer == "y" || answer == "yes";
    }

    std::string BuildOffsetTimestampDisplay(const std::string& canonicalTimestamp)
    {
        if (canonicalTimestamp.size() < 14)
            return canonicalTimestamp;

        auto parsePart = [&](size_t start, size_t len) -> int {
            int value = 0;
            for (size_t i = 0; i < len; ++i)
            {
                const char ch = canonicalTimestamp[start + i];
                if (ch < '0' || ch > '9')
                    return -1;
                value = (value * 10) + (ch - '0');
            }
            return value;
        };

        std::tm utcTm = {};
        utcTm.tm_year = parsePart(0, 4) - 1900;
        utcTm.tm_mon = parsePart(4, 2) - 1;
        utcTm.tm_mday = parsePart(6, 2);
        utcTm.tm_hour = parsePart(8, 2);
        utcTm.tm_min = parsePart(10, 2);
        utcTm.tm_sec = parsePart(12, 2);
        if (utcTm.tm_year < 70 || utcTm.tm_mon < 0 || utcTm.tm_mday <= 0)
            return canonicalTimestamp;

        const std::time_t utcTime = _mkgmtime(&utcTm);
        if (utcTime == static_cast<std::time_t>(-1))
            return canonicalTimestamp;

        std::tm localTm = {};
        localtime_s(&localTm, &utcTime);
        return std::format(
            "{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d} UTC | local {:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}",
            utcTm.tm_year + 1900,
            utcTm.tm_mon + 1,
            utcTm.tm_mday,
            utcTm.tm_hour,
            utcTm.tm_min,
            utcTm.tm_sec,
            localTm.tm_year + 1900,
            localTm.tm_mon + 1,
            localTm.tm_mday,
            localTm.tm_hour,
            localTm.tm_min,
            localTm.tm_sec);
    }

    std::string BuildCompactOffsetTimestampDisplay(const std::string& canonicalTimestamp)
    {
        if (canonicalTimestamp.size() < 12)
            return {};

        auto parsePart = [&](size_t start, size_t len) -> int {
            int value = 0;
            for (size_t i = 0; i < len; ++i)
            {
                const char ch = canonicalTimestamp[start + i];
                if (ch < '0' || ch > '9')
                    return -1;
                value = (value * 10) + (ch - '0');
            }
            return value;
        };

        const int year = parsePart(0, 4);
        const int month = parsePart(4, 2);
        const int day = parsePart(6, 2);
        const int hour = parsePart(8, 2);
        const int minute = parsePart(10, 2);
        if (year < 0 || month < 1 || day < 1 || hour < 0 || minute < 0)
            return {};

        return std::format(
            "{:02d}-{:02d}-{:04d} {:02d}:{:02d} UTC",
            day, month, year, hour, minute);
    }

    template <typename Fn>
    auto RunWithPendingAnimation(const bootstrap::RuntimeConsole& console,
                                 const std::string& label,
                                 const std::string& text,
                                 Fn&& fn)
        -> decltype(fn())
    {
        using ResultT = decltype(fn());

        const DmaLogLevel savedLogLevel = DmaGetLogLevel();
        DmaSetLogLevel(DmaLogLevel::Silent);

        std::atomic<bool> finished = false;
        std::thread animator([&]() {
            int phase = 0;
            while (!finished.load(std::memory_order_acquire)) {
                console.PrintPending(label, text, phase++);
                std::this_thread::sleep_for(std::chrono::milliseconds(90));
            }
            });

        try
        {
            ResultT result = fn();
            finished.store(true, std::memory_order_release);
            if (animator.joinable())
                animator.join();
            DmaSetLogLevel(savedLogLevel);
            return result;
        }
        catch (...)
        {
            finished.store(true, std::memory_order_release);
            if (animator.joinable())
                animator.join();
            DmaSetLogLevel(savedLogLevel);
            throw;
        }
    }

    template <typename Fn>
    auto RunWithPendingAnimation(const bootstrap::RuntimeConsole& console, const std::string& text, Fn&& fn)
        -> decltype(fn())
    {
        return RunWithPendingAnimation(console, "Info", text, std::forward<Fn>(fn));
    }
}

int main(int argc, char* argv[])
{
    
    
    timeBeginPeriod(1);

    const bool verboseLogs = HasFlag(argc, argv, "--verbose");
    const bool offsetsSelfTest = HasFlag(argc, argv, "--offsets-self-test");

    bootstrap::RuntimeConsole console;
    console.Initialize(verboseLogs);

    console.PrintStartupBanner();
    console.PrintInfoLine("KevqDMA");

    console.AnimateForAtLeast("Connection", 360);
    console.PrintInfoOk("Connection");

    std::cout << '\n';

    if (!offsetsSelfTest) {
        std::string versionUpdateMessage;
        bootstrap::VersionUpdateInfo versionUpdateInfo = {};
        const bool versionCheckOk = RunWithPendingAnimation(console, "GitHub", "Version check", [&]() {
            return bootstrap::CheckForVersionUpdate(&versionUpdateInfo, &versionUpdateMessage);
        });
        if (versionCheckOk) {
            console.PrintOk("GitHub", "Version check");
            if (versionUpdateInfo.updateAvailable) {
                console.PrintLine("GitHub", "Update available | " + versionUpdateInfo.latestVersion + " | Current: " + versionUpdateInfo.currentVersion);
                if (PromptToInstallVersionUpdate(versionUpdateInfo)) {
                    std::string openReleaseError;
                    if (bootstrap::OpenVersionUpdateReleasePage(versionUpdateInfo, &openReleaseError)) {
                        console.PrintOk("GitHub", "Open latest release");
                        return 0;
                    }
                    console.PrintFail("GitHub", "Open latest release");
                    if (!openReleaseError.empty())
                        console.PrintLine("GitHub", openReleaseError);
                }
            }
        } else {
            console.PrintFail("GitHub", "Version check");
            if (!versionUpdateMessage.empty())
                console.PrintLine("GitHub", versionUpdateMessage);
        }

        std::cout << '\n';
    }

    std::string autoUpdateMessage;
    runtime_offsets::AutoUpdateReport autoUpdateReport = {};
    const bool updateOk = RunWithPendingAnimation(console, "Offset check update", [&]() {
        return SafeAutoUpdateOffsets(&autoUpdateMessage, &autoUpdateReport);
    });

    const std::string offsetCheckLabel = BuildOffsetCheckLabel(autoUpdateReport);
    if (updateOk) {
        console.PrintInfoOk(offsetCheckLabel, autoUpdateReport.offsetsUpdated ? 3 : 1);

        const std::string patchTransition = BuildPatchTransitionDisplay(autoUpdateReport);
        if (!patchTransition.empty())
            console.PrintInfoLine(patchTransition);

        const std::string currentPatchDisplay = BuildPatchDisplay(autoUpdateReport.currentPatch);
        const bool offsetsOutdated =
            !currentPatchDisplay.empty() &&
            !autoUpdateReport.offsetsCompatibleWithCurrentPatch;

        if (offsetsOutdated) {
            console.PrintErrorLine("Offsets are outdated and need to be replaced.");
        }
    } else {
        console.PrintInfoFail(offsetCheckLabel);
        if (!autoUpdateMessage.empty())
        console.PrintInfoLine(autoUpdateMessage);
    }

    std::string runtimeOffsetMessage;
    const std::string offsetSource = "offsets.json";
    const bool loadOk = RunWithPendingAnimation(console, "Offset load (" + offsetSource + ")", [&]() {
        return SafeLoadOffsets(&runtimeOffsetMessage);
    });
    if (!loadOk) {
        console.PrintErrorLine(runtimeOffsetMessage);
        WaitForExitAcknowledge(console);
        return 1;
    }
    std::string offsetLoadLabel = "Offset load (" + offsetSource + ")";
    const runtime_offsets::StateView loadedOffsetState = runtime_offsets::GetStateView();
    const std::string loadedOffsetsPatch = BuildPatchDisplay(loadedOffsetState.offsetsPatch);
    const std::string loadedOffsetsTimestamp = BuildCompactOffsetTimestampDisplay(loadedOffsetState.selectedSourceTimestamp);
    console.PrintInfoOk(offsetLoadLabel);
    if (!loadedOffsetsPatch.empty() || !loadedOffsetsTimestamp.empty()) {
        std::string offsetStateLine;
        if (!loadedOffsetsPatch.empty())
            offsetStateLine += "Last: " + loadedOffsetsPatch;
        if (!loadedOffsetsTimestamp.empty()) {
            if (!offsetStateLine.empty())
                offsetStateLine += " | ";
            offsetStateLine += loadedOffsetsTimestamp;
        }
        console.PrintInfoMarkedLine(offsetStateLine);
    }

    if (offsetsSelfTest) {
        std::cout << '\n';
        PrintCommunityLinks(console);
        return 0;
    }

    std::string dmaInitMessage;
    const bool dmaOk = RunWithPendingAnimation(console, "DMA subsystem initialized", [&]() {
        return SafeInitDma(&dmaInitMessage);
    });
    if (!dmaOk) {
        console.PrintInfoFail("DMA subsystem initialized");
        console.PrintErrorLine(dmaInitMessage.empty() ? "DMA initialization failed" : dmaInitMessage);
        WaitForExitAcknowledge(console);
        return 1;
    }
    console.PrintInfoOk("DMA subsystem initialized");

    if (mem.vHandle) {
        VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_CONFIG_TICK_PERIOD, 300);
        VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_CONFIG_READCACHE_TICKS, 8);
        VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_CONFIG_TLBCACHE_TICKS, 200);
        VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_CONFIG_PROCCACHE_TICKS_PARTIAL, 100);
        VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_CONFIG_PROCCACHE_TICKS_TOTAL, 4000);
    }

    config::Load();

    {
        const DmaLogLevel attachWaitLogLevel = DmaGetLogLevel();
        DmaSetLogLevel(DmaLogLevel::Silent);

        std::atomic<bool> csReady{false};
        std::atomic<bool> csCanceled{false};

        std::thread animator([&]() {
            int phase = 0;
            while (!csReady.load(std::memory_order_acquire) &&
                   !csCanceled.load(std::memory_order_acquire)) {
                console.PrintInfoPending("Waiting for cs2.exe", phase++);
                std::this_thread::sleep_for(std::chrono::milliseconds(90));
            }
        });

        
        
        int attempt = 0;
        while (true) {
            if (GetAsyncKeyState(VK_END) & 1) {
                csCanceled.store(true, std::memory_order_release);
                if (animator.joinable()) animator.join();
                DmaSetLogLevel(attachWaitLogLevel);
                console.PrintErrorLine("Canceled while waiting for cs2.exe");
                WaitForExitAcknowledge(console);
                return 1;
            }

            
            
            
            if (attempt > 0 && (attempt % 6) == 0) {
                mem.CloseDma();
                if (!mem.InitDma(true, false)) {
                    
                    mem.CloseDma();
                    mem.InitDma(false, false);
                }
                if (mem.vHandle) {
                    VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_CONFIG_TICK_PERIOD, 300);
                    VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_CONFIG_READCACHE_TICKS, 8);
                    VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_CONFIG_TLBCACHE_TICKS, 200);
                    VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_CONFIG_PROCCACHE_TICKS_PARTIAL, 100);
                    VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_CONFIG_PROCCACHE_TICKS_TOTAL, 4000);
                }
            }

            if (mem.vHandle && mem.AttachToProcess("cs2.exe", true)) {
                if (mem.vHandle)
                    VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_ALL, 1);
                g::clientBase = mem.GetBaseDaddy("client.dll");
                g::engine2Base = mem.GetBaseDaddy("engine2.dll");
                if (g::clientBase && g::engine2Base)
                    break;
                g::clientBase = 0;
                g::engine2Base = 0;
                mem.ResetProcessState();
            }

            ++attempt;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        csReady.store(true, std::memory_order_release);
        if (animator.joinable()) animator.join();
        DmaSetLogLevel(attachWaitLogLevel);
    }

    console.PrintInfoOk("Waiting for cs2.exe");

    
    
    
    
    
    
    
    
    {
        g::clientBase = 0;
        g::engine2Base = 0;
        if (mem.vHandle)
            VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_ALL, 1);
        mem.ResetProcessState();
        if (mem.vHandle && mem.AttachToProcess("cs2.exe", true)) {
            if (mem.vHandle)
                VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_ALL, 1);
            const uintptr_t freshClientBase = mem.GetBaseDaddy("client.dll");
            const uintptr_t freshEngine2Base = mem.GetBaseDaddy("engine2.dll");
            g::clientBase = freshClientBase;
            g::engine2Base = freshEngine2Base;
        }
    }

    if (!overlay::Create(g::screenWidth, g::screenHeight)) {
        console.PrintErrorLine("D3D11 overlay creation failed");
        WaitForExitAcknowledge(console);
        return 1;
    }

    WarmUpEspSnapshot();
    esp::StartDataWorker();
    webradar::Initialize();
    console.PrintInfoOk("The system is initialized and ready to work");

    std::cout << '\n';
    PrintCommunityLinks(console);

    struct ShutdownGuard {
        ~ShutdownGuard()
        {
            webradar::Shutdown();
            esp::StopDataWorker();
            overlay::Destroy();
        }
    } shutdownGuard;

    int exitCode = 0;
    try {
        overlay::Run();
    }
    catch (const std::exception& ex) {
        console.PrintErrorLine(std::format("Fatal runtime error: {}", ex.what()));
        exitCode = 1;
    }
    catch (...) {
        console.PrintErrorLine("Fatal runtime error");
        exitCode = 1;
    }

    return exitCode;
}
