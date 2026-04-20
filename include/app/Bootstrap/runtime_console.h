#pragma once

#include <string>

namespace bootstrap {
class RuntimeConsole {
public:
    void Initialize(bool verboseLogs);
    void PrintStartupBanner() const;

    void AnimateForAtLeast(const std::string& text, int ms) const;
    void PrintPending(const std::string& label, const std::string& text, int phase) const;
    void PrintOk(const std::string& label, const std::string& text) const;
    void PrintOk(const std::string& label, const std::string& text, int plusCount) const;
    void PrintFail(const std::string& label, const std::string& text) const;
    void PrintLine(const std::string& label, const std::string& text) const;
    void PrintMarkedLine(const std::string& label, const std::string& text) const;
    void PrintInfoPending(const std::string& text, int phase) const;
    void PrintInfoOk(const std::string& text) const;
    void PrintInfoOk(const std::string& text, int plusCount) const;
    void PrintInfoFail(const std::string& text) const;
    void PrintInfoLine(const std::string& text) const;
    void PrintInfoMarkedLine(const std::string& text) const;
    void PrintErrorLine(const std::string& text) const;

private:
    const char* C(const char* colorCode) const;
    void EnableAnsiColors();
    static void EnableBestDpiAwareness();
    static std::string BuildRuntimeTitle();
    static std::string DotPhase(int phase);

    bool useAnsi_ = false;
};
}
