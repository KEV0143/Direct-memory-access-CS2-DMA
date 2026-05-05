#include "Features/WebRadar/webradar.h"
#include "Features/WebRadar/embedded_assets.h"
#include "Features/WebRadar/web_remote.h"
#include "Features/Radar/map_registry.h"
#include "app/Core/globals.h"
#include "app/Config/project_paths.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

#include <json/json.hpp>

#pragma comment(lib, "Bcrypt.lib")
#pragma comment(lib, "Ws2_32.lib")

namespace
{
    constexpr size_t kMaxHttpRequestSize = 16 * 1024;

    using MapDefinition = radar::MapDefinition;

    struct HttpRequest
    {
        std::string method;
        std::string path;
        std::unordered_map<std::string, std::string> query;
        std::unordered_map<std::string, std::string> headers;
    };

    bool IsFiniteVec(const Vector3& v)
    {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    }

    
    bool IsValidWorldVec(const Vector3& v)
    {
        return IsFiniteVec(v) && (std::abs(v.x) > 1.0f || std::abs(v.y) > 1.0f);
    }

    void AppendJsonString(std::string& out, std::string_view value)
    {
        out.push_back('"');
        for (const unsigned char c : value) {
            switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (c < 0x20) {
                    static constexpr char kHex[] = "0123456789abcdef";
                    out += "\\u00";
                    out.push_back(kHex[(c >> 4) & 0x0F]);
                    out.push_back(kHex[c & 0x0F]);
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
            }
        }
        out.push_back('"');
    }

    template <typename T>
    void AppendJsonInt(std::string& out, T value)
    {
        char buffer[32] = {};
        const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
        if (result.ec == std::errc{})
            out.append(buffer, result.ptr);
    }

    void AppendJsonFloat(std::string& out, float value)
    {
        if (!std::isfinite(value))
            value = 0.0f;

        char buffer[48] = {};
        auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed, 2);
        if (result.ec != std::errc{}) {
            out += "0";
            return;
        }

