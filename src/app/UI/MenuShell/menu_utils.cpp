#include "app/UI/MenuShell/menu_utils.h"

#include "app/Core/globals.h"
#include "qrcodegen/qrcodegen.hpp"

#include <winsock2.h>
#include <Windows.h>
#include <iphlpapi.h>
#include <shellapi.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <ctime>
#include <format>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include <imgui.h>

#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Ws2_32.lib")

bool ui::menu_utils::CopyToBuffer(char* dst, size_t dstSize, const std::string& value)
{
    if (!dst || dstSize == 0)
        return false;

    strncpy_s(dst, dstSize, value.c_str(), _TRUNCATE);
    return true;
}

namespace
{
    const char* GetKnownKeyName(int vk)
    {
        switch (vk)
        {
        case VK_LBUTTON: return "Mouse 1";
        case VK_RBUTTON: return "Mouse 2";
        case VK_MBUTTON: return "Mouse 3";
        case VK_XBUTTON1: return "Mouse 4";
        case VK_XBUTTON2: return "Mouse 5";
        case VK_BACK: return "Backspace";
        case VK_TAB: return "Tab";
        case VK_RETURN: return "Enter";
        case VK_SHIFT: return "Shift";
        case VK_CONTROL: return "Ctrl";
        case VK_MENU: return "Alt";
        case VK_PAUSE: return "Pause";
        case VK_CAPITAL: return "CapsLock";
        case VK_ESCAPE: return "Esc";
        case VK_SPACE: return "Space";
        case VK_PRIOR: return "PageUp";
        case VK_NEXT: return "PageDown";
        case VK_END: return "End";
        case VK_HOME: return "Home";
        case VK_LEFT: return "Left Arrow";
        case VK_UP: return "Up Arrow";
        case VK_RIGHT: return "Right Arrow";
        case VK_DOWN: return "Down Arrow";
        case VK_SNAPSHOT: return "PrintScreen";
        case VK_INSERT: return "Insert";
        case VK_DELETE: return "Delete";
        case VK_LWIN: return "Left Win";
        case VK_RWIN: return "Right Win";
        case VK_APPS: return "Menu";
        case VK_SLEEP: return "Sleep";
        case VK_NUMLOCK: return "NumLock";
        case VK_SCROLL: return "ScrollLock";
        case VK_LSHIFT: return "Left Shift";
        case VK_RSHIFT: return "Right Shift";
        case VK_LCONTROL: return "Left Ctrl";
        case VK_RCONTROL: return "Right Ctrl";
        case VK_LMENU: return "Left Alt";
        case VK_RMENU: return "Right Alt";
        case VK_MULTIPLY: return "Num *";
        case VK_ADD: return "Num +";
        case VK_SUBTRACT: return "Num -";
        case VK_DECIMAL: return "Num .";
        case VK_DIVIDE: return "Num /";
        case VK_OEM_1: return "; :";
        case VK_OEM_PLUS: return "= +";
        case VK_OEM_COMMA: return ", <";
        case VK_OEM_MINUS: return "- _";
        case VK_OEM_PERIOD: return ". >";
        case VK_OEM_2: return "/ ?";
        case VK_OEM_3: return "` ~";
        case VK_OEM_4: return "[ {";
        case VK_OEM_5: return "\\ |";
        case VK_OEM_6: return "] }";
        case VK_OEM_7: return "' \"";
        case VK_OEM_102: return "OEM 102";
        case VK_BROWSER_BACK: return "Browser Back";
        case VK_BROWSER_FORWARD: return "Browser Forward";
        case VK_BROWSER_REFRESH: return "Browser Refresh";
        case VK_BROWSER_STOP: return "Browser Stop";
        case VK_BROWSER_SEARCH: return "Browser Search";
        case VK_BROWSER_FAVORITES: return "Browser Favorites";
        case VK_BROWSER_HOME: return "Browser Home";
        case VK_VOLUME_MUTE: return "Volume Mute";
        case VK_VOLUME_DOWN: return "Volume Down";
        case VK_VOLUME_UP: return "Volume Up";
        case VK_MEDIA_NEXT_TRACK: return "Media Next";
        case VK_MEDIA_PREV_TRACK: return "Media Prev";
        case VK_MEDIA_STOP: return "Media Stop";
        case VK_MEDIA_PLAY_PAUSE: return "Media Play/Pause";
        case VK_LAUNCH_MAIL: return "Launch Mail";
        case VK_LAUNCH_MEDIA_SELECT: return "Launch Media";
        case VK_LAUNCH_APP1: return "Launch App1";
        case VK_LAUNCH_APP2: return "Launch App2";
        default:
            return nullptr;
        }
    }

