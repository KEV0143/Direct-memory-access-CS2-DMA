#include "app/Bootstrap/crash_handler.h"

#include <Windows.h>
#include <DbgHelp.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#pragma comment(lib, "Dbghelp.lib")

namespace {

std::atomic<bool> g_installed{false};
LPTOP_LEVEL_EXCEPTION_FILTER g_previousFilter = nullptr;

void BuildDumpPath(wchar_t (&buf)[MAX_PATH]) {
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) lastSlash[1] = L'\0';
    else exePath[0] = L'\0';

    SYSTEMTIME st{};
    GetLocalTime(&st);

    swprintf_s(buf, MAX_PATH, L"%sKevqDMA-%04u%02u%02u-%02u%02u%02u.dmp",
               exePath, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}

LONG WINAPI UnhandledFilter(EXCEPTION_POINTERS* info) {
    wchar_t dumpPath[MAX_PATH];
    BuildDumpPath(dumpPath);

    HANDLE hFile = CreateFileW(dumpPath, GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = info;
        mei.ClientPointers = FALSE;

        const MINIDUMP_TYPE type = static_cast<MINIDUMP_TYPE>(
            MiniDumpWithThreadInfo |
            MiniDumpWithIndirectlyReferencedMemory |
            MiniDumpScanMemory);

        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                          hFile, type, &mei, nullptr, nullptr);
        CloseHandle(hFile);

        char asciiPath[MAX_PATH] = {};
        WideCharToMultiByte(CP_UTF8, 0, dumpPath, -1, asciiPath, MAX_PATH, nullptr, nullptr);
        std::fprintf(stderr, "\n[KevqDMA] crashed; minidump written to %s\n", asciiPath);
    } else {
        std::fprintf(stderr, "\n[KevqDMA] crashed; failed to write minidump\n");
    }

    if (g_previousFilter)
        return g_previousFilter(info);
    return EXCEPTION_EXECUTE_HANDLER;
}

}

namespace bootstrap {

void InstallCrashHandler() {
    bool expected = false;
    if (!g_installed.compare_exchange_strong(expected, true))
        return;
    g_previousFilter = SetUnhandledExceptionFilter(&UnhandledFilter);
}

}
