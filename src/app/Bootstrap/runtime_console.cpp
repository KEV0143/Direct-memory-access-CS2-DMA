#include "app/Core/build_info.h"
#include "app/Bootstrap/runtime_console.h"

#include <Windows.h>
#include <DMALibrary/Memory/Memory.h>

#include <chrono>
#include <iostream>
#include <thread>

namespace
{
    const char* kColorReset = "\x1b[0m";
    const char* kColorYellow = "\x1b[38;5;11m";
    const char* kColorGreen = "\x1b[38;5;2m";
    const char* kColorRed = "\x1b[38;5;1m";
}

const char* bootstrap::RuntimeConsole::C(const char* colorCode) const
{
    return useAnsi_ ? colorCode : "";
}

void bootstrap::RuntimeConsole::Initialize(bool verboseLogs)
{
    DmaSetLogLevel(verboseLogs ? DmaLogLevel::Info : DmaLogLevel::Warning);
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    EnableAnsiColors();
    SetConsoleTitleA(BuildRuntimeTitle().c_str());
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        CONSOLE_CURSOR_INFO ci = { 1, FALSE };
        SetConsoleCursorInfo(hOut, &ci);
    }
    EnableBestDpiAwareness();
}

void bootstrap::RuntimeConsole::PrintStartupBanner() const
{
    std::cout << "   ____  __            ________    \n";
    std::cout << "  |    |/ _|_______  _\\_____  \\   \n";
    std::cout << "  |      <_/ __ \\  \\/ //  / \\  \\  \n";
    std::cout << "  |    |  \\  ___/\\   //   \\_/   \\ \n";
    std::cout << "  |____|__ \\___  >\\_/ \\_____\\ \\_/ \n";
    std::cout << "          \\/   \\/            \\__/ \n";
    std::cout << '\n';
}

void bootstrap::RuntimeConsole::AnimateForAtLeast(const std::string& text, int ms) const
{
    const uint64_t start = GetTickCount64();
    int phase = 0;
    while (GetTickCount64() - start < static_cast<uint64_t>(ms)) {
        PrintInfoPending(text, phase++);
        std::this_thread::sleep_for(std::chrono::milliseconds(90));
    }
}

void bootstrap::RuntimeConsole::PrintPending(const std::string& label, const std::string& text, int phase) const
{
    std::cout << "\r" << C(kColorYellow) << "  | " << label << " | " << C(kColorReset)
        << text << ' ' << DotPhase(phase) << "                    " << std::flush;
}

void bootstrap::RuntimeConsole::PrintOk(const std::string& label, const std::string& text) const
{
    std::cout << "\r" << C(kColorYellow) << "  | " << label << " | " << C(kColorReset)
        << text << " [" << C(kColorGreen) << "+" << C(kColorReset) << "]                    \n";
}

void bootstrap::RuntimeConsole::PrintOk(const std::string& label, const std::string& text, int plusCount) const
{
    if (plusCount < 1)
        plusCount = 1;

    std::cout << "\r" << C(kColorYellow) << "  | " << label << " | " << C(kColorReset) << text << " [";
    for (int i = 0; i < plusCount; ++i) {
        if (i > 0)
            std::cout << ' ';
        std::cout << C(kColorGreen) << "+" << C(kColorReset);
    }
    std::cout << "]                    \n";
}

void bootstrap::RuntimeConsole::PrintFail(const std::string& label, const std::string& text) const
{
    std::cout << "\r" << C(kColorYellow) << "  | " << label << " | " << C(kColorReset)
        << text << " [" << C(kColorRed) << "-" << C(kColorReset) << "]                    \n";
}

void bootstrap::RuntimeConsole::PrintLine(const std::string& label, const std::string& text) const
{
    std::cout << C(kColorYellow) << "  | " << label << " | " << C(kColorReset)
        << text << '\n';
}

void bootstrap::RuntimeConsole::PrintMarkedLine(const std::string& label, const std::string& text) const
{
    std::cout << C(kColorYellow) << "  | " << label << " | " << C(kColorReset)
        << text << " [" << C(kColorGreen) << "*" << C(kColorReset) << "]\n";
}

void bootstrap::RuntimeConsole::PrintInfoPending(const std::string& text, int phase) const
{
    PrintPending("Info", text, phase);
}

void bootstrap::RuntimeConsole::PrintInfoOk(const std::string& text) const
{
    PrintOk("Info", text);
}

void bootstrap::RuntimeConsole::PrintInfoOk(const std::string& text, int plusCount) const
{
    PrintOk("Info", text, plusCount);
}

void bootstrap::RuntimeConsole::PrintInfoFail(const std::string& text) const
{
    PrintFail("Info", text);
}

void bootstrap::RuntimeConsole::PrintInfoLine(const std::string& text) const
{
    PrintLine("Info", text);
}

void bootstrap::RuntimeConsole::PrintInfoMarkedLine(const std::string& text) const
{
    PrintMarkedLine("Info", text);
}

void bootstrap::RuntimeConsole::PrintErrorLine(const std::string& text) const
{
    std::cout << "\r" << C(kColorRed) << "  | Error | " << C(kColorReset)
        << text << "                    \n";
}

void bootstrap::RuntimeConsole::EnableBestDpiAwareness()
{
    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (!user32)
        return;

    using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(HANDLE);
    auto setDpiContext = reinterpret_cast<SetProcessDpiAwarenessContextFn>(
        GetProcAddress(user32, "SetProcessDpiAwarenessContext"));

    HANDLE perMonitorAwareV2 = reinterpret_cast<HANDLE>(-4);
    if (setDpiContext && setDpiContext(perMonitorAwareV2))
        return;

    SetProcessDPIAware();
}

std::string bootstrap::RuntimeConsole::BuildRuntimeTitle()
{
    return app::build_info::RuntimeTitle();
}

std::string bootstrap::RuntimeConsole::DotPhase(int phase)
{
    switch (phase % 5) {
    case 0: return ".    ";
    case 1: return ". .  ";
    case 2: return ". . .";
    case 3: return ". .  ";
    default: return ".    ";
    }
}

void bootstrap::RuntimeConsole::EnableAnsiColors()
{
    HANDLE outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (outputHandle == INVALID_HANDLE_VALUE)
        return;

    DWORD mode = 0;
    if (!GetConsoleMode(outputHandle, &mode))
        return;

    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (SetConsoleMode(outputHandle, mode))
        useAnsi_ = true;
}
