#include "pch.h"
#include "Memory.h"

#include <thread>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <chrono>
#include <functional>
#include <random>
#include <TlHelp32.h>

namespace {
	std::atomic<bool> g_memoryWriteBlockedLogged = false;
	std::atomic<bool> g_scatterWriteBlockedLogged = false;

	void LogWriteBlockedOnce(std::atomic<bool>& flag, const char* operation)
	{
		bool expected = false;
		if (flag.compare_exchange_strong(expected, true, std::memory_order_relaxed))
			LOG("[WARN] %s blocked: DMA transport is enforced read-only.\n", operation);
	}

	std::mt19937_64& ReadJitterRng()
	{
		thread_local std::mt19937_64 rng([] {
			const auto nowSeed = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
			const auto tidSeed = static_cast<uint64_t>(std::hash<std::thread::id> {}(std::this_thread::get_id()));
			std::seed_seq seq {
				static_cast<uint32_t>(nowSeed),
				static_cast<uint32_t>(nowSeed >> 32),
				static_cast<uint32_t>(tidSeed),
				static_cast<uint32_t>(tidSeed >> 32)
			};
			return std::mt19937_64(seq);
		}());
		return rng;
	}

	void BusyWaitMicros(int delayUs)
	{
		if (delayUs <= 0)
			return;

		const auto deadline = std::chrono::steady_clock::now() + std::chrono::microseconds(delayUs);
		while (std::chrono::steady_clock::now() < deadline)
			YieldProcessor();
	}

	void ApplyReadJitter(int minUs, int maxUs, int minGapUs)
	{
		if (maxUs <= 0 || minUs > maxUs)
			return;

		thread_local auto lastJitterAt = std::chrono::steady_clock::time_point {};
		const auto now = std::chrono::steady_clock::now();
		if (minGapUs > 0 && lastJitterAt.time_since_epoch().count() != 0) {
			if ((now - lastJitterAt) < std::chrono::microseconds(minGapUs))
				return;
		}

		std::uniform_int_distribution<int> distribution(minUs, maxUs);
		lastJitterAt = now;
		BusyWaitMicros(distribution(ReadJitterRng()));
	}

	void ApplyDirectReadJitter()
	{
		ApplyReadJitter(12, 60, 250);
	}

	void ApplyScatterReadJitter()
	{
		ApplyReadJitter(3, 12, 0);
	}

	uint64_t SteadyNowMs()
	{
		return static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now().time_since_epoch()).count());
	}

	uint64_t FileTimeToUnixMs(const FILETIME& fileTime)
	{
		ULARGE_INTEGER value = {};
		value.LowPart = fileTime.dwLowDateTime;
		value.HighPart = fileTime.dwHighDateTime;
		if (value.QuadPart < 116444736000000000ULL)
			return 0;
		return static_cast<uint64_t>((value.QuadPart - 116444736000000000ULL) / 10000ULL);
	}

	uint64_t CurrentSystemTimeMs()
	{
		FILETIME ft = {};
		GetSystemTimeAsFileTime(&ft);
		return FileTimeToUnixMs(ft);
	}

	uint64_t ApproxBootTimeMs()
	{
		const uint64_t nowMs = CurrentSystemTimeMs();
		const uint64_t uptimeMs = GetTickCount64();
		return (nowMs > uptimeMs) ? (nowMs - uptimeMs) : 0;
	}

	std::string BuildMemMapCachePath()
	{
		auto tempPath = std::filesystem::temp_directory_path();
		return (tempPath / "KevqDMA_mmap.txt").string();
	}

	bool IsUsableMemMapCache(const std::string& path)
	{
		WIN32_FILE_ATTRIBUTE_DATA attrs = {};
		if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &attrs))
			return false;
		if ((attrs.nFileSizeHigh == 0 && attrs.nFileSizeLow == 0) ||
			(attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			return false;

		const uint64_t fileWriteMs = FileTimeToUnixMs(attrs.ftLastWriteTime);
		const uint64_t bootMs = ApproxBootTimeMs();
		if (fileWriteMs == 0 || bootMs == 0)
			return false;

		return fileWriteMs + 5000ULL >= bootMs;
	}
}

bool Memory::EnsureRuntimeLibrariesLoaded()
{
	if (modules.VMM && modules.FTD3XX && modules.LEECHCORE)
		return true;

	LOG("[INFO] Loading required runtime libraries.\n");
	if (!modules.VMM)
		modules.VMM = LoadLibraryA("vmm.dll");
	if (!modules.FTD3XX)
		modules.FTD3XX = LoadLibraryA("FTD3XX.dll");
	if (!modules.LEECHCORE)
		modules.LEECHCORE = LoadLibraryA("leechcore.dll");

	if (!modules.VMM || !modules.FTD3XX || !modules.LEECHCORE)
	{
		LOG("vmm: %p\n", modules.VMM);
		LOG("ftd: %p\n", modules.FTD3XX);
		LOG("leech: %p\n", modules.LEECHCORE);
		LOG("[ERROR] Failed to load one or more runtime libraries.\n");
		return false;
	}

	LOG("[INFO] Runtime libraries loaded successfully.\n");
	return true;
}

Memory::Memory()
{
	try
	{
		this->key = std::make_shared<c_keys>();
	}
	catch (...)
	{
		key.reset();
	}
}

Memory::~Memory()
{
	if (this->vHandle)
		VMMDLL_Close(this->vHandle);
	DMA_INITIALIZED = false;
	PROCESS_INITIALIZED = false;
}