        size_t len = static_cast<size_t>(result.ptr - buffer);
        while (len > 1 && buffer[len - 1] == '0')
            --len;
        if (len > 1 && buffer[len - 1] == '.')
            --len;
        out.append(buffer, len);
    }

    void AppendJsonVec3(std::string& out, const Vector3& v)
    {
        out.push_back('[');
        AppendJsonFloat(out, v.x);
        out.push_back(',');
        AppendJsonFloat(out, v.y);
        out.push_back(',');
        AppendJsonFloat(out, v.z);
        out.push_back(']');
    }

    void AppendJsonVec2(std::string& out, float x, float y)
    {
        out.push_back('[');
        AppendJsonFloat(out, x);
        out.push_back(',');
        AppendJsonFloat(out, y);
        out.push_back(']');
    }

    const char* TeamDefaultModelName(int team)
    {
        if (team == 3)
            return "ctm_sas";
        if (team == 2)
            return "tm_phoenix";
        return "tm_phoenix";
    }

    const char* WeaponNameFromItemId(uint16_t id)
    {
        switch (id) {
        case 1: return "deagle";
        case 2: return "elite";
        case 3: return "fiveseven";
        case 4: return "glock";
        case 7: return "ak47";
        case 8: return "aug";
        case 9: return "awp";
        case 10: return "famas";
        case 11: return "g3sg1";
        case 13: return "galilar";
        case 14: return "m249";
        case 16: return "m4a1";
        case 17: return "mac10";
        case 19: return "p90";
        case 23: return "mp5sd";
        case 24: return "ump45";
        case 25: return "xm1014";
        case 26: return "bizon";
        case 27: return "mag7";
        case 28: return "negev";
        case 29: return "sawedoff";
        case 30: return "tec9";
        case 31: return "taser";
        case 32: return "p2000";
        case 33: return "mp7";
        case 34: return "mp9";
        case 35: return "nova";
        case 36: return "p250";
        case 38: return "scar20";
        case 39: return "sg556";
        case 40: return "ssg08";
        case 42: return "knife";
        case 43: return "flashbang";
        case 44: return "hegrenade";
        case 45: return "smokegrenade";
        case 46: return "molotov";
        case 47: return "decoy";
        case 48: return "incgrenade";
        case 49: return "c4";
        case 59: return "knife";
        case 60: return "m4a1_silencer";
        case 61: return "usp_silencer";
        case 63: return "cz75a";
        case 64: return "revolver";
        default:
            break;
        }

        if (id >= 500 && id <= 525)
            return "knife";
        return "";
    }

    std::string SafePlayerName(const char* value, size_t maxLen)
    {
        if (!value || maxLen == 0)
            return {};

        size_t n = 0;
        while (n < maxLen && value[n] != '\0')
            ++n;

        std::string out(value, n);
        for (char& c : out) {
            if (static_cast<unsigned char>(c) < 0x20)
                c = ' ';
        }
        return out;
    }

    std::string ToLower(std::string_view value)
    {
        std::string lowered;
        lowered.reserve(value.size());

        for (const unsigned char c : value)
            lowered.push_back(static_cast<char>(std::tolower(c)));

        return lowered;
    }

    uint16_t NormalizePort(int port)
    {
        if (port < 1025 || port > 65535)
            return webradar::cfg::kDefaultListenPort;
        return static_cast<uint16_t>(port);
    }

    int RequestedProtocolVersion(const std::unordered_map<std::string, std::string>& query)
    {
        const auto it = query.find("pv");
        if (it == query.end())
            return webradar::cfg::kDefaultProtocolVersion;
        const std::string& v = it->second;
        if (v.empty())
            return webradar::cfg::kDefaultProtocolVersion;
        int parsed = 0;
        for (char c : v) {
            if (c < '0' || c > '9')
                return webradar::cfg::kDefaultProtocolVersion;
            parsed = parsed * 10 + (c - '0');
            if (parsed > 99)
                return webradar::cfg::kDefaultProtocolVersion;
        }
        if (parsed < webradar::cfg::kMinProtocolVersion ||
            parsed > webradar::cfg::kMaxProtocolVersion)
            return webradar::cfg::kDefaultProtocolVersion;
        return parsed;
    }

    std::string UrlDecode(std::string_view encoded)
    {
        std::string out;
        out.reserve(encoded.size());

        for (size_t i = 0; i < encoded.size(); ++i) {
            const char c = encoded[i];
            if (c == '+') {
                out.push_back(' ');
                continue;
            }

            if (c == '%' && (i + 2) < encoded.size()) {
                auto hexToInt = [](char x) -> int {
                    if (x >= '0' && x <= '9')
                        return x - '0';
                    if (x >= 'a' && x <= 'f')
                        return 10 + (x - 'a');
                    if (x >= 'A' && x <= 'F')
                        return 10 + (x - 'A');
                    return -1;
                };

                const int hiValue = hexToInt(encoded[i + 1]);
                const int loValue = hexToInt(encoded[i + 2]);
                if (hiValue >= 0 && loValue >= 0) {
                    out.push_back(static_cast<char>((hiValue << 4) | loValue));
                    i += 2;
                    continue;
                }
            }

            out.push_back(c);
        }

        return out;
    }

    std::string TrimCopy(std::string_view text)
    {
        size_t start = 0;
        while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0)
            ++start;

        size_t end = text.size();
        while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
            --end;

        return std::string(text.substr(start, end - start));
    }

    std::unordered_map<std::string, std::string> ParseQuery(std::string_view queryText)
    {
        std::unordered_map<std::string, std::string> result;
        size_t start = 0;
        while (start <= queryText.size()) {
            const size_t end = queryText.find('&', start);
            const std::string_view part = queryText.substr(start, end == std::string::npos ? std::string::npos : (end - start));
            if (!part.empty()) {
                const size_t sep = part.find('=');
                const std::string key = UrlDecode(part.substr(0, sep));
                const std::string value = sep == std::string::npos ? std::string() : UrlDecode(part.substr(sep + 1));
                result[key] = value;
            }

            if (end == std::string::npos)
                break;
            start = end + 1;
        }

        return result;
    }

    bool ReadHttpRequest(SOCKET client, std::string* outRequest)
    {
        if (!outRequest)
            return false;

        std::string request;
        request.reserve(2048);
        std::array<char, 2048> buffer = {};

        while (request.find("\r\n\r\n") == std::string::npos && request.size() < kMaxHttpRequestSize) {
            const int received = recv(client, buffer.data(), static_cast<int>(buffer.size()), 0);
            if (received <= 0)
                return false;
            request.append(buffer.data(), static_cast<size_t>(received));
        }

        if (request.find("\r\n\r\n") == std::string::npos)
            return false;

        *outRequest = std::move(request);
        return true;
    }

    HttpRequest ParseHttpRequest(const std::string& rawRequest)
    {
        HttpRequest request;
        const size_t firstLineEnd = rawRequest.find("\r\n");
        const std::string_view requestView(rawRequest);
        const std::string_view firstLine = requestView.substr(0, firstLineEnd);
        const size_t methodSep = firstLine.find(' ');
        if (methodSep == std::string::npos)
            return request;

        const size_t pathSep = firstLine.find(' ', methodSep + 1);
        if (pathSep == std::string::npos)
            return request;

        request.method.assign(firstLine.substr(0, methodSep));
        const std::string_view target = firstLine.substr(methodSep + 1, pathSep - methodSep - 1);
        const size_t querySep = target.find('?');
        request.path.assign(querySep == std::string::npos ? target : target.substr(0, querySep));
        if (request.path.empty())
            request.path = "/";
        if (querySep != std::string::npos)
            request.query = ParseQuery(target.substr(querySep + 1));

        size_t lineStart = (firstLineEnd == std::string::npos) ? std::string::npos : firstLineEnd + 2;
        while (lineStart != std::string::npos && lineStart < rawRequest.size()) {
            const size_t lineEnd = rawRequest.find("\r\n", lineStart);
            if (lineEnd == std::string::npos || lineEnd == lineStart)
                break;

            const std::string_view line = requestView.substr(lineStart, lineEnd - lineStart);
            const size_t colon = line.find(':');
            if (colon != std::string::npos) {
                const std::string key = ToLower(TrimCopy(line.substr(0, colon)));
                const std::string value = TrimCopy(line.substr(colon + 1));
                if (!key.empty())
                    request.headers[key] = value;
            }

            lineStart = lineEnd + 2;
        }

        return request;
    }
}

namespace webradar
{

namespace
{
    constexpr int kWebRadarRealtimeIntervalMs = cfg::kMinRealtimeIntervalMs;

    double Ema(double current, double sample, double alpha)
    {
        if (!std::isfinite(current) || current <= 0.0)
            return sample;
        return current + (sample - current) * alpha;
    }