    bool IsExtendedKey(int vk)
    {
        switch (vk)
        {
        case VK_INSERT:
        case VK_DELETE:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_DIVIDE:
        case VK_RMENU:
        case VK_RCONTROL:
            return true;
        default:
            return false;
        }
    }

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

    bool IsAsciiPrintable(const std::string& text)
    {
        if (text.empty())
            return false;

        for (unsigned char c : text) {
            if (c < 0x20 || c > 0x7E)
                return false;
        }

        return true;
    }

    int ClampRadarPort(int port)
    {
        if (port <= 0)
            port = g::webRadarPort;
        if (port < 1025)
            port = 1025;
        if (port > 65535)
            port = 65535;
        return port;
    }

    std::string BuildRadarHttpUrl(const std::string& host, int port)
    {
        if (host.empty())
            return {};

        return "http://" + host + ":" + std::to_string(ClampRadarPort(port));
    }

    std::vector<std::string> CollectLanIpv4Addresses()
    {
        WSADATA wsaData = {};
        const bool wsaReady = (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);

        ULONG bufferLength = 15 * 1024;
        std::vector<unsigned char> buffer(bufferLength);

        const ULONG flags =
            GAA_FLAG_SKIP_ANYCAST |
            GAA_FLAG_SKIP_MULTICAST |
            GAA_FLAG_SKIP_DNS_SERVER;

        IP_ADAPTER_ADDRESSES* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        ULONG result = GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &bufferLength);
        if (result == ERROR_BUFFER_OVERFLOW) {
            buffer.resize(bufferLength);
            adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
            result = GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &bufferLength);
        }
        if (result != NO_ERROR) {
            if (wsaReady)
                WSACleanup();
            return {};
        }

        std::unordered_set<std::string> seen;
        std::vector<std::string> addresses;
        char textBuffer[INET_ADDRSTRLEN] = {};

        for (auto* adapter = adapters; adapter != nullptr; adapter = adapter->Next) {
            if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
                continue;

            for (auto* address = adapter->FirstUnicastAddress; address != nullptr; address = address->Next) {
                if (!address->Address.lpSockaddr || address->Address.lpSockaddr->sa_family != AF_INET)
                    continue;

                auto* ipv4 = reinterpret_cast<sockaddr_in*>(address->Address.lpSockaddr);
                if (!inet_ntop(AF_INET, &ipv4->sin_addr, textBuffer, sizeof(textBuffer)))
                    continue;

                const std::string ip = textBuffer;
                if (ip.rfind("127.", 0) == 0 || ip.rfind("169.254.", 0) == 0)
                    continue;
                if (!seen.insert(ip).second)
                    continue;

                addresses.push_back(ip);
            }
        }

        std::sort(addresses.begin(), addresses.end());
        if (wsaReady)
            WSACleanup();
        return addresses;
    }
}

std::string ui::menu_utils::BuildLocalRadarLink(uint16_t port)
{
    return BuildRadarHttpUrl("127.0.0.1", port);
}

std::vector<std::string> ui::menu_utils::BuildLanRadarLinks(uint16_t port)
{
    std::vector<std::string> links;
    for (const std::string& ip : CollectLanIpv4Addresses())
        links.push_back(BuildRadarHttpUrl(ip, port));
    return links;
}

std::string ui::menu_utils::ToDisplayAddress(const std::string& link)
{
    if (link.rfind("http://", 0) == 0)
        return link.substr(7);
    if (link.rfind("https://", 0) == 0)
        return link.substr(8);
    return link;
}