bool Memory::DumpMemoryMap(const std::string& outputPath, bool debug)
{
	LPCSTR args[] = { "-device", "fpga", "-waitinitialize", "-norefresh", "", "" };
	int argc = 4;
	if (debug)
	{
		args[argc++] = const_cast<LPCSTR>("-v");
		args[argc++] = const_cast<LPCSTR>("-printf");
	}

	VMM_HANDLE handle = VMMDLL_Initialize(argc, args);
	if (!handle)
	{
		LOG("[ERROR] Failed to open VMM handle.\n");
		return false;
	}

	PVMMDLL_MAP_PHYSMEM pPhysMemMap = NULL;
	if (!VMMDLL_Map_GetPhysMem(handle, &pPhysMemMap))
	{
		LOG("[ERROR] Failed to query physical memory map.\n");
		VMMDLL_Close(handle);
		return false;
	}

	if (pPhysMemMap->dwVersion != VMMDLL_MAP_PHYSMEM_VERSION)
	{
		LOG("[ERROR] Unsupported physical memory map version.\n");
		VMMDLL_MemFree(pPhysMemMap);
		VMMDLL_Close(handle);
		return false;
	}

	if (pPhysMemMap->cMap == 0)
	{
		LOG("[ERROR] Physical memory map is empty.\n");
		VMMDLL_MemFree(pPhysMemMap);
		VMMDLL_Close(handle);
		return false;
	}
	//Dump map to file
	std::stringstream sb;
	for (DWORD i = 0; i < pPhysMemMap->cMap; i++)
	{
		sb << std::hex << pPhysMemMap->pMap[i].pa << " " << (pPhysMemMap->pMap[i].pa + pPhysMemMap->pMap[i].cb - 1) << std::endl;
	}

	std::ofstream nFile(outputPath);
	nFile << sb.str();
	nFile.close();

	VMMDLL_MemFree(pPhysMemMap);
	LOG("[INFO] Physical memory map file written successfully.\n");
	VMMDLL_Close(handle);
	return true;
}

unsigned char abort2[4] = {0x10, 0x00, 0x10, 0x00};

bool Memory::SetFPGA()
{
	ULONG64 qwID = 0, qwVersionMajor = 0, qwVersionMinor = 0;
	if (!VMMDLL_ConfigGet(this->vHandle, LC_OPT_FPGA_FPGA_ID, &qwID) ||
		!VMMDLL_ConfigGet(this->vHandle, LC_OPT_FPGA_VERSION_MAJOR, &qwVersionMajor) ||
		!VMMDLL_ConfigGet(this->vHandle, LC_OPT_FPGA_VERSION_MINOR, &qwVersionMinor))
	{
		LOG("[WARN] FPGA metadata query failed. Proceeding with limited diagnostics.\n");
		return false;
	}

	LOG("[INFO] VMMDLL_ConfigGet: ID=%lli VERSION=%lli.%lli\n", qwID, qwVersionMajor, qwVersionMinor);

	if ((qwVersionMajor >= 4) && ((qwVersionMajor >= 5) || (qwVersionMinor >= 7)))
	{
		HANDLE handle;
		LC_CONFIG config = {.dwVersion = LC_CONFIG_VERSION, .szDevice = "existing"};
		handle = LcCreate(&config);
		if (!handle)
		{
			LOG("[ERROR] Failed to create FPGA configuration handle.\n");
			return false;
		}

		LcCommand(handle, LC_CMD_FPGA_CFGREGPCIE_MARKWR | 0x002, 4, reinterpret_cast<PBYTE>(&abort2), NULL, NULL);
		LOG("[INFO] FPGA PCIe register flag reset completed.\n");
		LcClose(handle);
	}

	return true;
}