    std::string HttpStatusText(int statusCode)
    {
        switch (statusCode) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default: return "OK";
        }
    }

    bool SendAll(SOCKET socketHandle, const char* data, size_t size)
    {
        size_t offset = 0;
        while (offset < size) {
            const int sent = send(socketHandle, data + offset, static_cast<int>(size - offset), 0);
            if (sent <= 0)
                return false;
            offset += static_cast<size_t>(sent);
        }

        return true;
    }

    bool SendHttpResponseEx(
        SOCKET socketHandle,
        int statusCode,
        const char* contentType,
        const std::string& body,
        const char* cacheControl,
        bool keepAlive,
        const std::string& accessControlOrigin = std::string("*"))
    {
        const std::string cacheControlValue = cacheControl ? cacheControl : "no-store";
        const std::string noCacheCompatHeaders =
            cacheControlValue.find("no-store") != std::string::npos
                ? "Pragma: no-cache\r\nExpires: 0\r\n"
                : "";
        const std::string corsHeader = accessControlOrigin.empty()
            ? std::string()
            : std::format(
                "Access-Control-Allow-Origin: {}\r\n"
                "Vary: Origin\r\n",
                accessControlOrigin);
        const std::string header = std::format(
            "HTTP/1.1 {} {}\r\n"
            "Connection: {}\r\n"
            "{}"
            "X-Content-Type-Options: nosniff\r\n"
            "Cache-Control: {}\r\n"
            "{}"
            "Content-Type: {}\r\n"
            "Content-Length: {}\r\n\r\n",
            statusCode,
            HttpStatusText(statusCode),
            keepAlive ? "keep-alive" : "close",
            corsHeader,
            cacheControlValue,
            noCacheCompatHeaders,
            contentType ? contentType : "application/octet-stream",
            body.size());

        return SendAll(socketHandle, header.data(), header.size()) &&
            SendAll(socketHandle, body.data(), body.size());
    }

    bool SendHttpResponse(SOCKET socketHandle, int statusCode, const char* contentType, const std::string& body)
    {
        return SendHttpResponseEx(socketHandle, statusCode, contentType, body, "no-store", false);
    }

    bool SendSseHeaders(SOCKET socketHandle, const std::string& accessControlOrigin = std::string("*"))
    {
        const std::string corsHeader = accessControlOrigin.empty()
            ? std::string()
            : std::format(
                "Access-Control-Allow-Origin: {}\r\n"
                "Vary: Origin\r\n",
                accessControlOrigin);
        const std::string header = std::format(
            "HTTP/1.1 200 OK\r\n"
            "Connection: keep-alive\r\n"
            "Cache-Control: no-store\r\n"
            "{}"
            "X-Content-Type-Options: nosniff\r\n"
            "Content-Type: text/event-stream\r\n"
            "X-Accel-Buffering: no\r\n\r\n",
            corsHeader);
        return SendAll(socketHandle, header.data(), header.size());
    }

    bool SendSseEvent(SOCKET socketHandle, const char* eventName, const std::string& body)
    {
        std::string payload;
        payload.reserve(body.size() + 48);
        if (eventName && *eventName != '\0')
            payload += std::string("event: ") + eventName + "\n";
        payload += "data: ";
        payload += body;
        payload += "\n\n";
        return SendAll(socketHandle, payload.data(), payload.size());
    }

    std::string ResolveAllowedOrigin(
        const std::vector<std::string>& allowlist,
        const std::unordered_map<std::string, std::string>& headers)
    {
        if (allowlist.empty())
            return std::string("*");

        const auto it = headers.find("origin");
        if (it == headers.end())
            return std::string();

        const std::string& origin = it->second;
        if (origin.empty())
            return std::string();

        for (const auto& allowed : allowlist) {
            if (allowed.empty())
                continue;
            if (allowed == "*")
                return origin;
            if (_stricmp(allowed.c_str(), origin.c_str()) == 0)
                return origin;
        }
        return std::string();
    }

    bool HeaderContainsValue(
        const std::unordered_map<std::string, std::string>& headers,
        const char* headerName,
        const char* value)
    {
        if (!headerName || !value)
            return false;

        const auto it = headers.find(headerName);
        if (it == headers.end())
            return false;

        const std::string haystack = ToLower(it->second);
        const std::string needle = ToLower(value);
        size_t start = 0;
        while (start < haystack.size()) {
            size_t end = haystack.find(',', start);
            std::string part = TrimCopy(haystack.substr(start, end == std::string::npos ? std::string::npos : (end - start)));
            if (part == needle)
                return true;
            if (end == std::string::npos)
                break;
            start = end + 1;
        }

        return false;
    }

    std::string Base64Encode(const unsigned char* data, size_t size)
    {
        static constexpr char kAlphabet[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";

        if (!data || size == 0)
            return {};

        std::string out;
        out.reserve(((size + 2) / 3) * 4);

        for (size_t i = 0; i < size; i += 3) {
            const uint32_t octetA = data[i];
            const uint32_t octetB = (i + 1 < size) ? data[i + 1] : 0u;
            const uint32_t octetC = (i + 2 < size) ? data[i + 2] : 0u;
            const uint32_t triple = (octetA << 16) | (octetB << 8) | octetC;

            out.push_back(kAlphabet[(triple >> 18) & 0x3F]);
            out.push_back(kAlphabet[(triple >> 12) & 0x3F]);
            out.push_back((i + 1 < size) ? kAlphabet[(triple >> 6) & 0x3F] : '=');
            out.push_back((i + 2 < size) ? kAlphabet[triple & 0x3F] : '=');
        }

        return out;
    }

    std::string BuildWebSocketAcceptKey(const std::string& clientKey)
    {
        if (clientKey.empty())
            return {};

        static constexpr const char* kWebSocketGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        const std::string input = clientKey + kWebSocketGuid;

        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        DWORD objectSize = 0;
        DWORD resultSize = 0;
        NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA1_ALGORITHM, nullptr, 0);
        if (status < 0)
            return {};

        status = BCryptGetProperty(
            algorithm,
            BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectSize),
            sizeof(objectSize),
            &resultSize,
            0);
        if (status < 0 || objectSize == 0) {
            BCryptCloseAlgorithmProvider(algorithm, 0);
            return {};
        }

