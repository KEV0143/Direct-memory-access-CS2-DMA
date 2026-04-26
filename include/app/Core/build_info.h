#pragma once

#include <Windows.h>

#include <cstdint>
#include <ctime>
#include <string>
#include <string_view>

namespace app::build_info {
    inline constexpr std::string_view kVersionTag = "v1.0.2";
inline constexpr std::string_view kRepositoryUrl = "https://github.com/KEV0143/Direct-memory-access-CS2-DMA";
inline constexpr std::string_view kRepositoryReleasesUrl = "https://github.com/KEV0143/Direct-memory-access-CS2-DMA/releases";
inline constexpr std::string_view kRemoteBuildInfoUrl = "https://raw.githubusercontent.com/KEV0143/Direct-memory-access-CS2-DMA/main/include/app/Core/build_info.h";
inline constexpr std::string_view kRemoteBuildInfoFallbackUrl = "https://raw.githubusercontent.com/KEV0143/Direct-memory-access-CS2-DMA/master/include/app/Core/build_info.h";

inline const std::string& VersionTag()
{
    static const std::string version(kVersionTag);
    return version;
}

inline const std::string& RepositoryUrl()
{
    static const std::string url(kRepositoryUrl);
    return url;
}

inline const std::string& RepositoryReleasesUrl()
{
    static const std::string url(kRepositoryReleasesUrl);
    return url;
}

inline const std::string& HttpUserAgent()
{
    static const std::string userAgent = "KevqDMA/" + VersionTag();
    return userAgent;
}

inline const std::string& RemoteBuildInfoUrl()
{
    static const std::string url(kRemoteBuildInfoUrl);
    return url;
}

inline const std::string& RemoteBuildInfoFallbackUrl()
{
    static const std::string url(kRemoteBuildInfoFallbackUrl);
    return url;
}

inline std::string ReadLinkedBuildStamp()
{
    const HMODULE module = GetModuleHandleA(nullptr);
    if (!module)
        return {};

    const auto* base = reinterpret_cast<const std::uint8_t*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE)
        return {};

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (!nt || nt->Signature != IMAGE_NT_SIGNATURE)
        return {};

    const std::time_t rawStamp = static_cast<std::time_t>(nt->FileHeader.TimeDateStamp);
    std::tm localTm = {};
    if (localtime_s(&localTm, &rawStamp) != 0)
        return {};

    char buf[64] = {};
    if (std::strftime(buf, sizeof(buf), "%b %d %H:%M:%S", &localTm) == 0)
        return {};

    return std::string(buf);
}

inline const std::string& DisplayStamp()
{
    static const std::string stamp = []() {
        const std::string linked = ReadLinkedBuildStamp();
        if (!linked.empty())
            return linked;
        return std::string(__DATE__) + " " + __TIME__;
    }();
    return stamp;
}

inline const std::string& RuntimeTitle()
{
    static const std::string title = "KevqDMA " + VersionTag() + " Build: " + DisplayStamp();
    return title;
}
}