bool Memory::InitDma(bool memMap, bool debug)
{
	if (DMA_INITIALIZED)
		return true;

	LAST_DMA_INIT_STATS = {};
	const uint64_t totalStartMs = SteadyNowMs();
	if (!EnsureRuntimeLibrariesLoaded())
		return false;
	LAST_DMA_INIT_STATS.runtimeLibsMs = SteadyNowMs() - totalStartMs;

	if (!this->key)
		this->key = std::make_shared<c_keys>();

	LOG("[INFO] DMA subsystem initialization started.\n");

	bool useMemMap = memMap;
	while (true)
	{
		LPCSTR args[] = { const_cast<LPCSTR>(""), const_cast<LPCSTR>("-device"), const_cast<LPCSTR>("fpga://algo=0"), const_cast<LPCSTR>(""), const_cast<LPCSTR>(""), const_cast<LPCSTR>(""), const_cast<LPCSTR>("") };
		DWORD argc = 3;
		if (debug)
		{
			args[argc++] = const_cast<LPCSTR>("-v");
			args[argc++] = const_cast<LPCSTR>("-printf");
		}

		std::string path = "";
		if (useMemMap)
		{
			LAST_DMA_INIT_STATS.usedMemMap = true;
			path = BuildMemMapCachePath();
			if (IsUsableMemMapCache(path))
			{
				LAST_DMA_INIT_STATS.reusedMemMapCache = true;
				LOG("[INFO] Reusing physical memory map cache from current boot.\n");
			}
			else
			{
				const uint64_t memMapStartMs = SteadyNowMs();
				LOG("[INFO] Physical memory map acquisition started.\n");
				const bool dumped = this->DumpMemoryMap(path, debug);
				LAST_DMA_INIT_STATS.memMapMs = SteadyNowMs() - memMapStartMs;
				if (!dumped)
				{
					LOG("[WARN] Memory map acquisition failed. Continuing without memory map.\n");
				}
				else
				{
					LOG("[INFO] Physical memory map acquired successfully.\n");
				}
			}

			if (IsUsableMemMapCache(path))
			{
				args[argc++] = const_cast<LPSTR>("-memmap");
				args[argc++] = const_cast<LPSTR>(path.c_str());
			}
		}

		const uint64_t vmmInitStartMs = SteadyNowMs();
		this->vHandle = VMMDLL_Initialize(argc, args);
		LAST_DMA_INIT_STATS.vmmInitMs += (SteadyNowMs() - vmmInitStartMs);
		if (this->vHandle)
			break;

		if (useMemMap)
		{
			useMemMap = false;
			LOG("[WARN] DMA initialization with memory map failed; retrying without memory map.\n");
			continue;
		}
		LOG("[ERROR] DMA initialization failed. Verify FPGA connection and device availability.\n");
		return false;
	}

	ULONG64 FPGA_ID = 0, DEVICE_ID = 0;

	VMMDLL_ConfigGet(this->vHandle, LC_OPT_FPGA_FPGA_ID, &FPGA_ID);
	VMMDLL_ConfigGet(this->vHandle, LC_OPT_FPGA_DEVICE_ID, &DEVICE_ID);

	LOG("[INFO] FPGA identifier: %llu\n", FPGA_ID);
	LOG("[INFO] Device identifier: %llu\n", DEVICE_ID);

	const uint64_t fpgaStartMs = SteadyNowMs();
	if (!this->SetFPGA())
	{
		LOG("[ERROR] FPGA configuration step failed.\n");
		VMMDLL_Close(this->vHandle);
		return false;
	}
	LAST_DMA_INIT_STATS.fpgaConfigMs = SteadyNowMs() - fpgaStartMs;

	DMA_INITIALIZED = TRUE;
	LAST_DMA_INIT_STATS.totalMs = SteadyNowMs() - totalStartMs;
	LOG(
		"[INFO] DMA init timing: total=%llums libs=%llums memmap=%llums%s vmm=%llums fpga=%llums\n",
		static_cast<unsigned long long>(LAST_DMA_INIT_STATS.totalMs),
		static_cast<unsigned long long>(LAST_DMA_INIT_STATS.runtimeLibsMs),
		static_cast<unsigned long long>(LAST_DMA_INIT_STATS.memMapMs),
		LAST_DMA_INIT_STATS.reusedMemMapCache ? " [cache]" : "",
		static_cast<unsigned long long>(LAST_DMA_INIT_STATS.vmmInitMs),
		static_cast<unsigned long long>(LAST_DMA_INIT_STATS.fpgaConfigMs));
	LOG("[INFO] DMA subsystem initialization completed successfully.\n");
	return true;
}

bool Memory::AttachToProcess(const std::string& process_name, bool applyCr3Fix)
{
	if (!DMA_INITIALIZED)
	{
		LOG("[ERROR] AttachToProcess called before DMA initialization.\n");
		return false;
	}

	auto to_lower = [](std::string s) {
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return s;
	};
	auto strip_exe = [&](std::string s) {
		std::string lower = to_lower(s);
		if (lower.size() > 4 && lower.substr(lower.size() - 4) == ".exe")
			return s.substr(0, s.size() - 4);
		return s;
	};
	auto canAccessProbeModules = [&](DWORD pid, const std::string& targetProcessName) -> bool {
		auto hasModule = [&](const std::string& moduleName) -> bool {
			return !moduleName.empty() &&
				VMMDLL_ProcessGetModuleBaseU(this->vHandle, pid, moduleName.c_str()) != 0;
		};

		const std::string targetLower = to_lower(targetProcessName);
		const std::string targetBase = strip_exe(targetProcessName);
		const std::string targetBaseLower = to_lower(targetBase);
		const bool isCs2Target =
			targetLower == "cs2.exe" ||
			targetLower == "cs2" ||
			targetBaseLower == "cs2";

		if (isCs2Target) {
			// For CS2 the exe image resolving is too weak a proof that the
			// DTB is correct. The process may be visible while the real game
			// modules are still unresolved or stale. Require a gameplay
			// module to be accessible.
			return hasModule("client.dll") || hasModule("engine2.dll");
		}

		return hasModule(targetProcessName) || (targetBase != targetProcessName && hasModule(targetBase));
	};

	const std::string target_name = to_lower(process_name);
	if (PROCESS_INITIALIZED && to_lower(current_process.process_name) == target_name)
	{
		if (applyCr3Fix && this->vHandle)
			VMMDLL_ConfigSet(this->vHandle, VMMDLL_OPT_REFRESH_ALL, 1);
		if (!applyCr3Fix || canAccessProbeModules(current_process.PID, process_name))
			return true;

		LOG("[INFO] Re-attaching %s because probe modules are not yet accessible.\n", process_name.c_str());
		PROCESS_INITIALIZED = FALSE;
		current_process = {};
	}

	PROCESS_INITIALIZED = FALSE;
	current_process = {};

	current_process.PID = GetPidFromName(process_name);
	if (!current_process.PID)
		return false;

	current_process.process_name = process_name;

	if (applyCr3Fix)
	{
		if (!FixCr3())
			LOG("[WARN] CR3 remediation was not confirmed for %s.\n", process_name.c_str());
		else
			LOG("[INFO] CR3 remediation completed for %s.\n", process_name.c_str());
	}

	if (this->vHandle)
		VMMDLL_ConfigSet(this->vHandle, VMMDLL_OPT_REFRESH_ALL, 1);

	current_process.base_address = GetBaseDaddy(process_name);
	if (!current_process.base_address)
	{
		LOG("[WARN] Unable to resolve module base address for %s during attach. Continuing with PID-only attach.\n", process_name.c_str());
	}

	current_process.base_size = GetBaseSize(process_name);
	if (!current_process.base_size)
	{
		LOG("[WARN] Unable to resolve module image size for %s during attach. Continuing with PID-only attach.\n", process_name.c_str());
	}

	LOG("[INFO] Target process attached.\n");
	LOG("[INFO] Process: %s\n", process_name.c_str());
	LOG("[INFO] PID: %i\n", current_process.PID);
	LOG("[INFO] Base address: 0x%llx\n", current_process.base_address);
	LOG("[INFO] Image size: 0x%llx\n", current_process.base_size);

	PROCESS_INITIALIZED = TRUE;

	return true;
}

