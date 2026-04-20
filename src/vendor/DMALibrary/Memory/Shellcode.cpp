#include "pch.h"
#include "Shellcode.h"
#include "Memory.h"

std::vector<std::string> blacklist = {"kernel32.dll", "kernelbase.dll", "wow64.dll", "wow64win.dll", "wow64cpu.dll", "ntoskrnl.exe", "win32kbase.sys"};
namespace {
	std::atomic<bool> g_shellcodeBlockedLogged = false;
}

uint64_t c_shellcode::find_codecave(size_t function_size, const std::string& process_name, const std::string& module)
{
	int pid = mem.GetPidFromName(process_name);
	VMMDLL_PROCESS_INFORMATION process_info = {0};
	process_info.magic = VMMDLL_PROCESS_INFORMATION_MAGIC;
	process_info.wVersion = VMMDLL_PROCESS_INFORMATION_VERSION;
	SIZE_T process_info_size = sizeof(VMMDLL_PROCESS_INFORMATION);
	if (!VMMDLL_ProcessGetInformation(mem.vHandle, pid, &process_info, &process_info_size))
	{
		LOG("[!] Could not retrieve process for PID: %i", pid);
		return 0;
	}

	DWORD cSections = 0;
	if (!VMMDLL_ProcessGetSectionsU(mem.vHandle, pid, const_cast<LPSTR>(module.c_str())  /* VMMDLL API takes non-const LPSTR but doesn't modify */, NULL, 0, &cSections) || !cSections)
	{
		LOG("[!] Could not retrieve sections #1 for '%s'\n", module.c_str());
		return 0;
	}

	const PIMAGE_SECTION_HEADER pSections = static_cast<PIMAGE_SECTION_HEADER>(LocalAlloc(LMEM_ZEROINIT, cSections * sizeof(IMAGE_SECTION_HEADER)));
	if (!pSections || !VMMDLL_ProcessGetSectionsU(mem.vHandle, pid, const_cast<LPSTR>(module.c_str())  /* VMMDLL API takes non-const LPSTR but doesn't modify */, pSections, cSections, &cSections) || !cSections)
	{
		LOG("[!] Could not retrieve sections #2 for '%s'\n", module.c_str());
		if (pSections) LocalFree(pSections);
		return 0;
	}

	/*Scan for code cave*/
	uint64_t codecave = 0;
	for (DWORD i = 0; i < cSections; i++)
	{
		if (!codecave && ((pSections[i].Characteristics & (IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_READ))) && ((pSections[i].Misc.VirtualSize & 0xfff) < (0x1000 - function_size)))
		{
			codecave = VMMDLL_ProcessGetModuleBaseU(mem.vHandle, pid, const_cast<LPSTR>(module.c_str())  /* VMMDLL API takes non-const LPSTR but doesn't modify */) + ((pSections[i].VirtualAddress + pSections[i].Misc.VirtualSize)) + 0x10;
			if (!codecave)
				break;
		}
	}

	LocalFree(pSections);

	if (!codecave)
	{
		LOG("[!] Could not find a code cave for '%s'\n", module.c_str());
		return 0;
	}

	auto buffer = std::unique_ptr<uint8_t[]>(new uint8_t[function_size]);
	if (!mem.Read(codecave, buffer.get(), function_size, pid))
	{
		LOG("[!] Could not read codecave for '%s'\n", module.c_str());
		return 0;
	}

	for (size_t i = 0; i < function_size; i++)
	{
		if (buffer[i] != 0x0)
		{
			LOG("[!] Codecave isn't big enough for the shellcode.\n");
			return 0;
		}
	}

	return codecave;
}

std::vector<uint64_t> c_shellcode::find_all_codecave(size_t function_size, const std::string& process_name)
{
	std::vector<uint64_t> codecaves = { };
	std::vector<std::string> module_list = mem.GetModuleList(process_name);
	for (size_t i = 0; i < module_list.size(); i++)
	{
		if (std::find(blacklist.begin(), blacklist.end(), module_list[i]) != blacklist.end())
			continue;

		std::string module = module_list[i];
		uint64_t codecave = find_codecave(function_size, process_name, module);
		if (!codecave)
			continue;
		codecaves.push_back(codecave);
	}
	return codecaves;
}

bool c_shellcode::call_function(void* hook, void* function, const std::string& process_name)
{
	UNREFERENCED_PARAMETER(hook);
	UNREFERENCED_PARAMETER(function);
	UNREFERENCED_PARAMETER(process_name);
	bool expected = false;
	if (g_shellcodeBlockedLogged.compare_exchange_strong(expected, true, std::memory_order_relaxed))
		LOG("[WARN] Shellcode helper blocked: DMA transport is enforced read-only.\n");
	return false;
}
