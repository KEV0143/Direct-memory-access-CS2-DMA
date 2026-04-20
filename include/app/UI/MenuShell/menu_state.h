#pragma once

#include <string>

namespace ui {
struct MenuState {
    char profileName[64] = "default";
    char webMapOverride[96] = {};

    bool profileInit = false;
    bool waitingForMenuKey = false;

    std::string statusText;
    float statusShownAt = 0.0f;
    float statusUntil = 0.0f;
};
}
