#include "app/Bootstrap/version_update.h"

#include "app/Core/build_info.h"

#include <Windows.h>
#include <Shellapi.h>
#include <winhttp.h>

#include <json/json.hpp>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace
{
    using json = nlohmann::json;

    struct HttpTextResponse
    {
        DWORD statusCode = 0;
        std::string body;
    };

    std::string Trim(std::string value)
    {
        const auto isSpace = [](unsigned char ch) {
            return std::isspace(ch) != 0;
        };

        while (!value.empty() && isSpace(static_cast<unsigned char>(value.front())))
            value.erase(value.begin());
        while (!value.empty() && isSpace(static_cast<unsigned char>(value.back())))
            value.pop_back();
        return value;
    }

    std::wstring ToWide(std::string_view text)
    {
        if (text.empty())
            return {};
        const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (length <= 0)
            return {};
        std::wstring result(static_cast<size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
        return result;
    }

    std::string ToLower(std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return value;
    }

    std::vector<std::wstring> GitHubHeaders()
    {
        return {
            L"Accept: application/vnd.github+json",
            L"X-GitHub-Api-Version: 2022-11-28",
            ToWide("User-Agent: KevqDMA/" + app::build_info::VersionTag())
        };
    }

    bool IsSuccessStatus(DWORD statusCode)
    {
        return statusCode >= 200 && statusCode < 300;
    }

    std::string ExtractGitHubApiMessage(const HttpTextResponse& response)
    {
        if (response.body.empty())
            return {};

        json root = json::parse(response.body, nullptr, false);
        if (root.is_discarded() || !root.is_object())
            return {};

        return Trim(root.value("message", std::string{}));
    }

    bool IsGitHubRateLimitResponse(const HttpTextResponse& response)
    {
        if (response.statusCode == 429)
            return true;
        if (response.statusCode != 403)
            return false;
        return ToLower(ExtractGitHubApiMessage(response)).find("rate limit") != std::string::npos;
    }

    std::string FormatGitHubHttpError(std::string_view context, const HttpTextResponse& response)
    {
        std::string message = ExtractGitHubApiMessage(response);
        if (message.empty())
            message = "HTTP " + std::to_string(response.statusCode);
        return std::string(context) + ": " + message;
    }

    void SetNoUpdateDefaults(bootstrap::VersionUpdateInfo& result)
    {
        result.updateAvailable = false;
        if (result.releaseUrl.empty())
            result.releaseUrl = app::build_info::RepositoryUrl() + "/releases";
    }

    bool HttpGetText(const std::string& url,
                     const std::vector<std::wstring>& headers,
                     HttpTextResponse& out,
                     std::string* error)
    {
        out = {};

        const bool useHttps = url.rfind("https://", 0) == 0;
        size_t hostStart = url.find("://");
        if (hostStart == std::string::npos) {
            if (error)
                *error = "Invalid URL: " + url;
            return false;
        }
        hostStart += 3;

        const size_t pathStart = url.find('/', hostStart);
        const std::string host =
            (pathStart != std::string::npos) ? url.substr(hostStart, pathStart - hostStart) : url.substr(hostStart);
        const std::string path =
            (pathStart != std::string::npos) ? url.substr(pathStart) : "/";
        if (host.empty()) {
            if (error)
                *error = "Invalid URL host: " + url;
            return false;
        }

        const std::wstring wideHost = ToWide(host);
        const std::wstring widePath = ToWide(path);
        if (wideHost.empty() || widePath.empty()) {
            if (error)
                *error = "UTF-8 conversion failed for URL: " + url;
            return false;
        }

        HINTERNET session = WinHttpOpen(
            ToWide("KevqDMA/" + app::build_info::VersionTag()).c_str(),
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);
        if (!session) {
            if (error)
                *error = "WinHttpOpen failed";
            return false;
        }

        const INTERNET_PORT port = useHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
        HINTERNET connection = WinHttpConnect(session, wideHost.c_str(), port, 0);
        if (!connection) {
            WinHttpCloseHandle(session);
            if (error)
                *error = "WinHttpConnect failed for " + host;
            return false;
        }

        const DWORD flags = useHttps ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET request = WinHttpOpenRequest(
            connection,
            L"GET",
            widePath.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            flags);
        if (!request) {
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            if (error)
                *error = "WinHttpOpenRequest failed";
            return false;
        }

        for (const std::wstring& header : headers) {
            if (!header.empty())
                WinHttpAddRequestHeaders(request, header.c_str(), static_cast<DWORD>(header.size()), WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
        }

        const bool requestOk =
            WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(request, nullptr);
        if (!requestOk) {
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            if (error)
                *error = "HTTP GET failed for " + url;
            return false;
        }

        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        if (!WinHttpQueryHeaders(
                request,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &statusCode,
                &statusCodeSize,
                WINHTTP_NO_HEADER_INDEX)) {
            statusCode = 0;
        }
        out.statusCode = statusCode;

        char buffer[4096];
        DWORD bytesRead = 0;
        while (WinHttpReadData(request, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0)
            out.body.append(buffer, buffer + bytesRead);

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);

        return true;
    }

    std::string NormalizeVersion(std::string value)
    {
        value = Trim(std::move(value));
        if (!value.empty() && (value.front() == 'v' || value.front() == 'V'))
            value.erase(value.begin());
        return value;
    }

    bool TryParseVersion(std::string_view version, std::vector<int>& parts)
    {
        parts.clear();
        const std::string normalized = NormalizeVersion(std::string(version));
        if (normalized.empty())
            return false;

        size_t i = 0;
        while (i < normalized.size()) {
            if (!std::isdigit(static_cast<unsigned char>(normalized[i])))
                break;

            size_t j = i;
            while (j < normalized.size() && std::isdigit(static_cast<unsigned char>(normalized[j])))
                ++j;

            int value = 0;
            const auto [ptr, ec] = std::from_chars(normalized.data() + i, normalized.data() + j, value);
            if (ec != std::errc() || ptr != normalized.data() + j)
                return false;
            parts.push_back(value);

            if (j >= normalized.size())
                return !parts.empty();
            if (normalized[j] != '.')
                break;
            i = j + 1;
        }

        return !parts.empty();
    }

    bool IsNewerVersion(std::string_view latest, std::string_view current)
    {
        const std::string latestNormalized = NormalizeVersion(std::string(latest));
        const std::string currentNormalized = NormalizeVersion(std::string(current));
        if (latestNormalized.empty())
            return false;
        if (currentNormalized.empty())
            return true;

        std::vector<int> latestParts;
        std::vector<int> currentParts;
        const bool latestParsed = TryParseVersion(latestNormalized, latestParts);
        const bool currentParsed = TryParseVersion(currentNormalized, currentParts);

        if (latestParsed && currentParsed) {
            const size_t count = std::max(latestParts.size(), currentParts.size());
            for (size_t i = 0; i < count; ++i) {
                const int latestPart = i < latestParts.size() ? latestParts[i] : 0;
                const int currentPart = i < currentParts.size() ? currentParts[i] : 0;
                if (latestPart != currentPart)
                    return latestPart > currentPart;
            }
            return false;
        }

        return _stricmp(latestNormalized.c_str(), currentNormalized.c_str()) != 0;
    }

    bool TryLoadLatestTag(bootstrap::VersionUpdateInfo& result, std::string* error)
    {
        HttpTextResponse response = {};
        if (!HttpGetText(
                app::build_info::LatestTagApiUrl(),
                GitHubHeaders(),
                response,
                error)) {
            return false;
        }

        if (response.statusCode == 404 || IsGitHubRateLimitResponse(response)) {
            SetNoUpdateDefaults(result);
            if (error)
                error->clear();
            return true;
        }

        if (!IsSuccessStatus(response.statusCode)) {
            if (error)
                *error = FormatGitHubHttpError("Latest tag request failed", response);
            return false;
        }

        json root = json::parse(response.body, nullptr, false);
        if (root.is_discarded() || !root.is_array()) {
            if (error)
                *error = "Latest tag response is not valid JSON.";
            return false;
        }

        if (root.empty()) {
            SetNoUpdateDefaults(result);
            if (error)
                error->clear();
            return true;
        }

        const json& firstTag = root.front();
        if (!firstTag.is_object()) {
            if (error)
                *error = "Latest tag payload is invalid.";
            return false;
        }

        result.latestVersion = Trim(firstTag.value("name", std::string{}));
        result.releaseUrl = app::build_info::RepositoryUrl() + "/releases/tag/" + result.latestVersion;
        if (result.latestVersion.empty()) {
            if (error)
                *error = "Latest tag payload is missing name.";
            return false;
        }

        if (error)
            error->clear();
        return true;
    }
}

bool bootstrap::CheckForVersionUpdate(VersionUpdateInfo* info, std::string* error)
{
    VersionUpdateInfo result = {};
    result.currentVersion = app::build_info::VersionTag();

    HttpTextResponse response = {};
    if (!HttpGetText(
            app::build_info::LatestReleaseApiUrl(),
            GitHubHeaders(),
            response,
            error)) {
        return false;
    }

    if (response.statusCode == 403 || response.statusCode == 429) {
        if (IsGitHubRateLimitResponse(response)) {
            SetNoUpdateDefaults(result);
            if (info)
                *info = result;
            if (error)
                error->clear();
            return true;
        }
    }

    if (response.statusCode == 404) {
        if (!TryLoadLatestTag(result, error))
            return false;
        result.updateAvailable =
            !result.latestVersion.empty() &&
            IsNewerVersion(result.latestVersion, result.currentVersion);
        if (info)
            *info = result;
        if (error)
            error->clear();
        return true;
    }

    if (!IsSuccessStatus(response.statusCode)) {
        if (error)
            *error = FormatGitHubHttpError("Version check failed", response);
        return false;
    }

    json root = json::parse(response.body, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        if (error)
            *error = "Latest release response is not valid JSON.";
        return false;
    }

    result.latestVersion = Trim(root.value("tag_name", std::string{}));
    result.releaseName = Trim(root.value("name", std::string{}));
    result.releaseUrl = Trim(root.value("html_url", std::string{}));

    if (result.latestVersion.empty() || result.releaseUrl.empty()) {
        if (error)
            *error = "Latest release payload is missing tag_name or html_url.";
        return false;
    }

    result.updateAvailable = IsNewerVersion(result.latestVersion, result.currentVersion);

    if (info)
        *info = result;
    if (error)
        error->clear();
    return true;
}

bool bootstrap::OpenVersionUpdateReleasePage(const VersionUpdateInfo& info, std::string* error)
{
    if (info.releaseUrl.empty()) {
        if (error)
            *error = "Release URL is empty.";
        return false;
    }

    const HINSTANCE shellResult = ShellExecuteA(nullptr, "open", info.releaseUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(shellResult) <= 32) {
        if (error)
            *error = "Failed to open release page: " + info.releaseUrl;
        return false;
    }

    return true;
}