bool Memory::Init(std::string process_name, bool memMap, bool debug)
{
	if (!InitDma(memMap, debug))
		return false;
	return AttachToProcess(process_name, true);
}

void Memory::ResetProcessState()
{
	PROCESS_INITIALIZED = FALSE;
	current_process = {};
}

static bool s_fixCr3PluginsInitialized = false;

void Memory::CloseDma()
{
	PROCESS_INITIALIZED = FALSE;
	current_process = {};
	if (vHandle) {
		VMMDLL_Close(vHandle);
		vHandle = nullptr;
	}
	DMA_INITIALIZED = FALSE;
	// Reset so that FixCr3 re-initializes plugins on the new VMM handle.
	s_fixCr3PluginsInitialized = false;
}

DWORD Memory::GetPidFromName(const std::string& process_name)
{
	DWORD pid = 0;
	VMMDLL_PidGetFromName(this->vHandle, (LPSTR)process_name.c_str(), &pid);
	if (pid)
		return pid;

	// Some environments expose names without ".exe" in PID lookup APIs.
	std::string base_name = process_name;
	if (base_name.size() > 4)
	{
		std::string tail = base_name.substr(base_name.size() - 4);
		std::transform(tail.begin(), tail.end(), tail.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (tail == ".exe")
			base_name = base_name.substr(0, base_name.size() - 4);
	}

	if (base_name != process_name)
	{
		VMMDLL_PidGetFromName(this->vHandle, (LPSTR)base_name.c_str(), &pid);
		if (pid)
			return pid;
	}

	// Fallback: enumerate all processes and do a case-insensitive substring match.
	PVMMDLL_PROCESS_INFORMATION process_info = NULL;
	DWORD total_processes = 0;
	if (!VMMDLL_ProcessGetInformationAll(this->vHandle, &process_info, &total_processes))
		return 0;

	auto to_lower = [](std::string s) {
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return s;
	};

	const std::string target_a = to_lower(process_name);
	const std::string target_b = to_lower(base_name);
	const std::string target_c = to_lower(base_name + ".exe");

	struct Candidate
	{
		DWORD pid = 0;
		int score = 0;
		bool hasProcessModule = false;
		bool hasClientModule = false;
	};

	Candidate best = {};

	for (DWORD i = 0; i < total_processes; i++)
	{
		std::string short_name = process_info[i].szName ? process_info[i].szName : "";
		std::string long_name = process_info[i].szNameLong ? process_info[i].szNameLong : "";
		short_name = to_lower(short_name);
		long_name = to_lower(long_name);

		int score = 0;
		if (short_name == target_a || short_name == target_b || short_name == target_c)
			score = 4;
		else if (long_name == target_a || long_name == target_b || long_name == target_c)
			score = 3;
		else if (short_name.find(target_a) != std::string::npos || long_name.find(target_a) != std::string::npos ||
		         short_name.find(target_b) != std::string::npos || long_name.find(target_b) != std::string::npos ||
		         short_name.find(target_c) != std::string::npos || long_name.find(target_c) != std::string::npos)
			score = 1;

		if (score == 0 || process_info[i].dwPID == 0)
			continue;

		const DWORD candidatePid = process_info[i].dwPID;
		const bool hasProcessModule =
			VMMDLL_ProcessGetModuleBaseU(this->vHandle, candidatePid, process_name.c_str()) != 0 ||
			(base_name != process_name && VMMDLL_ProcessGetModuleBaseU(this->vHandle, candidatePid, base_name.c_str()) != 0);
		const bool hasClientModule = VMMDLL_ProcessGetModuleBaseU(this->vHandle, candidatePid, "client.dll") != 0;

		const bool better =
			best.pid == 0 ||
			score > best.score ||
			(score == best.score && hasClientModule && !best.hasClientModule) ||
			(score == best.score && hasClientModule == best.hasClientModule && hasProcessModule && !best.hasProcessModule) ||
			(score == best.score && hasClientModule == best.hasClientModule && hasProcessModule == best.hasProcessModule && candidatePid > best.pid);

		if (better)
		{
			best.pid = candidatePid;
			best.score = score;
			best.hasProcessModule = hasProcessModule;
			best.hasClientModule = hasClientModule;
		}
	}

	VMMDLL_MemFree(process_info);
	if (best.pid)
		return best.pid;

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE)
		return 0;

	PROCESSENTRY32W entry = {};
	entry.dwSize = sizeof(entry);
	DWORD win32Pid = 0;
	const std::wstring targetWideA(target_a.begin(), target_a.end());
	const std::wstring targetWideB(target_b.begin(), target_b.end());
	const std::wstring targetWideC(target_c.begin(), target_c.end());
	if (Process32FirstW(snapshot, &entry))
	{
		do
		{
			std::wstring exeName = entry.szExeFile;
			std::transform(exeName.begin(), exeName.end(), exeName.begin(), [](wchar_t c) {
				return static_cast<wchar_t>(std::towlower(c));
			});
			if (exeName == targetWideA || exeName == targetWideB || exeName == targetWideC)
			{
				win32Pid = entry.th32ProcessID;
				break;
			}
		} while (Process32NextW(snapshot, &entry));
	}

	CloseHandle(snapshot);
	return win32Pid;
}

