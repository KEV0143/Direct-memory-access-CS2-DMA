#include "pch.h"
#include "Registry.h"
#include "Memory.h"

namespace
{
	std::string WideToUtf8(const wchar_t* wide)
	{
		if (!wide || !wide[0])
			return {};

		const int required = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
		if (required <= 1)
			return {};

		std::string utf8(static_cast<size_t>(required - 1), '\0');
		WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8.data(), required, nullptr, nullptr);
		return utf8;
	}
}

std::string c_registry::QueryValue(const char* path, e_registry_type type)
{
	if (!mem.vHandle)
		return "";

	BYTE buffer[0x400];
	DWORD _type = static_cast<DWORD>(type);
	DWORD size = sizeof(buffer);

	char pathBuf[512];
	strncpy_s(pathBuf, path, _TRUNCATE);
	if (!VMMDLL_WinReg_QueryValueExU(mem.vHandle, pathBuf, &_type, buffer, &size))
	{
		LOG("[!] failed QueryValueExU call\n");
		return "";
	}

	if (type == e_registry_type::dword)
	{
		DWORD dwordValue;
		memcpy(&dwordValue, buffer, sizeof(DWORD));
		return std::to_string(dwordValue);
	}

	return WideToUtf8(reinterpret_cast<const wchar_t*>(buffer));
}
