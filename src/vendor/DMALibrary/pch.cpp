// pch.cpp: source file corresponding to the pre-compiled header

#include "pch.h"

#include <atomic>
#include <cctype>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cwchar>
#include <cwctype>

namespace
{
	std::atomic<DmaLogLevel> g_dmaLogLevel { DmaLogLevel::Warning };

	template <typename CharT>
	void TrimTrailingLineBreaks(std::basic_string<CharT>& value)
	{
		while (!value.empty()) {
			const CharT ch = value.back();
			if (ch == static_cast<CharT>('\n') || ch == static_cast<CharT>('\r'))
				value.pop_back();
			else
				break;
		}
	}

	DmaLogLevel ParseLogLevel(const char* raw)
	{
		if (!raw || !raw[0])
			return DmaLogLevel::Warning;

		std::string value(raw);
		for (char& c : value)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

		if (value == "error")
			return DmaLogLevel::Error;
		if (value == "warn" || value == "warning")
			return DmaLogLevel::Warning;
		if (value == "debug")
			return DmaLogLevel::Debug;
		if (value == "info")
			return DmaLogLevel::Info;
		if (value == "silent" || value == "off" || value == "none")
			return DmaLogLevel::Silent;
		return DmaLogLevel::Warning;
	}

	DmaLogLevel LevelFromMessage(const char* fmt)
	{
		if (!fmt)
			return DmaLogLevel::Info;
		if (std::strncmp(fmt, "[ERROR]", 7) == 0)
			return DmaLogLevel::Error;
		if (std::strncmp(fmt, "[WARN]", 6) == 0 || std::strncmp(fmt, "[!]", 3) == 0 || std::strncmp(fmt, "[-]", 3) == 0)
			return DmaLogLevel::Warning;
		if (std::strncmp(fmt, "[DEBUG]", 7) == 0)
			return DmaLogLevel::Debug;
		return DmaLogLevel::Info;
	}

	DmaLogLevel LevelFromMessage(const wchar_t* fmt)
	{
		if (!fmt)
			return DmaLogLevel::Info;
		if (wcsncmp(fmt, L"[ERROR]", 7) == 0)
			return DmaLogLevel::Error;
		if (wcsncmp(fmt, L"[WARN]", 6) == 0 || wcsncmp(fmt, L"[!]", 3) == 0 || wcsncmp(fmt, L"[-]", 3) == 0)
			return DmaLogLevel::Warning;
		if (wcsncmp(fmt, L"[DEBUG]", 7) == 0)
			return DmaLogLevel::Debug;
		return DmaLogLevel::Info;
	}

	bool ShouldPrint(DmaLogLevel msgLevel)
	{
		const DmaLogLevel currentLevel = g_dmaLogLevel.load();
		if (currentLevel == DmaLogLevel::Silent)
			return false;
		return static_cast<int>(msgLevel) <= static_cast<int>(currentLevel);
	}

	const char* PrefixToStrip(const char* fmt)
	{
		if (!fmt)
			return "";
		if (std::strncmp(fmt, "[ERROR]", 7) == 0)
			return fmt + 7;
		if (std::strncmp(fmt, "[WARN]", 6) == 0)
			return fmt + 6;
		if (std::strncmp(fmt, "[INFO]", 6) == 0)
			return fmt + 6;
		if (std::strncmp(fmt, "[DEBUG]", 7) == 0)
			return fmt + 7;
		if (std::strncmp(fmt, "[!]", 3) == 0 || std::strncmp(fmt, "[-]", 3) == 0 || std::strncmp(fmt, "[+]", 3) == 0)
			return fmt + 3;
		return fmt;
	}

	const wchar_t* PrefixToStrip(const wchar_t* fmt)
	{
		if (!fmt)
			return L"";
		if (wcsncmp(fmt, L"[ERROR]", 7) == 0)
			return fmt + 7;
		if (wcsncmp(fmt, L"[WARN]", 6) == 0)
			return fmt + 6;
		if (wcsncmp(fmt, L"[INFO]", 6) == 0)
			return fmt + 6;
		if (wcsncmp(fmt, L"[DEBUG]", 7) == 0)
			return fmt + 7;
		if (wcsncmp(fmt, L"[!]", 3) == 0 || wcsncmp(fmt, L"[-]", 3) == 0 || wcsncmp(fmt, L"[+]", 3) == 0)
			return fmt + 3;
		return fmt;
	}