std::vector<int> Memory::GetPidListFromName(const std::string& name)
{
	PVMMDLL_PROCESS_INFORMATION process_info = NULL;
	DWORD total_processes = 0;
	std::vector<int> list = { };

	if (!VMMDLL_ProcessGetInformationAll(this->vHandle, &process_info, &total_processes))
	{
		LOG("[!] Failed to get process list\n");
		return list;
	}

	for (size_t i = 0; i < total_processes; i++)
	{
		auto process = process_info[i];
		if (process.szNameLong && strstr(process.szNameLong, name.c_str()))
			list.push_back(process.dwPID);
	}

	VMMDLL_MemFree(process_info);
	return list;
}

std::vector<std::string> Memory::GetModuleList(const std::string& process_name)
{
	std::vector<std::string> list = { };
	PVMMDLL_MAP_MODULE module_info = NULL;
	if (!VMMDLL_Map_GetModuleU(this->vHandle, current_process.PID, &module_info, VMMDLL_MODULE_FLAG_NORMAL))
	{
		LOG("[!] Failed to get module list\n");
		return list;
	}

	for (size_t i = 0; i < module_info->cMap; i++)
	{
		auto module = module_info->pMap[i];
		list.push_back(module.uszText);
	}

	VMMDLL_MemFree(module_info);
	return list;
}

VMMDLL_PROCESS_INFORMATION Memory::GetProcessInformation()
{
	VMMDLL_PROCESS_INFORMATION info = { };
	SIZE_T process_information = sizeof(VMMDLL_PROCESS_INFORMATION);
	ZeroMemory(&info, sizeof(VMMDLL_PROCESS_INFORMATION));
	info.magic = VMMDLL_PROCESS_INFORMATION_MAGIC;
	info.wVersion = VMMDLL_PROCESS_INFORMATION_VERSION;

	if (!VMMDLL_ProcessGetInformation(this->vHandle, current_process.PID, &info, &process_information))
	{
		LOG("[!] Failed to find process information\n");
		return { };
	}

	LOG("[+] Found process information\n");
	return info;
}

PEB Memory::GetProcessPeb()
{
	auto info = GetProcessInformation();
	if (info.win.vaPEB)
	{
		LOG("[+] Found process PEB ptr at 0x%p\n", info.win.vaPEB);
		return Read<PEB>(info.win.vaPEB);
	}
	LOG("[!] Failed to find the processes PEB\n");
	return { };
}

size_t Memory::GetBaseDaddy(std::string module_name)
{
	// Fast/robust path used by other DMA projects: direct module base query.
	ULONG64 base = VMMDLL_ProcessGetModuleBaseU(this->vHandle, current_process.PID, module_name.c_str());
	if (base)
	{
		LOG("[INFO] Base address resolved for %s at 0x%p.\n", module_name.c_str(), base);
		return static_cast<size_t>(base);
	}

	// Fallback: map lookup by wide-string name.
	std::wstring str(module_name.begin(), module_name.end());
	PVMMDLL_MAP_MODULEENTRY module_info;
	if (VMMDLL_Map_GetModuleFromNameW(this->vHandle, current_process.PID, const_cast<LPWSTR>(str.c_str()), &module_info, VMMDLL_MODULE_FLAG_NORMAL))
	{
		LOG("[INFO] Base address resolved for %s at 0x%p.\n", module_name.c_str(), module_info->vaBase);
		return module_info->vaBase;
	}

	return 0;
}

size_t Memory::GetBaseSize(std::string module_name)
{
	std::wstring str(module_name.begin(), module_name.end());

	PVMMDLL_MAP_MODULEENTRY module_info;
	auto bResult = VMMDLL_Map_GetModuleFromNameW(this->vHandle, current_process.PID, const_cast<LPWSTR>(str.c_str()), &module_info, VMMDLL_MODULE_FLAG_NORMAL);
	if (bResult)
	{
		LOG("[INFO] Image size resolved for %s at 0x%llx.\n", module_name.c_str(), static_cast<unsigned long long>(module_info->cbImageSize));
		return module_info->cbImageSize;
	}
	return 0;
}

uintptr_t Memory::GetExportTableAddress(std::string import, std::string process, std::string module)
{
	PVMMDLL_MAP_EAT eat_map = NULL;
	PVMMDLL_MAP_EATENTRY export_entry = NULL;
	bool result = VMMDLL_Map_GetEATU(mem.vHandle, mem.GetPidFromName(process) /*| VMMDLL_PID_PROCESS_WITH_KERNELMEMORY*/, const_cast<LPSTR>(module.c_str()), &eat_map);
	if (!result)
	{
		LOG("[!] Failed to get Export Table\n");
		return 0;
	}

	if (eat_map->dwVersion != VMMDLL_MAP_EAT_VERSION)
	{
		VMMDLL_MemFree(eat_map);
		eat_map = NULL;
		LOG("[!] Invalid VMM Map Version\n");
		return 0;
	}

	uintptr_t addr = 0;
	for (int i = 0; i < eat_map->cMap; i++)
	{
		export_entry = eat_map->pMap + i;
		if (strcmp(export_entry->uszFunction, import.c_str()) == 0)
		{
			addr = export_entry->vaFunction;
			break;
		}
	}

	VMMDLL_MemFree(eat_map);
	eat_map = NULL;

	return addr;
}