        std::vector<unsigned char> objectBuffer(objectSize);
        std::array<unsigned char, 20> digest = {};

        status = BCryptCreateHash(
            algorithm,
            &hash,
            objectBuffer.data(),
            static_cast<ULONG>(objectBuffer.size()),
            nullptr,
            0,
            0);
        if (status >= 0) {
            status = BCryptHashData(
                hash,
                reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())),
                static_cast<ULONG>(input.size()),
                0);
        }
        if (status >= 0) {
            status = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0);
        }

        if (hash)
            BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);

        if (status < 0)
            return {};

        return Base64Encode(digest.data(), digest.size());
    }

    bool SendWebSocketHandshakeResponse(
        SOCKET socketHandle,
        const std::string& acceptKey,
        const std::string& accessControlOrigin = std::string("*"))
    {
        if (acceptKey.empty())
            return false;

        const std::string corsHeader = accessControlOrigin.empty()
            ? std::string()
            : std::format(
                "Access-Control-Allow-Origin: {}\r\n",
                accessControlOrigin);
        const std::string response = std::format(
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "{}"
            "Sec-WebSocket-Accept: {}\r\n\r\n",
            corsHeader,
            acceptKey);
        return SendAll(socketHandle, response.data(), response.size());
    }

    bool SendWebSocketFrame(SOCKET socketHandle, unsigned char opcode, const void* payload, size_t payloadSize)
    {
        std::array<unsigned char, 10> header = {};
        size_t headerSize = 0;
        header[headerSize++] = static_cast<unsigned char>(0x80 | (opcode & 0x0F));

        if (payloadSize < 126) {
            header[headerSize++] = static_cast<unsigned char>(payloadSize);
        } else if (payloadSize <= 0xFFFFu) {
            header[headerSize++] = 126;
            header[headerSize++] = static_cast<unsigned char>((payloadSize >> 8) & 0xFF);
            header[headerSize++] = static_cast<unsigned char>(payloadSize & 0xFF);
        } else {
            header[headerSize++] = 127;
            for (int shift = 56; shift >= 0; shift -= 8) {
                header[headerSize++] = static_cast<unsigned char>((payloadSize >> shift) & 0xFF);
            }
        }

        if (!SendAll(socketHandle, reinterpret_cast<const char*>(header.data()), headerSize))
            return false;
        if (payloadSize == 0)
            return true;
        return SendAll(socketHandle, static_cast<const char*>(payload), payloadSize);
    }

    bool SendWebSocketTextFrame(SOCKET socketHandle, const std::string& payload)
    {
        return SendWebSocketFrame(socketHandle, 0x1, payload.data(), payload.size());
    }

    bool SendWebSocketBinaryFrame(SOCKET socketHandle, const void* data, size_t size)
    {
        return SendWebSocketFrame(socketHandle, 0x2, data, size);
    }

    bool SendWebSocketPing(SOCKET socketHandle)
    {
        return SendWebSocketFrame(socketHandle, 0x9, nullptr, 0);
    }

    bool SendWebSocketCloseFrame(SOCKET socketHandle, uint16_t code = 1000)
    {
        unsigned char payload[2];
        payload[0] = static_cast<unsigned char>((code >> 8) & 0xFF);
        payload[1] = static_cast<unsigned char>(code & 0xFF);
        return SendWebSocketFrame(socketHandle, 0x8, payload, sizeof(payload));
    }

    int DrainWebSocketIncoming(SOCKET socketHandle, bool* closeRequested)
    {
        if (closeRequested)
            *closeRequested = false;
        int totalConsumed = 0;
        for (int iter = 0; iter < 4; ++iter) {
            char buf[256];
            const int r = recv(socketHandle, buf, sizeof(buf), 0);
            if (r > 0) {
                totalConsumed += r;
                for (int i = 0; i + 1 < r; ++i) {
                    const unsigned char first = static_cast<unsigned char>(buf[i]);
                    const unsigned char opcode = first & 0x0F;
                    if (opcode == 0x8) {
                        if (closeRequested)
                            *closeRequested = true;
                        break;
                    }
                }
                if (r < static_cast<int>(sizeof(buf)))
                    break;
                continue;
            }
            if (r == 0) {
                if (closeRequested)
                    *closeRequested = true;
                break;
            }
            const int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK || err == WSAETIMEDOUT)
                break;
            if (closeRequested)
                *closeRequested = true;
            break;
        }
        return totalConsumed;
    }

    std::string GuessContentTypeByExtension(const std::string& extension)
    {
        if (extension == ".html" || extension == ".htm")
            return "text/html; charset=utf-8";
        if (extension == ".json")
            return "application/json; charset=utf-8";
        if (extension == ".js")
            return "application/javascript; charset=utf-8";
        if (extension == ".css")
            return "text/css; charset=utf-8";
        if (extension == ".png")
            return "image/png";
        if (extension == ".webp")
            return "image/webp";
        if (extension == ".jpg" || extension == ".jpeg")
            return "image/jpeg";
        if (extension == ".svg")
            return "image/svg+xml";
        if (extension == ".ico")
            return "image/x-icon";
        if (extension == ".woff2")
            return "font/woff2";
        return "application/octet-stream";
    }

    std::string GuessContentType(const std::filesystem::path& path)
    {
        return GuessContentTypeByExtension(ToLower(path.extension().string()));
    }

    std::string GuessContentTypeFromUrl(const std::string& urlPath)
    {
        const auto dot = urlPath.rfind('.');
        if (dot == std::string::npos)
            return "application/octet-stream";
        return GuessContentTypeByExtension(ToLower(urlPath.substr(dot)));
    }

    const char* CacheControlForRequestPath(const std::string& requestPath, const std::filesystem::path& assetPath)
    {
        const std::string extension = ToLower(assetPath.extension().string());
        if (requestPath == "/" ||
            extension == ".html" ||
            extension == ".htm" ||
            extension == ".js" ||
            extension == ".css")
            return "no-store";

        if (requestPath.rfind("/assets/", 0) == 0 ||
            requestPath.rfind("/data/", 0) == 0 ||
            extension == ".png" ||
            extension == ".jpg" ||
            extension == ".jpeg" ||
            extension == ".svg" ||
            extension == ".woff2") {
            return "public, max-age=86400, stale-while-revalidate=43200";
        }

        return "public, max-age=300, stale-while-revalidate=60";
    }

    bool ReadEntireFile(const std::filesystem::path& path, std::string* outContents)
    {
        if (!outContents)
            return false;

        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            return false;

        file.seekg(0, std::ios::end);
        const std::streamoff size = file.tellg();
        if (size < 0)
            return false;
        file.seekg(0, std::ios::beg);

        std::string contents;
        contents.resize(static_cast<size_t>(size));
        if (size > 0)
            file.read(contents.data(), static_cast<std::streamsize>(size));
        if (!file.good() && !file.eof())
            return false;

        *outContents = std::move(contents);
        return true;
    }

    const std::vector<MapDefinition>& GetMapDefinitions()
    {
        return radar::GetMapDefinitions();
    }

    std::string BuildFallbackMapsJson()
    {
        return radar::BuildMapsJson();
    }

    std::filesystem::path ResolveStaticAssetPath(const std::string& requestPath)
    {
        const auto assetRoot = app::paths::ResolveWebRadarAssetDirectory();
        if (assetRoot.empty())
            return {};

        std::string relative = requestPath;
        if (relative.empty() || relative == "/")
            relative = "/index.html";

        if (!relative.empty() && relative.front() == '/')
            relative.erase(relative.begin());

        relative = UrlDecode(relative);
        std::replace(relative.begin(), relative.end(), '\\', '/');
        if (relative.find("..") != std::string::npos || relative.find(':') != std::string::npos)
            return {};

        const auto candidate = (assetRoot / relative).lexically_normal();
        const auto rootNormalized = assetRoot.lexically_normal();

        const std::string rootString = ToLower(rootNormalized.generic_string());
        const std::string candidateString = ToLower(candidate.generic_string());
        if (candidateString.size() < rootString.size())
            return {};
        if (candidateString.compare(0, rootString.size(), rootString) != 0)
            return {};
        if (candidateString.size() > rootString.size()) {
            const char next = candidateString[rootString.size()];
            if (next != '/' && next != '\\')
                return {};
        }

        return candidate;
    }
}

