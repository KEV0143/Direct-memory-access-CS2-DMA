#pragma once

#include <string>

namespace bootstrap {
struct VersionUpdateInfo
{
    std::string currentVersion;
    std::string latestVersion;
    std::string releaseName;
    std::string releaseUrl;
    bool updateAvailable = false;
};

bool CheckForVersionUpdate(VersionUpdateInfo* info, std::string* error = nullptr);
bool OpenVersionUpdateReleasePage(const VersionUpdateInfo& info, std::string* error = nullptr);
}