uintptr_t Memory::GetImportTableAddress(std::string import, std::string process, std::string module)
{
	PVMMDLL_MAP_IAT iat_map = NULL;
	PVMMDLL_MAP_IATENTRY import_entry = NULL;
	bool result = VMMDLL_Map_GetIATU(mem.vHandle, mem.GetPidFromName(process) /*| VMMDLL_PID_PROCESS_WITH_KERNELMEMORY*/, const_cast<LPSTR>(module.c_str()), &iat_map);
	if (!result)
	{
		LOG("[!] Failed to get Import Table\n");
		return 0;
	}

	if (iat_map->dwVersion != VMMDLL_MAP_IAT_VERSION)
	{
		VMMDLL_MemFree(iat_map);
		iat_map = NULL;
		LOG("[!] Invalid VMM Map Version\n");
		return 0;
	}

	uintptr_t addr = 0;
	for (int i = 0; i < iat_map->cMap; i++)
	{
		import_entry = iat_map->pMap + i;
		if (strcmp(import_entry->uszFunction, import.c_str()) == 0)
		{
			addr = import_entry->vaFunction;
			break;
		}
	}

	VMMDLL_MemFree(iat_map);
	iat_map = NULL;

	return addr;
}

uint64_t cbSize = 0x80000;
//callback for VfsFileListU
VOID cbAddFile(_Inout_ HANDLE h, _In_ LPCSTR uszName, _In_ ULONG64 cb, _In_opt_ PVMMDLL_VFS_FILELIST_EXINFO pExInfo)
{
	if (strcmp(uszName, "dtb.txt") == 0)
		cbSize = cb;
}

struct Info
{
	uint32_t index;
	uint32_t process_id;
	uint64_t dtb;
	uint64_t kernelAddr;
	std::string name;
};