WEBRadar::WEBRadar()
{
    settings_.listenPort = cfg::kDefaultListenPort;
    requestedPort_.store(cfg::kDefaultListenPort, std::memory_order_relaxed);
    stats_.listenPort = cfg::kDefaultListenPort;
    stats_.statusText = "WEBRadar ready";
    latestPayloadJson_ = BuildFallbackLiveJson(settings_, "unknown", UnixNowMs());
    latestPayloadJsonV2_ = BuildFallbackLiveJsonV2(settings_, "unknown", UnixNowMs());
}

WEBRadar::~WEBRadar()
{
    Stop();
}

void WEBRadar::Start()
{
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true))
        return;

    worker_ = std::thread(&WEBRadar::WorkerLoop, this);
    serverThread_ = std::thread(&WEBRadar::ServerLoop, this);
}

void WEBRadar::Stop()
{
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false))
        return;

    cv_.notify_all();

    const uintptr_t handle = listenSocket_.exchange(0);
    if (handle != 0) {
        const SOCKET socketHandle = static_cast<SOCKET>(handle);
        shutdown(socketHandle, SD_BOTH);
        closesocket(socketHandle);
    }

    CloseStreamClients();
    CloseWebSocketClients();

    if (worker_.joinable())
        worker_.join();
    if (serverThread_.joinable())
        serverThread_.join();
}

