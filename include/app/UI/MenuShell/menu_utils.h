#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace key_names {
std::string ToDisplayName(int virtualKey);
}

namespace ui::menu_utils {
bool CopyToBuffer(char* dst, size_t dstSize, const std::string& value);
std::string BuildLocalRadarLink(uint16_t port = 0);
std::vector<std::string> BuildLanRadarLinks(uint16_t port = 0);
std::string ToDisplayAddress(const std::string& link);
bool OpenExternal(const std::string& link);
std::string FormatUnixMs(uint64_t ms);
bool RenderQrCode(const std::string& text, float size);
}