	const char* LabelForLevel(DmaLogLevel level)
	{
		switch (level) {
		case DmaLogLevel::Error:   return "Error";
		case DmaLogLevel::Warning: return "Warning";
		case DmaLogLevel::Debug:   return "Debug";
		case DmaLogLevel::Info:
		default:                   return "Info";
		}
	}

	const wchar_t* LabelForLevelW(DmaLogLevel level)
	{
		switch (level) {
		case DmaLogLevel::Error:   return L"Error";
		case DmaLogLevel::Warning: return L"Warning";
		case DmaLogLevel::Debug:   return L"Debug";
		case DmaLogLevel::Info:
		default:                   return L"Info";
		}
	}

	void ApplyColorForLevel(DmaLogLevel level, WORD* outOld);
	void RestoreColor(WORD oldAttributes);

	void PrintStyledLine(const std::string& message, DmaLogLevel level)
	{
		WORD oldAttributes = 0;
		std::fputs("\r  ", stdout);
		ApplyColorForLevel(level, &oldAttributes);
		std::printf("| %s | ", LabelForLevel(level));
		RestoreColor(oldAttributes);
		std::printf("%s                    \n", message.c_str());
	}

	void PrintStyledLine(const std::wstring& message, DmaLogLevel level)
	{
		WORD oldAttributes = 0;
		std::fputws(L"\r  ", stdout);
		ApplyColorForLevel(level, &oldAttributes);
		std::wprintf(L"| %ls | ", LabelForLevelW(level));
		RestoreColor(oldAttributes);
		std::wprintf(L"%ls                    \n", message.c_str());
	}

	WORD ColorForLevel(DmaLogLevel level)
	{
		switch (level) {
		case DmaLogLevel::Error:   return FOREGROUND_RED | FOREGROUND_INTENSITY;
		case DmaLogLevel::Warning: return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
		case DmaLogLevel::Info:    return FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
		case DmaLogLevel::Debug:   return FOREGROUND_BLUE | FOREGROUND_GREEN;
		default:                   return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
		}
	}

	void ApplyColorForLevel(DmaLogLevel level, WORD* outOld)
	{
		if (outOld)
			*outOld = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

		HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
		if (h == INVALID_HANDLE_VALUE)
			return;

		CONSOLE_SCREEN_BUFFER_INFO info = {};
		if (GetConsoleScreenBufferInfo(h, &info) && outOld)
			*outOld = info.wAttributes;

		SetConsoleTextAttribute(h, ColorForLevel(level));
	}

	void RestoreColor(WORD oldAttributes)
	{
		HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
		if (h == INVALID_HANDLE_VALUE)
			return;
		SetConsoleTextAttribute(h, oldAttributes);
	}

	struct DmaLogInit
	{
		DmaLogInit()
		{
			char* envValue = nullptr;
			size_t envLen = 0;
			if (_dupenv_s(&envValue, &envLen, "KEVQDMA_LOG_LEVEL") == 0 && envValue)
			{
				g_dmaLogLevel.store(ParseLogLevel(envValue));
			}
			if (envValue)
				free(envValue);
		}
	} g_dmaLogInit;
}

void DmaSetLogLevel(DmaLogLevel level)
{
	g_dmaLogLevel.store(level);
}

DmaLogLevel DmaGetLogLevel()
{
	return g_dmaLogLevel.load();
}

void DmaLogPrintf(const char* fmt, ...)
{
	const DmaLogLevel level = LevelFromMessage(fmt);
	if (!fmt || !ShouldPrint(level))
		return;

	va_list args;
	va_start(args, fmt);
	char buffer[4096] = {};
	std::vsnprintf(buffer, sizeof(buffer), PrefixToStrip(fmt), args);
	va_end(args);
	std::string message(buffer);
	while (!message.empty() && std::isspace(static_cast<unsigned char>(message.front())))
		message.erase(message.begin());
	TrimTrailingLineBreaks(message);
	if (message.empty())
		return;
	PrintStyledLine(message, level);
}

void DmaLogWPrintf(const wchar_t* fmt, ...)
{
	const DmaLogLevel level = LevelFromMessage(fmt);
	if (!fmt || !ShouldPrint(level))
		return;

	va_list args;
	va_start(args, fmt);
	wchar_t buffer[4096] = {};
	_vsnwprintf_s(buffer, _countof(buffer), _TRUNCATE, PrefixToStrip(fmt), args);
	va_end(args);
	std::wstring message(buffer);
		while (!message.empty() && std::iswspace(message.front()))
			message.erase(message.begin());
	TrimTrailingLineBreaks(message);
	if (message.empty())
		return;
	PrintStyledLine(message, level);
}