void WEBRadar::Configure(bool enabled, int intervalMs, uint16_t listenPort, const std::string& mapOverride,
                         bool bindLan, std::vector<std::string> originAllowlist)
{
    bool portChanged = false;
    bool disableLocal = false;
    bool bindChanged = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const bool prevEnabled = settings_.enabled;
        const int prevInterval = settings_.intervalMs;
        const uint16_t prevPort = settings_.listenPort;
        const std::string prevMapOverride = settings_.mapOverride;
        const bool prevBindLan = settings_.bindLan;
        const auto prevAllowlist = settings_.originAllowlist;

        settings_.enabled = enabled;
        settings_.intervalMs = std::clamp(intervalMs, cfg::kMinRealtimeIntervalMs, cfg::kMaxRealtimeIntervalMs);
        settings_.listenPort = NormalizePort(listenPort);
        settings_.mapOverride = NormalizeMapName(mapOverride);
        settings_.bindLan = bindLan;
        settings_.originAllowlist = std::move(originAllowlist);
        stats_.enabled = settings_.enabled;
        stats_.listenPort = settings_.listenPort;
        requestedPort_.store(settings_.listenPort, std::memory_order_relaxed);
        portChanged = (prevPort != settings_.listenPort);
        bindChanged = (prevBindLan != settings_.bindLan);
        disableLocal = prevEnabled && !settings_.enabled;

        const bool changed =
            prevEnabled != settings_.enabled ||
            prevInterval != settings_.intervalMs ||
            portChanged ||
            bindChanged ||
            prevMapOverride != settings_.mapOverride ||
            prevAllowlist != settings_.originAllowlist;

        if (changed) {
            ++settingsVersion_;
            cv_.notify_one();
        }
    }

    if (!portChanged && !bindChanged && !disableLocal)
        return;

    const uintptr_t handle = listenSocket_.exchange(0);
    if (handle != 0) {
        const SOCKET socketHandle = static_cast<SOCKET>(handle);
        shutdown(socketHandle, SD_BOTH);
        closesocket(socketHandle);
    }

    CloseStreamClients();
    CloseWebSocketClients();
}

void WEBRadar::UpdateSnapshot(const esp::WebRadarSnapshot& snapshot)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        latestSnapshot_ = snapshot;
        hasSnapshot_ = true;
        ++snapshotVersion_;
    }
    cv_.notify_one();
}

RuntimeStats WEBRadar::GetStats() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

bool WEBRadar::HasActiveConsumers() const
{
    if (!IsLocalEnabled())
        return false;

    {
        std::lock_guard<std::mutex> streamLock(streamMutex_);
        if (!streamClients_.empty())
            return true;
    }
    {
        std::lock_guard<std::mutex> wsLock(wsMutex_);
        for (const auto& client : wsClients_) {
            if (client && client->active.load(std::memory_order_relaxed))
                return true;
        }
    }
    return false;
}

bool WEBRadar::IsLocalEnabled() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return settings_.enabled;
}

void WEBRadar::CloseStreamClients()
{
    std::vector<StreamClient> clients;
    {
        std::lock_guard<std::mutex> lock(streamMutex_);
        streamGeneration_.fetch_add(1, std::memory_order_relaxed);
        clients.swap(streamClients_);
    }

    for (const StreamClient& client : clients) {
        const uintptr_t handleValue = client.socketHandle;
        if (handleValue == 0)
            continue;
        const SOCKET socketHandle = static_cast<SOCKET>(handleValue);
        shutdown(socketHandle, SD_BOTH);
        closesocket(socketHandle);
    }
}

void WEBRadar::CloseWebSocketClients()
{
    std::vector<std::unique_ptr<WebSocketClient>> clients;
    {
        std::lock_guard<std::mutex> lock(wsMutex_);
        clients.swap(wsClients_);
    }

    for (auto& client : clients) {
        if (!client)
            continue;

        client->active.store(false, std::memory_order_relaxed);
        const uintptr_t handleValue = client->socketHandle.exchange(0, std::memory_order_relaxed);
        if (handleValue != 0) {
            const SOCKET socketHandle = static_cast<SOCKET>(handleValue);
            shutdown(socketHandle, SD_BOTH);
            closesocket(socketHandle);
        }
    }

    for (auto& client : clients) {
        if (client && client->thread.joinable())
            client->thread.join();
    }
}

void WEBRadar::PruneWebSocketClients()
{
    std::vector<std::unique_ptr<WebSocketClient>> finished;
    {
        std::lock_guard<std::mutex> lock(wsMutex_);
        for (size_t i = 0; i < wsClients_.size();) {
            WebSocketClient* client = wsClients_[i].get();
            if (client && !client->active.load(std::memory_order_relaxed)) {
                finished.push_back(std::move(wsClients_[i]));
                wsClients_.erase(wsClients_.begin() + static_cast<std::ptrdiff_t>(i));
                continue;
            }
            ++i;
        }
    }

    for (auto& client : finished) {
        if (client && client->thread.joinable())
            client->thread.join();
    }
}

bool WEBRadar::RegisterWebSocketClient(uintptr_t socketHandleValue, int protocolVersion)
{
    if (socketHandleValue == 0)
        return false;

    PruneWebSocketClients();

    const SOCKET socketHandle = static_cast<SOCKET>(socketHandleValue);
    u_long nonBlocking = 1;
    ioctlsocket(socketHandle, FIONBIO, &nonBlocking);
    const BOOL noDelay = TRUE;
    setsockopt(socketHandle, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));
    const DWORD wsSendTimeoutMs = 1500;
    setsockopt(socketHandle, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&wsSendTimeoutMs), sizeof(wsSendTimeoutMs));

    auto client = std::make_unique<WebSocketClient>();
    client->socketHandle.store(socketHandleValue, std::memory_order_relaxed);
    client->protocolVersion = protocolVersion >= 2 ? 2 : 1;

    uint64_t currentVersion = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (client->protocolVersion < 2)
            legacyPayloadDemandUntilMs_ = UnixNowMs() + 30000;
        currentVersion = latestPayloadVersion_;
    }
    if (client->protocolVersion < 2)
        cv_.notify_one();

    const uint64_t firstSeenVersion =
        client->protocolVersion >= 2 && currentVersion > 0 ? (currentVersion - 1) : currentVersion;
    client->thread = std::thread(&WEBRadar::WebSocketClientLoop, this, client.get(), firstSeenVersion);

    {
        std::lock_guard<std::mutex> lock(wsMutex_);
        wsClients_.push_back(std::move(client));
    }

    return true;
}