bool Memory::FixCr3()
{
	auto to_lower = [](std::string s) {
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return s;
	};
	auto strip_exe = [&](std::string s) {
		std::string lower = to_lower(s);
		if (lower.size() > 4 && lower.substr(lower.size() - 4) == ".exe")
			return s.substr(0, s.size() - 4);
		return s;
	};
	auto canAccessProbeModules = [&](DWORD pid) -> bool {
		auto hasModule = [&](const std::string& moduleName) -> bool {
			if (moduleName.empty()) return false;
			uint64_t base = VMMDLL_ProcessGetModuleBaseU(this->vHandle, pid, moduleName.c_str());
			if (base == 0) return false;
			
			IMAGE_DOS_HEADER dos = { 0 };
			DWORD read_size = 0;
			if (!VMMDLL_MemReadEx(this->vHandle, pid, base, (PBYTE)&dos, sizeof(IMAGE_DOS_HEADER), &read_size, VMMDLL_FLAG_NOCACHE))
				return false;
			return dos.e_magic == 0x5A4D;
		};

		const std::string targetLower = to_lower(current_process.process_name);
		const std::string targetBase = strip_exe(current_process.process_name);
		const std::string targetBaseLower = to_lower(targetBase);
		const bool isCs2Target =
			targetLower == "cs2.exe" ||
			targetLower == "cs2" ||
			targetBaseLower == "cs2";

		if (isCs2Target)
			return hasModule("client.dll") || hasModule("engine2.dll");

		return hasModule(current_process.process_name) ||
			(targetBase != current_process.process_name && hasModule(targetBase));
	};

	if (this->vHandle)
		VMMDLL_ConfigSet(this->vHandle, VMMDLL_OPT_REFRESH_ALL, 1);

	if (canAccessProbeModules(current_process.PID))
		return true;

	if (!s_fixCr3PluginsInitialized)
	{
		if (!VMMDLL_InitializePlugins(this->vHandle))
		{
			LOG("[ERROR] VMMDLL_InitializePlugins call failed.\n");
			return false;
		}
		s_fixCr3PluginsInitialized = true;
	}

	//have to sleep a little or we try reading the file before the plugin initializes fully
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	while (true)
	{
		BYTE bytes[4] = {0};
		DWORD i = 0;
		auto nt = VMMDLL_VfsReadW(this->vHandle, const_cast<LPWSTR>(L"\\misc\\procinfo\\progress_percent.txt"), bytes, 3, &i, 0);
		if (nt == VMMDLL_STATUS_SUCCESS && atoi(reinterpret_cast<LPSTR>(bytes)) == 100)
			break;

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	VMMDLL_VFS_FILELIST2 VfsFileList;
	VfsFileList.dwVersion = VMMDLL_VFS_FILELIST_VERSION;
	VfsFileList.h = 0;
	VfsFileList.pfnAddDirectory = 0;
	VfsFileList.pfnAddFile = cbAddFile; //dumb af callback who made this system

	const bool result = VMMDLL_VfsListU(this->vHandle, const_cast<LPSTR>("\\misc\\procinfo\\"), &VfsFileList);
	if (!result)
		return false;

	//read the data from the txt and parse it
	const size_t buffer_size = cbSize;
	std::unique_ptr<BYTE[]> bytes(new BYTE[buffer_size]);
	DWORD j = 0;
	auto nt = VMMDLL_VfsReadW(this->vHandle, const_cast<LPWSTR>(L"\\misc\\procinfo\\dtb.txt"), bytes.get(), buffer_size - 1, &j, 0);
	if (nt != VMMDLL_STATUS_SUCCESS)
		return false;

	std::vector<uint64_t> possible_dtbs = { };
	std::string lines(reinterpret_cast<char*>(bytes.get()));
	std::istringstream iss(lines);
	std::string line = "";

	while (std::getline(iss, line))
	{
		Info info = { };

		std::istringstream info_ss(line);
		if (info_ss >> std::hex >> info.index >> std::dec >> info.process_id >> std::hex >> info.dtb >> info.kernelAddr >> info.name)
		{
			if (info.process_id == 0) //parts that lack a name or have a NULL pid are suspects
				possible_dtbs.push_back(info.dtb);
			if (current_process.process_name.find(info.name) != std::string::npos)
				possible_dtbs.push_back(info.dtb);
		}
	}

	//loop over possible dtbs and set the config to use it til we find the correct one
	for (size_t i = 0; i < possible_dtbs.size(); i++)
	{
		auto dtb = possible_dtbs[i];
		VMMDLL_ConfigSet(this->vHandle, VMMDLL_OPT_PROCESS_DTB | current_process.PID, dtb);
		VMMDLL_ConfigSet(this->vHandle, VMMDLL_OPT_REFRESH_ALL, 1);
		if (canAccessProbeModules(current_process.PID))
		{
			LOG("[INFO] DTB remediation completed.\n");
			return true;
		}
	}

	LOG("[WARN] DTB remediation did not find a valid candidate.\n");
	VMMDLL_ConfigSet(this->vHandle, VMMDLL_OPT_PROCESS_DTB | current_process.PID, 0);
	VMMDLL_ConfigSet(this->vHandle, VMMDLL_OPT_REFRESH_ALL, 1);
	return false;
}

bool Memory::DumpMemory(uintptr_t address, std::string path)
{
	LOG("[!] Memory dumping currently does not rebuild the IAT table, imports will be missing from the dump.\n");
	IMAGE_DOS_HEADER dos { };
	Read(address, &dos, sizeof(IMAGE_DOS_HEADER));

	//Check if memory has a PE 
	if (dos.e_magic != 0x5A4D) //Check if it starts with MZ
	{
		LOG("[-] Invalid PE Header\n");
		return false;
	}

	IMAGE_NT_HEADERS64 nt;
	Read(address + dos.e_lfanew, &nt, sizeof(IMAGE_NT_HEADERS64));

	//Sanity check
	if (nt.Signature != IMAGE_NT_SIGNATURE || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
	{
		LOG("[-] Failed signature check\n");
		return false;
	}
	//Shouldn't change ever. so const 
	const size_t target_size = nt.OptionalHeader.SizeOfImage;
	//Crashes if we don't make it a ptr :(
	auto target = std::unique_ptr<uint8_t[]>(new uint8_t[target_size]);

	//Read whole modules memory
	Read(address, target.get(), target_size);
	auto nt_header = (PIMAGE_NT_HEADERS64)(target.get() + dos.e_lfanew);
	auto sections = (PIMAGE_SECTION_HEADER)(target.get() + dos.e_lfanew + FIELD_OFFSET(IMAGE_NT_HEADERS, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader);

	for (size_t i = 0; i < nt.FileHeader.NumberOfSections; i++, sections++)
	{
		//Rewrite the file offsets to the virtual addresses
		LOG("[!] Rewriting file offsets at 0x%08X size 0x%08X\n", sections->VirtualAddress, sections->Misc.VirtualSize);
		sections->PointerToRawData = sections->VirtualAddress;
		sections->SizeOfRawData = sections->Misc.VirtualSize;
	}

	auto debug = (PIMAGE_DEBUG_DIRECTORY)(target.get() + nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress);
	debug->PointerToRawData = debug->AddressOfRawData;

	// IAT rebuild removed — not used

	//Create New Import Section

	//Build new import Table

	//Dump file
	const auto dumped_file = CreateFileW(std::wstring(path.begin(), path.end()).c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_COMPRESSED, NULL);
	if (dumped_file == INVALID_HANDLE_VALUE)
	{
		LOG("[!] Failed creating file: %i\n", GetLastError());
		return false;
	}

	if (!WriteFile(dumped_file, target.get(), static_cast<DWORD>(target_size), NULL, NULL))
	{
		LOG("[!] Failed writing file: %i\n", GetLastError());
		CloseHandle(dumped_file);
		return false;
	}

	LOG("[+] Successfully dumped memory at %s\n", path.c_str());
	CloseHandle(dumped_file);
	return true;
}

static const char* hexdigits =
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\001\002\003\004\005\006\007\010\011\000\000\000\000\000\000"
	"\000\012\013\014\015\016\017\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\012\013\014\015\016\017\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
	"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000";

static uint8_t GetByte(const char* hex)
{
	return static_cast<uint8_t>((hexdigits[hex[0]] << 4) | (hexdigits[hex[1]]));
}

uint64_t Memory::FindSignature(const char* signature, uint64_t range_start, uint64_t range_end, int PID)
{
	if (!signature || signature[0] == '\0' || range_start >= range_end)
		return 0;

	if (PID == 0)
		PID = current_process.PID;

	std::vector<uint8_t> buffer(range_end - range_start);
	if (!VMMDLL_MemReadEx(this->vHandle, PID, range_start, buffer.data(), buffer.size(), 0, VMMDLL_FLAG_NOCACHE))
		return 0;

	const char* pat = signature;
	uint64_t first_match = 0;
	for (uint64_t i = range_start; i < range_end; i++)
	{
		if (*pat == '?' || buffer[i - range_start] == GetByte(pat))
		{
			if (!first_match)
				first_match = i;

			if (!pat[2])
				break;

			pat += (*pat == '?') ? 2 : 3;
		}
		else
		{
			pat = signature;
			first_match = 0;
		}
	}

	return first_match;
}

bool Memory::Write(uintptr_t address, void* buffer, size_t size) const
{
	UNREFERENCED_PARAMETER(address);
	UNREFERENCED_PARAMETER(buffer);
	UNREFERENCED_PARAMETER(size);
	LogWriteBlockedOnce(g_memoryWriteBlockedLogged, "Memory::Write");
	return false;
}

bool Memory::Write(uintptr_t address, void* buffer, size_t size, int pid) const
{
	UNREFERENCED_PARAMETER(address);
	UNREFERENCED_PARAMETER(buffer);
	UNREFERENCED_PARAMETER(size);
	UNREFERENCED_PARAMETER(pid);
	LogWriteBlockedOnce(g_memoryWriteBlockedLogged, "Memory::Write(pid)");
	return false;
}

bool Memory::Read(uintptr_t address, void* buffer, size_t size) const
{
	ApplyDirectReadJitter();
	DWORD read_size = 0;
	if (!VMMDLL_MemReadEx(this->vHandle, current_process.PID, address, static_cast<PBYTE>(buffer), size, &read_size, VMMDLL_FLAG_NOCACHE))
	{
		LOG("[!] Failed to read Memory at 0x%p\n", address);
		return false;
	}

	return (read_size == size);
}

bool Memory::Read(uintptr_t address, void* buffer, size_t size, int pid) const
{
	ApplyDirectReadJitter();
	DWORD read_size = 0;
	if (!VMMDLL_MemReadEx(this->vHandle, pid, address, static_cast<PBYTE>(buffer), size, &read_size, VMMDLL_FLAG_NOCACHE))
	{
		LOG("[!] Failed to read Memory at 0x%p\n", address);
		return false;
	}
	return (read_size == size);
}

VMMDLL_SCATTER_HANDLE Memory::CreateScatterHandle() const
{
	SCATTER_PENDING_COUNT = 0;
	SCATTER_PREPARE_FAIL_COUNT = 0;
	const VMMDLL_SCATTER_HANDLE ScatterHandle = VMMDLL_Scatter_Initialize(this->vHandle, current_process.PID, VMMDLL_FLAG_NOCACHE);
	if (!ScatterHandle)
		LOG("[!] Failed to create scatter handle\n");
	return ScatterHandle;
}

VMMDLL_SCATTER_HANDLE Memory::CreateScatterHandle(int pid) const
{
	SCATTER_PENDING_COUNT = 0;
	SCATTER_PREPARE_FAIL_COUNT = 0;
	const VMMDLL_SCATTER_HANDLE ScatterHandle = VMMDLL_Scatter_Initialize(this->vHandle, pid, VMMDLL_FLAG_NOCACHE);
	if (!ScatterHandle)
		LOG("[!] Failed to create scatter handle\n");
	return ScatterHandle;
}

void Memory::CloseScatterHandle(VMMDLL_SCATTER_HANDLE handle)
{
	SCATTER_PENDING_COUNT = 0;
	SCATTER_PREPARE_FAIL_COUNT = 0;
	VMMDLL_Scatter_CloseHandle(handle);
}

void Memory::AddScatterReadRequest(VMMDLL_SCATTER_HANDLE handle, uint64_t address, void* buffer, size_t size)
{
	if (!VMMDLL_Scatter_PrepareEx(handle, address, size, static_cast<PBYTE>(buffer), NULL))
	{
		++SCATTER_PREPARE_FAIL_COUNT;
	}
	else
	{
		++SCATTER_PENDING_COUNT;
	}
}

void Memory::AddScatterWriteRequest(VMMDLL_SCATTER_HANDLE handle, uint64_t address, void* buffer, size_t size)
{
	UNREFERENCED_PARAMETER(handle);
	UNREFERENCED_PARAMETER(address);
	UNREFERENCED_PARAMETER(buffer);
	UNREFERENCED_PARAMETER(size);
	LogWriteBlockedOnce(g_scatterWriteBlockedLogged, "Memory::AddScatterWriteRequest");
}

bool Memory::ExecuteReadScatter(VMMDLL_SCATTER_HANDLE handle, int pid)
{
	if (!handle)
		return false;

	ApplyScatterReadJitter();

	if (pid == 0)
		pid = current_process.PID;

	const uint32_t pending = SCATTER_PENDING_COUNT;
	const uint32_t prepareFails = SCATTER_PREPARE_FAIL_COUNT;
	SCATTER_PENDING_COUNT = 0;
	SCATTER_PREPARE_FAIL_COUNT = 0;

	// If no requests were queued since the last execute/clear, just clear and
	// return success. VMMDLL_Scatter_ExecuteRead can fail or behave
	// unpredictably with zero pending requests on some FPGA firmware versions.
	if (pending == 0)
	{
		VMMDLL_Scatter_Clear(handle, pid, VMMDLL_FLAG_NOCACHE);
		// All prepares failed — buffers contain stale data, report failure.
		return (prepareFails == 0);
	}

	for (int attempt = 0; attempt < 2; ++attempt)
	{
		if (VMMDLL_Scatter_ExecuteRead(handle))
		{
			if (!VMMDLL_Scatter_Clear(handle, pid, VMMDLL_FLAG_NOCACHE))
				return false;
			return true;
		}
		Sleep(1);
	}

	static uint32_t s_scatterReadFailureCount = 0;
	++s_scatterReadFailureCount;
	if (!SCATTER_WARN_SUPPRESSED.load(std::memory_order_relaxed) &&
		(s_scatterReadFailureCount == 1 || (s_scatterReadFailureCount % 250) == 0))
	{
		LOG("[WARN] Scatter read failed (%u total). Check UpdateData stage/reason logs for exact failed block.\n", s_scatterReadFailureCount);
	}

	// Try to clear queue even on failure to avoid reusing stale requests.
	VMMDLL_Scatter_Clear(handle, pid, VMMDLL_FLAG_NOCACHE);
	return false;
}

void Memory::SetScatterReadWarningSuppressed(bool suppressed)
{
	SCATTER_WARN_SUPPRESSED.store(suppressed, std::memory_order_relaxed);
}

void Memory::ExecuteWriteScatter(VMMDLL_SCATTER_HANDLE handle, int pid)
{
	UNREFERENCED_PARAMETER(handle);
	UNREFERENCED_PARAMETER(pid);
	LogWriteBlockedOnce(g_scatterWriteBlockedLogged, "Memory::ExecuteWriteScatter");
}
