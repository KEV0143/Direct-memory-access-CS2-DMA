#pragma once
#include <string>
#include <vector>

namespace config {
    void Save();
    void Load();

    bool SaveNamed(const std::string& profileName);
    bool LoadNamed(const std::string& profileName);
    std::vector<std::string> ListProfiles();
    const std::string& GetActiveProfile();
}