void WEBRadar::WebSocketClientLoop(WebSocketClient* client, uint64_t lastSeenVersion)
{
    if (!client)
        return;

    constexpr uint64_t kIdlePingIntervalMs = 25000;
    constexpr uint64_t kIdleDropMs = 75000;
    const uint64_t startMs = UnixNowMs();
    uint64_t lastPingMs = startMs;
    uint64_t lastActivityMs = startMs;

    while (running_.load(std::memory_order_relaxed) && client->active.load(std::memory_order_relaxed)) {
        std::string payloadJson;
        uint64_t payloadVersion = 0;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, std::chrono::milliseconds(250), [this, client, lastSeenVersion] {
                return !running_.load(std::memory_order_relaxed) ||
                    !client->active.load(std::memory_order_relaxed) ||
                    latestPayloadVersion_ != lastSeenVersion;
            });

            if (!running_.load(std::memory_order_relaxed) || !client->active.load(std::memory_order_relaxed))
                break;

            payloadJson = client->protocolVersion >= 2 ? latestPayloadJsonV2_ : latestPayloadJson_;
            payloadVersion = latestPayloadVersion_;
        }

        const uintptr_t handleValue = client->socketHandle.load(std::memory_order_relaxed);
        if (handleValue == 0)
            break;
        const SOCKET socketHandle = static_cast<SOCKET>(handleValue);

        bool peerClose = false;
        const int drained = DrainWebSocketIncoming(socketHandle, &peerClose);
        if (drained > 0)
            lastActivityMs = UnixNowMs();
        if (peerClose) {
            client->active.store(false, std::memory_order_relaxed);
            break;
        }

        const uint64_t nowMs = UnixNowMs();
        if (nowMs - lastActivityMs > kIdleDropMs) {
            client->active.store(false, std::memory_order_relaxed);
            break;
        }

        bool sentSomething = false;
        if (payloadVersion != lastSeenVersion && !payloadJson.empty()) {
            if (!SendWebSocketTextFrame(socketHandle, payloadJson)) {
                client->active.store(false, std::memory_order_relaxed);
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    ++stats_.droppedFramesSlowClient;
                }
                break;
            }
            RecordBytesOut(payloadJson.size());
            lastSeenVersion = payloadVersion;
            sentSomething = true;
        }

        if (!sentSomething && (nowMs - lastPingMs) >= kIdlePingIntervalMs) {
            if (!SendWebSocketPing(socketHandle)) {
                client->active.store(false, std::memory_order_relaxed);
                break;
            }
            lastPingMs = nowMs;
        }
    }

    const uintptr_t handleValue = client->socketHandle.exchange(0, std::memory_order_relaxed);
    if (handleValue != 0) {
        const SOCKET socketHandle = static_cast<SOCKET>(handleValue);
        SendWebSocketCloseFrame(socketHandle, 1000);
        shutdown(socketHandle, SD_BOTH);
        closesocket(socketHandle);
    }
    client->active.store(false, std::memory_order_relaxed);
}

void WEBRadar::BroadcastLivePayload(const std::string& payloadJsonV1, const std::string& payloadJsonV2)
{
    size_t bytesSent = 0;
    if (remote::HasActiveConsumerDemand())
        remote::Publish(payloadJsonV2);

    if (!IsLocalEnabled())
        return;

    std::vector<StreamClient> clients;
    uint64_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(streamMutex_);
        generation = streamGeneration_.load(std::memory_order_relaxed);
        clients.swap(streamClients_);
    }

    std::vector<StreamClient> survivors;
    survivors.reserve(clients.size());

    for (const StreamClient& client : clients) {
        const SOCKET socketHandle = static_cast<SOCKET>(client.socketHandle);
        if (socketHandle == INVALID_SOCKET)
            continue;

        bool connected = true;
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(socketHandle, &readSet);
        timeval tv = { 0, 0 };
        if (select(0, &readSet, nullptr, nullptr, &tv) > 0) {
            char buf;
            const int r = recv(socketHandle, &buf, 1, MSG_PEEK);
            connected = (r > 0);
        }

        if (!connected) {
            shutdown(socketHandle, SD_BOTH);
            closesocket(socketHandle);
            continue;
        }

        const std::string& payloadJson = client.protocolVersion >= 2 ? payloadJsonV2 : payloadJsonV1;
        if (!payloadJson.empty()) {
            if (!SendSseEvent(socketHandle, "snapshot", payloadJson)) {
                shutdown(socketHandle, SD_BOTH);
                closesocket(socketHandle);
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    ++stats_.droppedFramesSlowClient;
                }
                continue;
            }
            bytesSent += payloadJson.size();
        }

        survivors.push_back(client);
    }

    if (!survivors.empty()) {
        bool keepSurvivors = false;
        if (running_.load(std::memory_order_relaxed)) {
            std::lock_guard<std::mutex> lock(streamMutex_);
            keepSurvivors = (streamGeneration_.load(std::memory_order_relaxed) == generation);
            if (keepSurvivors)
                streamClients_.insert(streamClients_.end(), survivors.begin(), survivors.end());
        }

        if (!keepSurvivors) {
            for (const StreamClient& client : survivors) {
                const SOCKET socketHandle = static_cast<SOCKET>(client.socketHandle);
                if (socketHandle == INVALID_SOCKET)
                    continue;
                shutdown(socketHandle, SD_BOTH);
                closesocket(socketHandle);
            }
        }
    }

    if (bytesSent > 0)
        RecordBytesOut(bytesSent);
}