bool ui::menu_utils::OpenExternal(const std::string& link)
{
    if (link.empty())
        return false;

    const HINSTANCE h = ShellExecuteA(nullptr, "open", link.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<intptr_t>(h) > 32)
        return true;

    std::string args = "url.dll,FileProtocolHandler \"" + link + "\"";
    const HINSTANCE h2 = ShellExecuteA(nullptr, "open", "rundll32.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<intptr_t>(h2) > 32;
}

std::string ui::menu_utils::FormatUnixMs(uint64_t ms)
{
    if (ms == 0)
        return "-";

    const std::time_t t = static_cast<std::time_t>(ms / 1000ULL);
    std::tm tmLocal = {};
    localtime_s(&tmLocal, &t);

    char buf[32] = {};
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tmLocal);
    return buf;
}

bool ui::menu_utils::RenderQrCode(const std::string& text, float size)
{
    if (text.empty() || size <= 0.0f) {
        ImGui::Dummy(ImVec2(size, size));
        return false;
    }

    using qrcodegen::QrCode;

    static std::string cachedText;
    static std::unique_ptr<QrCode> cachedCode;

    if (cachedText != text || !cachedCode) {
        try {
            cachedCode = std::make_unique<QrCode>(QrCode::encodeText(text.c_str(), QrCode::Ecc::MEDIUM));
        } catch (...) {
            cachedCode.reset();
        }
        cachedText = text;
    }

    if (!cachedCode) {
        ImGui::Dummy(ImVec2(size, size));
        return false;
    }

    const int moduleCount = cachedCode->getSize();
    if (moduleCount <= 0) {
        ImGui::Dummy(ImVec2(size, size));
        return false;
    }

    
    constexpr int kQuietZone = 4;
    const float moduleSize = size / static_cast<float>(moduleCount + kQuietZone * 2);
    const float totalSize  = size;
    const float qzOffset   = kQuietZone * moduleSize;

    const ImGuiStyle& style = ImGui::GetStyle();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    
    drawList->AddRectFilled(
        start,
        ImVec2(start.x + totalSize, start.y + totalSize),
        IM_COL32(255, 255, 255, 255),
        style.FrameRounding + 1.0f);

    for (int y = 0; y < moduleCount; ++y) {
        for (int x = 0; x < moduleCount; ++x) {
            if (!cachedCode->getModule(x, y))
                continue;

            const ImVec2 cellMin(start.x + qzOffset + x * moduleSize, start.y + qzOffset + y * moduleSize);
            const ImVec2 cellMax(start.x + qzOffset + (x + 1) * moduleSize, start.y + qzOffset + (y + 1) * moduleSize);
            drawList->AddRectFilled(cellMin, cellMax, IM_COL32(0, 0, 0, 255));
        }
    }

    ImGui::Dummy(ImVec2(totalSize, totalSize));
    return true;
}

std::string key_names::ToDisplayName(int virtualKey)
{
    if (virtualKey >= '0' && virtualKey <= '9')
        return std::string(1, static_cast<char>(virtualKey));

    if (virtualKey >= 'A' && virtualKey <= 'Z')
        return std::string(1, static_cast<char>(virtualKey));

    if (virtualKey >= VK_NUMPAD0 && virtualKey <= VK_NUMPAD9)
        return "Num " + std::to_string(virtualKey - VK_NUMPAD0);

    if (virtualKey >= VK_F1 && virtualKey <= VK_F24)
        return "F" + std::to_string(virtualKey - VK_F1 + 1);

    if (const char* known = GetKnownKeyName(virtualKey))
        return std::string(known);

    UINT scanCode = MapVirtualKeyW(static_cast<UINT>(virtualKey), MAPVK_VK_TO_VSC);
    LONG lParam = static_cast<LONG>(scanCode << 16);
    if (IsExtendedKey(virtualKey))
        lParam |= (1 << 24);

    wchar_t keyName[64] = {};
    if (GetKeyNameTextW(lParam, keyName, static_cast<int>(std::size(keyName))) > 0) {
        const std::string utf8Name = WideToUtf8(keyName);
        if (IsAsciiPrintable(utf8Name))
            return utf8Name;
    }

    return std::format("0x{:02X}", virtualKey & 0xFF);
}
