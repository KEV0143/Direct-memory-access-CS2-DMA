#pragma once

#include <string>

namespace ui {
struct MenuState {
    char profileName[64] = "default";
    char webMapOverride[96] = {};
    char webRemoteHost[128] = {};
    char webRemoteLogin[64] = {};
    char webRemotePassword[128] = {};
    char webRemotePath[256] = {};

    bool profileInit = false;
    bool webRemoteInit = false;
    bool waitingForMenuKey = false;

    unsigned char menuKeyCaptureSnapshot[256] = {};

    std::string statusText;
    float statusShownAt = 0.0f;
    float statusUntil = 0.0f;
};
}