void WEBRadar::RecordBytesOut(size_t bytes)
{
    if (bytes == 0)
        return;

    const uint64_t nowMs = UnixNowMs();
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.totalBytesOut += static_cast<uint64_t>(bytes);

    if (bytesOutWindowUnixMs_ == 0)
        bytesOutWindowUnixMs_ = nowMs;
    bytesOutWindowBytes_ += static_cast<uint64_t>(bytes);

    const uint64_t elapsedMs = nowMs > bytesOutWindowUnixMs_ ? (nowMs - bytesOutWindowUnixMs_) : 0;
    if (elapsedMs >= 250) {
        const double sampleBytesPerSec =
            static_cast<double>(bytesOutWindowBytes_) * 1000.0 / static_cast<double>(elapsedMs);
        stats_.bytesOutPerSec = Ema(stats_.bytesOutPerSec, sampleBytesPerSec, 0.25);
        bytesOutWindowUnixMs_ = nowMs;
        bytesOutWindowBytes_ = 0;
    }
}

void WEBRadar::WorkerLoop()
{
    #include "webradar_parts/webradar_worker_loop_body.inl"
}

void WEBRadar::ServerLoop()
{
    #include "webradar_parts/webradar_server_loop_body.inl"
}

bool WEBRadar::HandleHttpClient(uintptr_t socketHandleValue, bool* keepOpen)
{
    #include "webradar_parts/webradar_handle_http_client_body.inl"
}

std::string WEBRadar::BuildStatusJson() const
{
    #include "webradar_parts/webradar_build_status_json_body.inl"
}

std::string WEBRadar::BuildFallbackLiveJson(const SettingsSnapshot& settings, const std::string& mapName, uint64_t nowMs) const
{
    #include "webradar_parts/webradar_build_fallback_live_json_body.inl"
}

std::string WEBRadar::BuildFallbackLiveJsonV2(const SettingsSnapshot& settings, const std::string& mapName, uint64_t nowMs) const
{
    (void)settings;
    nlohmann::json payload = {
        {"v", 2},
        {"seq", nowMs},
        {"ts", nowMs},
        {"map", mapName.empty() ? "unknown" : mapName},
        {"lt", 0},
        {"p", nlohmann::json::array()},
        {"b", nlohmann::json::array({0, nlohmann::json::array({0.0f, 0.0f, 0.0f}), 0.0f, 40.0f, 0.0f, 10.0f})},
        {"w", nlohmann::json::array()}
    };
    return payload.dump();
}

std::string WEBRadar::Trim(const std::string& text)
{
    size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0)
        ++start;

    size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
        --end;

    return text.substr(start, end - start);
}

uint64_t WEBRadar::UnixNowMs()
{
    const auto now = std::chrono::system_clock::now();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
}

std::string WEBRadar::BuildPlayerSteamId(int slot)
{
    return "player:" + std::to_string(slot + 1);
}

std::string WEBRadar::NormalizeMapName(const std::string& rawName)
{
    const std::string trimmed = Trim(rawName);
    if (trimmed.empty() || ToLower(trimmed) == "auto")
        return {};

    return radar::NormalizeMapName(trimmed);
}

std::string WEBRadar::ResolveMapName(const esp::WebRadarSnapshot& snapshot)
{
    #include "webradar_parts/webradar_resolve_map_name_body.inl"
}

WEBRadar& Instance()
{
    static WEBRadar instance;
    return instance;
}

void Initialize()
{
    remote::Start();
    Instance().Start();
}

void Shutdown()
{
    Instance().Stop();
    remote::Stop();
}

void ApplySettingsFromGlobals()
{
    Instance().Configure(
        g::webRadarEnabled,
        g::webRadarIntervalMs,
        static_cast<uint16_t>(g::webRadarPort),
        g::webRadarMapOverride,
        g::webRadarBindLan,
        g::webRadarOriginAllowlist);
    remote::Configure(remote::CaptureSettingsFromGlobals());
}

void CaptureFromEsp()
{
    static uint64_t s_lastCapturedPublishCount = 0;
    const uint64_t publishCount = esp::GetPublishCount();
    if (publishCount == 0 || publishCount == s_lastCapturedPublishCount)
        return;

    esp::WebRadarSnapshot snapshot = {};
    if (!esp::GetWebRadarSnapshot(&snapshot))
        return;

    s_lastCapturedPublishCount = publishCount;
    Instance().UpdateSnapshot(snapshot);
}

RuntimeStats GetRuntimeStats()
{
    return Instance().GetStats();
}

bool HasActiveConsumers()
{
    return Instance().HasActiveConsumers() || remote::HasActiveConsumerDemand();
}

} 
