#include "app/Bootstrap/version_update.h"

#include "app/Core/build_info.h"

#include <Windows.h>
#include <Shellapi.h>
#include <winhttp.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <regex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace
{
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

    std::vector<std::wstring> SourceHeaders()
    {
        return {
            L"Accept: text/plain, application/octet-stream;q=0.9, */*;q=0.8",
            ToWide("User-Agent: " + app::build_info::HttpUserAgent())
        };
    }

    bool IsSuccessStatus(DWORD statusCode)
    {
        return statusCode >= 200 && statusCode < 300;
    }

    void SetNoUpdateDefaults(bootstrap::VersionUpdateInfo& result)
    {
        result.updateAvailable = false;
        if (result.releaseUrl.empty())
            result.releaseUrl = app::build_info::RepositoryReleasesUrl();
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
            ToWide(app::build_info::HttpUserAgent()).c_str(),
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

    std::string BuildReleaseUrlForVersion(std::string_view version)
    {
        const std::string cleanVersion = Trim(std::string(version));
        if (cleanVersion.empty())
            return app::build_info::RepositoryReleasesUrl();
        return app::build_info::RepositoryUrl() + "/releases/tag/" + cleanVersion;
    }

    enum class SourceLoadResult
    {
        Loaded,
        Missing,
        Failed
    };

    bool TryExtractQuotedSourceValue(
        const std::string& sourceText,
        const char* constantName,
        std::string* outValue)
    {
        if (!outValue || !constantName)
            return false;

        const std::string pattern = std::string(R"(inline\s+constexpr\s+std::string_view\s+)") +
                                    constantName +
                                    "\\s*=\\s*\"([^\"]+)\"";
        const std::regex regex(pattern);
        std::smatch match;
        if (!std::regex_search(sourceText, match, regex) || match.size() < 2)
            return false;

        *outValue = Trim(match[1].str());
        return !outValue->empty();
    }

    SourceLoadResult TryLoadVersionFromBuildInfoSource(
        std::string_view sourceUrl,
        bootstrap::VersionUpdateInfo& result,
        std::string* error)
    {
        HttpTextResponse response = {};
        if (!HttpGetText(std::string(sourceUrl), SourceHeaders(), response, error))
            return SourceLoadResult::Failed;

        if (response.statusCode == 404) {
            if (error)
                error->clear();
            return SourceLoadResult::Missing;
        }

        if (!IsSuccessStatus(response.statusCode)) {
            if (error)
                *error = "Remote build_info request failed: HTTP " + std::to_string(response.statusCode);
            return SourceLoadResult::Failed;
        }

        if (!TryExtractQuotedSourceValue(response.body, "kVersionTag", &result.latestVersion)) {
            if (error)
                *error = "Remote build_info.h is missing kVersionTag.";
            return SourceLoadResult::Failed;
        }

        result.releaseName = result.latestVersion;
        result.releaseUrl = BuildReleaseUrlForVersion(result.latestVersion);

        result.updateAvailable = IsNewerVersion(result.latestVersion, result.currentVersion);
        if (error)
            error->clear();
        return SourceLoadResult::Loaded;
    }
}

bool bootstrap::CheckForVersionUpdate(VersionUpdateInfo* info, std::string* error)
{
    VersionUpdateInfo result = {};
    result.currentVersion = app::build_info::VersionTag();
    std::string primaryError;
    const SourceLoadResult primaryResult =
        TryLoadVersionFromBuildInfoSource(app::build_info::RemoteBuildInfoUrl(), result, &primaryError);
    if (primaryResult == SourceLoadResult::Loaded) {
        if (info)
            *info = result;
        if (error)
            error->clear();
        return true;
    }

    std::string fallbackError;
    const SourceLoadResult fallbackResult =
        TryLoadVersionFromBuildInfoSource(app::build_info::RemoteBuildInfoFallbackUrl(), result, &fallbackError);
    if (fallbackResult == SourceLoadResult::Loaded) {
        if (info)
            *info = result;
        if (error)
            error->clear();
        return true;
    }

    if (primaryResult == SourceLoadResult::Missing &&
        fallbackResult == SourceLoadResult::Missing) {
        SetNoUpdateDefaults(result);
        if (info)
            *info = result;
        if (error)
            error->clear();
        return true;
    }

    if (error) {
        if (!fallbackError.empty())
            *error = std::move(fallbackError);
        else
            *error = std::move(primaryError);
    }
    return false;
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
