#include "Features/WebRadar/web_remote.h"

#include "app/Config/config.h"
#include "app/Config/project_paths.h"
#include "app/Core/globals.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <json/json.hpp>

#include <libssh2.h>
#include <libssh2_sftp.h>

#pragma comment(lib, "Bcrypt.lib")
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Ws2_32.lib")

namespace
{
    constexpr int kSocketTimeoutMs = 2500;
    constexpr size_t kMaxPendingPayloadBytes = 256 * 1024;

    uint64_t UnixNowMs()
    {
        const auto now = std::chrono::system_clock::now();
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    }

    std::string Trim(std::string_view value)
    {
        size_t first = 0;
        size_t last = value.size();
        while (first < last && std::isspace(static_cast<unsigned char>(value[first])) != 0)
            ++first;
        while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1])) != 0)
            --last;
        return std::string(value.substr(first, last - first));
    }

    std::string SanitizeProfileName(std::string_view profileName)
    {
        std::string out;
        out.reserve(profileName.size());
        for (const unsigned char c : profileName) {
            if (std::isalnum(c) != 0 || c == '_' || c == '-')
                out.push_back(static_cast<char>(c));
        }
        if (out.empty())
            out = "KevqDefault";
        return out;
    }

    std::filesystem::path BuildSettingsPath(std::string_view profileName)
    {
        (void)profileName;
        return app::paths::GetConfigDirectory() / "WebRadarConfig.json";
    }

    std::filesystem::path BuildLegacySettingsPath(std::string_view profileName)
    {
        return app::paths::GetConfigDirectory() / (SanitizeProfileName(profileName) + ".web.json");
    }

    bool IsConfigured(const webradar::remote::Settings& settings)
    {
        return !Trim(settings.host).empty() && settings.webPort > 0 && settings.webPort <= 65535;
    }

    std::string Base64Encode(const unsigned char* data, size_t size)
    {
        static constexpr char kTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve(((size + 2) / 3) * 4);
        for (size_t i = 0; i < size; i += 3) {
            const uint32_t a = data[i];
            const uint32_t b = (i + 1 < size) ? data[i + 1] : 0u;
            const uint32_t c = (i + 2 < size) ? data[i + 2] : 0u;
            const uint32_t triple = (a << 16) | (b << 8) | c;
            out.push_back(kTable[(triple >> 18) & 0x3F]);
            out.push_back(kTable[(triple >> 12) & 0x3F]);
            out.push_back(i + 1 < size ? kTable[(triple >> 6) & 0x3F] : '=');
            out.push_back(i + 2 < size ? kTable[triple & 0x3F] : '=');
        }
        return out;
    }

    std::string ToLowerAscii(std::string_view value)
    {
        std::string out;
        out.reserve(value.size());
        for (const unsigned char c : value)
            out.push_back(static_cast<char>(std::tolower(c)));
        return out;
    }

    std::string ExtractHttpHeader(std::string_view response, std::string_view name)
    {
        const std::string lowerName = ToLowerAscii(name);
        size_t lineStart = 0;
        while (lineStart < response.size()) {
            const size_t lineEnd = response.find("\r\n", lineStart);
            if (lineEnd == std::string_view::npos)
                break;
            const std::string_view line = response.substr(lineStart, lineEnd - lineStart);
            const size_t colon = line.find(':');
            if (colon != std::string_view::npos) {
                const std::string headerName = ToLowerAscii(line.substr(0, colon));
                if (headerName == lowerName)
                    return Trim(line.substr(colon + 1));
            }
            lineStart = lineEnd + 2;
        }
        return {};
    }

    bool Sha1(std::string_view data, std::array<unsigned char, 20>& digest)
    {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA1_ALGORITHM, nullptr, 0) < 0)
            return false;
        const auto closeAlgorithm = [&] { BCryptCloseAlgorithmProvider(algorithm, 0); };
        const NTSTATUS status = BCryptHash(
            algorithm,
            nullptr,
            0,
            reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())),
            static_cast<ULONG>(data.size()),
            digest.data(),
            static_cast<ULONG>(digest.size()));
        closeAlgorithm();
        return status >= 0;
    }

    bool FillRandom(unsigned char* data, size_t size)
    {
        return BCryptGenRandom(nullptr, data, static_cast<ULONG>(size), BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0;
    }

    bool SendAll(SOCKET socketHandle, const char* data, size_t size)
    {
        size_t sent = 0;
        while (sent < size) {
            const int chunk = static_cast<int>(std::min<size_t>(size - sent, 64 * 1024));
            const int result = send(socketHandle, data + sent, chunk, 0);
            if (result <= 0)
                return false;
            sent += static_cast<size_t>(result);
        }
        return true;
    }

    void CloseSocket(SOCKET& socketHandle)
    {
        if (socketHandle != INVALID_SOCKET) {
            shutdown(socketHandle, SD_BOTH);
            closesocket(socketHandle);
            socketHandle = INVALID_SOCKET;
        }
    }

    std::string TrimProcessOutput(std::string output)
    {
        output = Trim(output);
        constexpr size_t kMaxErrorText = 480;
        if (output.size() > kMaxErrorText)
            output = output.substr(0, kMaxErrorText) + "...";
        return output;
    }

    std::string ShellQuote(std::string_view value)
    {
        std::string out = "'";
        for (const char c : value) {
            if (c == '\'')
                out += "'\\''";
            else
                out.push_back(c);
        }
        out.push_back('\'');
        return out;
    }

    bool IsSafeRemotePath(std::string_view value)
    {
        if (value.empty() || value.front() != '/')
            return false;
        if (value.size() > 256)
            return false;
        for (const unsigned char c : value) {
            if (std::isalnum(c) != 0 || c == '/' || c == '_' || c == '-' || c == '.')
                continue;
            return false;
        }
        if (value.find("..") != std::string_view::npos)
            return false;
        if (value.find("//") != std::string_view::npos)
            return false;
        if (value.front() == '/' && value.size() > 1) {
            char prev = '\0';
            for (const char c : value) {
                if (c == '.' && prev == '/')
                    return false;
                prev = c;
            }
        }
        static constexpr std::string_view kBlockedPrefixes[] = {
            "/etc",
            "/bin",
            "/sbin",
            "/boot",
            "/proc",
            "/sys",
            "/dev",
            "/lib",
            "/lib64",
            "/usr/bin",
            "/usr/sbin",
            "/usr/lib",
            "/usr/lib64",
            "/usr/share",
            "/run",
            "/var/run",
            "/var/log",
            "/root",
            "/.",
        };
        for (const auto& prefix : kBlockedPrefixes) {
            if (value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0) {
                if (value.size() == prefix.size() || value[prefix.size()] == '/')
                    return false;
            }
        }
        if (value == "/")
            return false;
        return true;
    }

    std::string NormalizeRemotePath(std::string value)
    {
        value = Trim(value);
        while (value.size() > 1 && value.back() == '/')
            value.pop_back();
        return value.empty() ? "/opt/kevqdma-webradar" : value;
    }

    SOCKET ConnectTcp(const std::string& host, int port, int* outMs, std::string* outError)
    {
        if (outMs)
            *outMs = -1;
        const std::string cleanHost = Trim(host);
        if (cleanHost.empty()) {
            if (outError)
                *outError = "Host is empty.";
            return INVALID_SOCKET;
        }

        addrinfo hints = {};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        addrinfo* result = nullptr;
        const std::string portText = std::to_string(std::clamp(port, 1, 65535));
        const auto start = std::chrono::steady_clock::now();
        const int gai = getaddrinfo(cleanHost.c_str(), portText.c_str(), &hints, &result);
        if (gai != 0 || !result) {
            if (outError)
                *outError = "DNS/connect resolve failed.";
            return INVALID_SOCKET;
        }

        SOCKET connected = INVALID_SOCKET;
        for (addrinfo* ptr = result; ptr; ptr = ptr->ai_next) {
            SOCKET candidate = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
            if (candidate == INVALID_SOCKET)
                continue;

            DWORD timeout = kSocketTimeoutMs;
            setsockopt(candidate, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
            setsockopt(candidate, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
            BOOL noDelay = TRUE;
            setsockopt(candidate, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));

            if (connect(candidate, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) == 0) {
                connected = candidate;
                break;
            }
            closesocket(candidate);
        }
        freeaddrinfo(result);

        if (connected == INVALID_SOCKET) {
            if (outError)
                *outError = "TCP connect failed.";
            return INVALID_SOCKET;
        }

        if (outMs) {
            const auto end = std::chrono::steady_clock::now();
            *outMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
        }
        return connected;
    }

    bool EnsureLibssh2(std::string* outError)
    {
        static std::once_flag initFlag;
        static int initResult = -1;
        std::call_once(initFlag, [] {
            initResult = libssh2_init(0);
        });
        if (initResult != 0) {
            if (outError)
                *outError = "libssh2 initialization failed.";
            return false;
        }
        return true;
    }

    std::string Libssh2LastError(LIBSSH2_SESSION* session, const char* fallback)
    {
        if (!session)
            return fallback;
        char* message = nullptr;
        int length = 0;
        libssh2_session_last_error(session, &message, &length, 0);
        if (message && length > 0)
            return TrimProcessOutput(std::string(message, static_cast<size_t>(length)));
        return fallback;
    }

    class SshSession {
    public:
        SshSession() = default;
        SshSession(const SshSession&) = delete;
        SshSession& operator=(const SshSession&) = delete;

        ~SshSession()
        {
            Reset();
        }

        bool Connect(const webradar::remote::Settings& settings, int timeoutMs, std::string* outError)
        {
            Reset();
            if (!EnsureLibssh2(outError))
                return false;
            if (Trim(settings.host).empty()) {
                if (outError)
                    *outError = "Host is empty.";
                return false;
            }
            if (Trim(settings.login).empty()) {
                if (outError)
                    *outError = "SSH login is empty.";
                return false;
            }
            if (Trim(settings.password).empty()) {
                if (outError)
                    *outError = "SSH password is empty.";
                return false;
            }

            WSADATA wsa = {};
            if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
                if (outError)
                    *outError = "WinSock initialization failed.";
                return false;
            }
            wsaStarted_ = true;

            socket_ = ConnectTcp(settings.host, settings.sshPort, nullptr, outError);
            if (socket_ == INVALID_SOCKET)
                return false;

            session_ = libssh2_session_init();
            if (!session_) {
                if (outError)
                    *outError = "SSH session allocation failed.";
                return false;
            }

            libssh2_session_set_blocking(session_, 1);
            libssh2_session_set_timeout(session_, timeoutMs);

            if (libssh2_session_handshake(session_, socket_) != 0) {
                if (outError)
                    *outError = Libssh2LastError(session_, "SSH handshake failed.");
                return false;
            }

            const std::string login = Trim(settings.login);
            const std::string password = settings.password;
            const int auth = libssh2_userauth_password_ex(
                session_,
                login.c_str(),
                static_cast<unsigned int>(login.size()),
                password.c_str(),
                static_cast<unsigned int>(password.size()),
                nullptr);
            if (auth != 0) {
                if (outError)
                    *outError = Libssh2LastError(session_, "SSH authentication failed.");
                return false;
            }
            return true;
        }

        LIBSSH2_SESSION* Get() const
        {
            return session_;
        }

        SOCKET Socket() const
        {
            return socket_;
        }

    private:
        void Reset()
        {
            if (session_) {
                libssh2_session_disconnect(session_, "KevqDMA WebRadar deploy finished");
                libssh2_session_free(session_);
                session_ = nullptr;
            }
            CloseSocket(socket_);
            if (wsaStarted_) {
                WSACleanup();
                wsaStarted_ = false;
            }
        }

        LIBSSH2_SESSION* session_ = nullptr;
        SOCKET socket_ = INVALID_SOCKET;
        bool wsaStarted_ = false;
    };

    bool WaitSshSocket(SshSession& ssh, int timeoutMs)
    {
        LIBSSH2_SESSION* session = ssh.Get();
        timeval timeout = {};
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;

        fd_set readSet = {};
        fd_set writeSet = {};
        fd_set exceptSet = {};
        FD_ZERO(&readSet);
        FD_ZERO(&writeSet);
        FD_ZERO(&exceptSet);
        FD_SET(ssh.Socket(), &exceptSet);

        const int directions = libssh2_session_block_directions(session);
        if ((directions & LIBSSH2_SESSION_BLOCK_INBOUND) != 0)
            FD_SET(ssh.Socket(), &readSet);
        if ((directions & LIBSSH2_SESSION_BLOCK_OUTBOUND) != 0)
            FD_SET(ssh.Socket(), &writeSet);

        if ((directions & (LIBSSH2_SESSION_BLOCK_INBOUND | LIBSSH2_SESSION_BLOCK_OUTBOUND)) == 0)
            FD_SET(ssh.Socket(), &readSet);

        return select(0, &readSet, &writeSet, &exceptSet, &timeout) >= 0;
    }

    bool RunSshCommand(SshSession& ssh, std::string_view command, std::string* outOutput, std::string* outError, int timeoutMs = 300000)
    {
        LIBSSH2_SESSION* session = ssh.Get();
        libssh2_session_set_blocking(session, 0);
        LIBSSH2_CHANNEL* channel = libssh2_channel_open_session(session);
        while (!channel && libssh2_session_last_errno(session) == LIBSSH2_ERROR_EAGAIN) {
            if (!WaitSshSocket(ssh, 250)) {
                if (outError)
                    *outError = "SSH socket wait failed.";
                libssh2_session_set_blocking(session, 1);
                return false;
            }
            channel = libssh2_channel_open_session(session);
        }
        if (!channel) {
            if (outError)
                *outError = Libssh2LastError(session, "SSH channel open failed.");
            libssh2_session_set_blocking(session, 1);
            return false;
        }

        auto closeChannel = [&] {
            libssh2_channel_close(channel);
            libssh2_channel_free(channel);
            libssh2_session_set_blocking(session, 1);
        };

        const std::string commandText(command);
        int rc = LIBSSH2_ERROR_EAGAIN;
        while (rc == LIBSSH2_ERROR_EAGAIN) {
            rc = libssh2_channel_exec(channel, commandText.c_str());
            if (rc == LIBSSH2_ERROR_EAGAIN)
                WaitSshSocket(ssh, 250);
        }
        if (rc != 0) {
            if (outError)
                *outError = Libssh2LastError(session, "SSH command start failed.");
            closeChannel();
            return false;
        }

        std::string output;
        std::array<char, 4096> buffer = {};
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        bool stdoutOpen = true;
        bool stderrOpen = true;
        while (stdoutOpen || stderrOpen) {
            bool madeProgress = false;

            if (stdoutOpen) {
                for (;;) {
                    const auto read = libssh2_channel_read_ex(channel, 0, buffer.data(), buffer.size());
                    if (read > 0) {
                        output.append(buffer.data(), static_cast<size_t>(read));
                        madeProgress = true;
                        continue;
                    }
                    if (read == LIBSSH2_ERROR_EAGAIN)
                        break;
                    stdoutOpen = false;
                    break;
                }
            }

            if (stderrOpen) {
                for (;;) {
                    const auto read = libssh2_channel_read_ex(channel, SSH_EXTENDED_DATA_STDERR, buffer.data(), buffer.size());
                    if (read > 0) {
                        output.append(buffer.data(), static_cast<size_t>(read));
                        madeProgress = true;
                        continue;
                    }
                    if (read == LIBSSH2_ERROR_EAGAIN)
                        break;
                    stderrOpen = false;
                    break;
                }
            }

            if (libssh2_channel_eof(channel) != 0)
                break;

            if (std::chrono::steady_clock::now() >= deadline) {
                if (outError) {
                    const std::string text = TrimProcessOutput(output);
                    *outError = text.empty() ? "Remote command timed out." : "Remote command timed out: " + text;
                }
                closeChannel();
                return false;
            }

            if (!madeProgress)
                WaitSshSocket(ssh, 250);
        }

        while ((rc = libssh2_channel_close(channel)) == LIBSSH2_ERROR_EAGAIN)
            WaitSshSocket(ssh, 250);
        const int exitStatus = libssh2_channel_get_exit_status(channel);
        closeChannel();

        if (outOutput)
            *outOutput = output;

        if (exitStatus != 0) {
            if (outError) {
                const std::string text = TrimProcessOutput(output);
                *outError = text.empty() ? "Remote command failed." : text;
            }
            return false;
        }
        return true;
    }

    bool RemotePathExists(LIBSSH2_SFTP* sftp, std::string_view path)
    {
        LIBSSH2_SFTP_ATTRIBUTES attrs = {};
        const std::string pathText(path);
        return libssh2_sftp_stat_ex(
            sftp,
            pathText.c_str(),
            static_cast<unsigned int>(pathText.size()),
            LIBSSH2_SFTP_STAT,
            &attrs) == 0;
    }

    bool SftpMkdirs(LIBSSH2_SFTP* sftp, std::string_view remotePath, std::string* outError)
    {
        std::string path = NormalizeRemotePath(std::string(remotePath));
        std::string current;
        size_t offset = 1;
        while (offset <= path.size()) {
            const size_t slash = path.find('/', offset);
            const std::string segment = path.substr(offset, slash == std::string::npos ? std::string::npos : slash - offset);
            if (!segment.empty()) {
                current += "/";
                current += segment;
                if (!RemotePathExists(sftp, current)) {
                    const int rc = libssh2_sftp_mkdir_ex(
                        sftp,
                        current.c_str(),
                        static_cast<unsigned int>(current.size()),
                        0755);
                    if (rc != 0 && !RemotePathExists(sftp, current)) {
                        if (outError)
                            *outError = "Cannot create remote directory: " + current;
                        return false;
                    }
                }
            }
            if (slash == std::string::npos)
                break;
            offset = slash + 1;
        }
        return true;
    }

    bool SftpUploadFile(LIBSSH2_SFTP* sftp, const std::filesystem::path& localPath, std::string_view remotePath, std::string* outError)
    {
        std::ifstream input(localPath, std::ios::binary);
        if (!input.is_open()) {
            if (outError)
                *outError = "Cannot read deploy file.";
            return false;
        }

        const std::string remoteFile(remotePath);
        LIBSSH2_SFTP_HANDLE* handle = libssh2_sftp_open_ex(
            sftp,
            remoteFile.c_str(),
            static_cast<unsigned int>(remoteFile.size()),
            LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC,
            0644,
            LIBSSH2_SFTP_OPENFILE);
        if (!handle) {
            if (outError)
                *outError = "Cannot open remote file: " + remoteFile;
            return false;
        }

        std::array<char, 64 * 1024> buffer = {};
        while (input.good()) {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            size_t remaining = static_cast<size_t>(input.gcount());
            size_t offset = 0;
            while (remaining > 0) {
                const auto written = libssh2_sftp_write(handle, buffer.data() + offset, remaining);
                if (written <= 0) {
                    libssh2_sftp_close(handle);
                    if (outError)
                        *outError = "SFTP upload failed: " + remoteFile;
                    return false;
                }
                offset += static_cast<size_t>(written);
                remaining -= static_cast<size_t>(written);
            }
        }

        libssh2_sftp_close(handle);
        return true;
    }

    bool SftpUploadDirectory(SshSession& ssh, const std::filesystem::path& localRoot, std::string_view remoteRoot, std::string* outError)
    {
        LIBSSH2_SFTP* sftp = libssh2_sftp_init(ssh.Get());
        if (!sftp) {
            if (outError)
                *outError = Libssh2LastError(ssh.Get(), "SFTP initialization failed.");
            return false;
        }

        auto shutdownSftp = [&] {
            libssh2_sftp_shutdown(sftp);
        };

        const std::string root = NormalizeRemotePath(std::string(remoteRoot));
        if (!SftpMkdirs(sftp, root, outError)) {
            shutdownSftp();
            return false;
        }

        std::error_code ec;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(localRoot, ec)) {
            if (ec) {
                if (outError)
                    *outError = "Cannot enumerate deploy package.";
                shutdownSftp();
                return false;
            }

            const auto relative = std::filesystem::relative(entry.path(), localRoot, ec);
            if (ec)
                continue;

            std::string remotePath = root + "/" + relative.generic_string();
            if (entry.is_directory(ec)) {
                if (!SftpMkdirs(sftp, remotePath, outError)) {
                    shutdownSftp();
                    return false;
                }
                continue;
            }

            const auto parent = std::filesystem::path(remotePath).parent_path().generic_string();
            if (!SftpMkdirs(sftp, parent, outError) || !SftpUploadFile(sftp, entry.path(), remotePath, outError)) {
                shutdownSftp();
                return false;
            }
        }

        shutdownSftp();
        return true;
    }

    void ReportDeployProgress(const webradar::remote::DeployProgressCallback& progress, std::string_view text)
    {
        if (progress)
            progress(text);
    }

    bool ReadHttpHeader(SOCKET socketHandle, std::string& header)
    {
        header.clear();
        std::array<char, 1024> buffer = {};
        while (header.size() < 16 * 1024) {
            const int received = recv(socketHandle, buffer.data(), static_cast<int>(buffer.size()), 0);
            if (received <= 0)
                return false;
            header.append(buffer.data(), static_cast<size_t>(received));
            if (header.find("\r\n\r\n") != std::string::npos)
                return true;
        }
        return false;
    }

    bool WebSocketHandshake(SOCKET socketHandle, const webradar::remote::Settings& settings, const char* path, std::string* outError)
    {
        std::array<unsigned char, 16> keyBytes = {};
        if (!FillRandom(keyBytes.data(), keyBytes.size())) {
            if (outError)
                *outError = "Random key failed.";
            return false;
        }

        const std::string key = Base64Encode(keyBytes.data(), keyBytes.size());
        std::string request;
        request.reserve(512);
        request += "GET ";
        request += path;
        request += " HTTP/1.1\r\nHost: ";
        request += Trim(settings.host);
        request += ":";
        request += std::to_string(settings.webPort);
        request += "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: ";
        request += key;
        request += "\r\nUser-Agent: KevqDMA-WebRadar/1.0\r\n\r\n";

        if (!SendAll(socketHandle, request.data(), request.size())) {
            if (outError)
                *outError = "Handshake send failed.";
            return false;
        }

        std::string response;
        if (!ReadHttpHeader(socketHandle, response)) {
            if (outError)
                *outError = "Handshake response timeout.";
            return false;
        }

        if (response.find(" 101 ") == std::string::npos && response.find(" 101\r\n") == std::string::npos) {
            if (outError)
                *outError = "Server did not accept WebSocket.";
            return false;
        }

        std::array<unsigned char, 20> acceptDigest = {};
        const std::string acceptSeed = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        if (!Sha1(acceptSeed, acceptDigest)) {
            if (outError)
                *outError = "WebSocket accept hash failed.";
            return false;
        }

        const std::string expectedAccept = Base64Encode(acceptDigest.data(), acceptDigest.size());
        const std::string actualAccept = ExtractHttpHeader(response, "Sec-WebSocket-Accept");
        if (actualAccept != expectedAccept) {
            if (outError)
                *outError = "WebSocket accept validation failed.";
            return false;
        }

        return true;
    }

    std::string BuildClientTextFrame(const std::string& payload)
    {
        std::string frame;
        const uint64_t size = static_cast<uint64_t>(payload.size());
        frame.reserve(payload.size() + 16);
        frame.push_back(static_cast<char>(0x81));
        if (size <= 125) {
            frame.push_back(static_cast<char>(0x80u | static_cast<unsigned char>(size)));
        } else if (size <= 0xFFFFu) {
            frame.push_back(static_cast<char>(0x80u | 126u));
            frame.push_back(static_cast<char>((size >> 8) & 0xFFu));
            frame.push_back(static_cast<char>(size & 0xFFu));
        } else {
            frame.push_back(static_cast<char>(0x80u | 127u));
            for (int shift = 56; shift >= 0; shift -= 8)
                frame.push_back(static_cast<char>((size >> shift) & 0xFFu));
        }

        std::array<unsigned char, 4> mask = {};
        if (!FillRandom(mask.data(), mask.size())) {
            mask = { 0x41, 0x57, 0x31, 0x9A };
        }

        for (const unsigned char c : mask)
            frame.push_back(static_cast<char>(c));

        const size_t offset = frame.size();
        frame.resize(offset + payload.size());
        for (size_t i = 0; i < payload.size(); ++i)
            frame[offset + i] = static_cast<char>(static_cast<unsigned char>(payload[i]) ^ mask[i & 3u]);
        return frame;
    }

    bool HttpGetStatus(const webradar::remote::Settings& settings, std::string* outError)
    {
        int pingMs = -1;
        SOCKET socketHandle = ConnectTcp(settings.host, settings.webPort, &pingMs, outError);
        if (socketHandle == INVALID_SOCKET)
            return false;

        std::string request;
        request += "GET /api/status HTTP/1.1\r\nHost: ";
        request += Trim(settings.host);
        request += "\r\nConnection: close\r\nUser-Agent: KevqDMA-WebRadar/1.0\r\n\r\n";
        const bool sent = SendAll(socketHandle, request.data(), request.size());
        std::string response;
        const bool read = sent && ReadHttpHeader(socketHandle, response);
        CloseSocket(socketHandle);

        if (!sent || !read) {
            if (outError)
                *outError = "HTTP status request failed.";
            return false;
        }

        if (response.find(" 200 ") == std::string::npos && response.find(" 200\r\n") == std::string::npos) {
            if (outError)
                *outError = "Remote server returned non-200 status.";
            return false;
        }

        return true;
    }

    class RemoteSender {
    public:
        void Start()
        {
            bool expected = false;
            if (!running_.compare_exchange_strong(expected, true))
                return;
            worker_ = std::thread(&RemoteSender::Loop, this);
        }

        void Stop()
        {
            running_.store(false, std::memory_order_relaxed);
            queueCv_.notify_all();
            CloseConnection();
            if (worker_.joinable())
                worker_.join();
        }

        void Configure(const webradar::remote::Settings& settings)
        {
            bool disabled = false;
            {
                std::lock_guard<std::mutex> lock(stateMutex_);
                settings_ = settings;
                settings_.host = Trim(settings_.host);
                settings_.webPort = std::clamp(settings_.webPort, 1, 65535);
                settings_.sshPort = std::clamp(settings_.sshPort, 1, 65535);
                stats_.enabled = settings_.enabled;
                stats_.configured = IsConfigured(settings_);
                disabled = !settings_.enabled;
                if (disabled)
                    stats_.connected = false;
            }
            if (disabled) {
                ClearPendingPayload();
                CloseConnection();
            }
            queueCv_.notify_all();
        }

        void Publish(const std::string& payload)
        {
            if (payload.empty() || payload.size() > kMaxPendingPayloadBytes)
                return;

            {
                std::lock_guard<std::mutex> stateLock(stateMutex_);
                if (!settings_.enabled || !IsConfigured(settings_))
                    return;
            }

            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                if (!pendingPayload_.empty()) {
                    std::lock_guard<std::mutex> stateLock(stateMutex_);
                    ++stats_.queueDrops;
                }
                pendingPayload_ = payload;
            }
            queueCv_.notify_one();
        }

        webradar::remote::Stats GetStats() const
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            return stats_;
        }

        bool HasDemand() const
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            return settings_.enabled && IsConfigured(settings_);
        }

    private:
        void SetError(const std::string& error)
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            stats_.lastError = error;
            stats_.connected = false;
        }

        void MarkConnected()
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            stats_.connected = true;
            stats_.lastError.clear();
        }

        void MarkSent(size_t bytes)
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            stats_.connected = true;
            ++stats_.sentPackets;
            stats_.sentBytes += bytes;
            stats_.lastSendUnixMs = UnixNowMs();
        }

        webradar::remote::Settings CurrentSettings() const
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            return settings_;
        }

        void ClearPendingPayload()
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            pendingPayload_.clear();
        }

        SOCKET CurrentSocket() const
        {
            std::lock_guard<std::mutex> lock(socketMutex_);
            return socketHandle_;
        }

        void ReplaceConnection(SOCKET& socketHandle)
        {
            std::lock_guard<std::mutex> lock(socketMutex_);
            CloseSocket(socketHandle_);
            socketHandle_ = socketHandle;
            socketHandle = INVALID_SOCKET;
        }

        void CloseConnection()
        {
            SOCKET socketHandle = INVALID_SOCKET;
            {
                std::lock_guard<std::mutex> lock(socketMutex_);
                socketHandle = socketHandle_;
                socketHandle_ = INVALID_SOCKET;
            }
            CloseSocket(socketHandle);

            std::lock_guard<std::mutex> lock(stateMutex_);
            stats_.connected = false;
        }

        bool Connect(SOCKET& socketHandle, const webradar::remote::Settings& settings)
        {
            std::string error;
            int pingMs = -1;
            socketHandle = ConnectTcp(settings.host, settings.webPort, &pingMs, &error);
            {
                std::lock_guard<std::mutex> lock(stateMutex_);
                stats_.lastPingMs = pingMs;
            }
            if (socketHandle == INVALID_SOCKET) {
                SetError(error);
                return false;
            }

            if (!WebSocketHandshake(socketHandle, settings, "/api/ingest", &error)) {
                CloseSocket(socketHandle);
                SetError(error);
                return false;
            }

            {
                std::lock_guard<std::mutex> lock(stateMutex_);
                ++stats_.reconnects;
            }
            MarkConnected();
            return true;
        }

        void Loop()
        {
            WSADATA wsa = {};
            WSAStartup(MAKEWORD(2, 2), &wsa);

            while (running_.load(std::memory_order_relaxed)) {
                std::string payload;
                {
                    std::unique_lock<std::mutex> lock(queueMutex_);
                    queueCv_.wait_for(lock, std::chrono::milliseconds(500), [&] {
                        return !running_.load(std::memory_order_relaxed) || !pendingPayload_.empty();
                    });
                    if (!running_.load(std::memory_order_relaxed))
                        break;
                    payload.swap(pendingPayload_);
                }

                if (payload.empty()) {
                    const auto settings = CurrentSettings();
                    if (!settings.enabled)
                        CloseConnection();
                    continue;
                }

                const auto settings = CurrentSettings();
                if (!settings.enabled || !IsConfigured(settings)) {
                    CloseConnection();
                    continue;
                }

                if (CurrentSocket() == INVALID_SOCKET) {
                    SOCKET newSocket = INVALID_SOCKET;
                    if (!Connect(newSocket, settings)) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(750));
                        continue;
                    }
                    const auto latestSettings = CurrentSettings();
                    if (!latestSettings.enabled ||
                        !IsConfigured(latestSettings) ||
                        latestSettings.host != settings.host ||
                        latestSettings.webPort != settings.webPort) {
                        CloseSocket(newSocket);
                        CloseConnection();
                        continue;
                    }
                    ReplaceConnection(newSocket);
                }

                const std::string frame = BuildClientTextFrame(payload);
                const auto latestSettings = CurrentSettings();
                if (!latestSettings.enabled || !IsConfigured(latestSettings)) {
                    CloseConnection();
                    continue;
                }
                const SOCKET socketHandle = CurrentSocket();
                if (socketHandle == INVALID_SOCKET)
                    continue;
                if (!SendAll(socketHandle, frame.data(), frame.size())) {
                    CloseConnection();
                    SetError("Remote WebSocket send failed.");
                    std::this_thread::sleep_for(std::chrono::milliseconds(350));
                    continue;
                }

                MarkSent(payload.size());
            }

            CloseConnection();
            WSACleanup();
        }

    private:
        mutable std::mutex stateMutex_;
        webradar::remote::Settings settings_;
        webradar::remote::Stats stats_;
        std::atomic<bool> running_{ false };
        std::thread worker_;
        std::mutex queueMutex_;
        std::condition_variable queueCv_;
        std::string pendingPayload_;
        mutable std::mutex socketMutex_;
        SOCKET socketHandle_ = INVALID_SOCKET;
    };

    RemoteSender& Sender()
    {
        static RemoteSender sender;
        return sender;
    }

    const char* RemoteServerJs()
    {
        return R"JS(const http = require("http");
const fs = require("fs");
const path = require("path");
const express = require("express");
const { WebSocketServer, WebSocket } = require("ws");

const PORT = Number(process.env.PORT || 8080);
const PUBLIC_DIR = path.join(__dirname, "public");
const app = express();
const server = http.createServer(app);
const viewers = new Set();
let latest = { v: 2, seq: 0, ts: Date.now(), map: "unknown", lt: 0, p: [], b: [0, [0, 0, 0], 0, 40, 0, 10], w: [] };
let latestText = JSON.stringify(latest);
let lastUpdateMs = 0;
let sourcePackets = 0;
let broadcastPackets = 0;
let dirty = false;
const BROADCAST_HZ = Number(process.env.BROADCAST_HZ || 60);
const BROADCAST_INTERVAL_MS = Math.max(16, Math.round(1000 / Math.max(1, Math.min(60, BROADCAST_HZ))));
const MAX_VIEWER_BUFFER_BYTES = 64 * 1024;

app.use(express.static(PUBLIC_DIR, { etag: true, maxAge: "1h" }));
app.get("/api/status", (req, res) => {
  res.set("Cache-Control", "no-store");
  res.json({
    ok: true,
    active_map: latest.map || "unknown",
    sent_packets: sourcePackets,
    broadcast_packets: broadcastPackets,
    last_update_ms: lastUpdateMs,
    connected_clients: viewers.size
  });
});
app.get("/api/live", (req, res) => {
  res.set("Cache-Control", "no-store");
  res.json(latest);
});
app.get("/api/stream", (req, res) => {
  res.writeHead(200, {
    "Content-Type": "text/event-stream",
    "Cache-Control": "no-store",
    "Connection": "keep-alive",
    "Access-Control-Allow-Origin": "*"
  });
  res.write(`event: snapshot\ndata: ${JSON.stringify(latest)}\n\n`);
  const client = {
    sse: true,
    send: text => {
      if (res.destroyed || res.writableEnded || res.writableNeedDrain) return false;
      return res.write(`event: snapshot\ndata: ${text}\n\n`);
    }
  };
  viewers.add(client);
  req.on("close", () => viewers.delete(client));
});
app.get("*", (req, res) => res.sendFile(path.join(PUBLIC_DIR, "index.html")));

const sourceWss = new WebSocketServer({ noServer: true });
const viewerWss = new WebSocketServer({ noServer: true });

function broadcastLatest() {
  if (!dirty) return;
  dirty = false;
  const text = latestText;
  for (const client of viewers) {
    try {
      if (client.sse === true) {
        client.send(text);
      } else if (client.readyState === WebSocket.OPEN) {
        if (client.bufferedAmount > MAX_VIEWER_BUFFER_BYTES) continue;
        client.send(text);
      }
    } catch {}
  }
  broadcastPackets += 1;
}
setInterval(broadcastLatest, BROADCAST_INTERVAL_MS).unref();

sourceWss.on("connection", ws => {
  ws.on("message", raw => {
    const text = raw.toString();
    if (text.length < 8 || text.length > 262144) return;
    try {
      const parsed = JSON.parse(text);
      if (!parsed || Number(parsed.v || 0) !== 2) return;
      latest = parsed;
      latestText = text;
      lastUpdateMs = Date.now();
      sourcePackets += 1;
      dirty = true;
    } catch {}
  });
});

viewerWss.on("connection", ws => {
  viewers.add(ws);
  ws.send(latestText);
  ws.on("close", () => viewers.delete(ws));
  ws.on("error", () => viewers.delete(ws));
});

server.on("upgrade", (req, socket, head) => {
  const url = new URL(req.url, "http://127.0.0.1");
  if (url.pathname === "/api/ingest") {
    sourceWss.handleUpgrade(req, socket, head, ws => sourceWss.emit("connection", ws, req));
    return;
  }
  if (url.pathname === "/api/ws") {
    viewerWss.handleUpgrade(req, socket, head, ws => viewerWss.emit("connection", ws, req));
    return;
  }
  socket.destroy();
});

server.listen(PORT, "0.0.0.0", () => {
  console.log(`KevqDMA WebRadar server listening on 0.0.0.0:${PORT}`);
});
)JS";
    }

    const char* PackageJson()
    {
        return R"JSON({"name":"kevqdma-webradar-server","version":"1.0.0","private":true,"main":"server.js","scripts":{"start":"node server.js"},"dependencies":{"express":"^4.19.2","ws":"^8.20.0"},"engines":{"node":">=18"}})JSON";
    }

    struct ResourceExportContext {
        std::filesystem::path publicDir;
        std::string error;
        size_t exported = 0;
    };

    BOOL CALLBACK ExportResourceCallback(HMODULE moduleHandle, LPCSTR type, LPSTR name, LONG_PTR param)
    {
        auto* context = reinterpret_cast<ResourceExportContext*>(param);
        if (!context || IS_INTRESOURCE(name))
            return TRUE;

        const std::string urlPath(name);
        if (urlPath.empty() || urlPath[0] != '/')
            return TRUE;

        const HRSRC resource = FindResourceA(moduleHandle, name, type);
        if (!resource)
            return TRUE;
        const HGLOBAL loaded = LoadResource(moduleHandle, resource);
        const void* data = loaded ? LockResource(loaded) : nullptr;
        const DWORD size = SizeofResource(moduleHandle, resource);
        if (!data || size == 0)
            return TRUE;

        std::filesystem::path relative = std::filesystem::path(urlPath.substr(1));
        if (relative.empty())
            return TRUE;
        const auto outPath = context->publicDir / relative;
        std::error_code ec;
        std::filesystem::create_directories(outPath.parent_path(), ec);
        if (ec) {
            context->error = "Cannot create asset directory.";
            return FALSE;
        }
        std::ofstream output(outPath, std::ios::binary | std::ios::trunc);
        if (!output.is_open()) {
            context->error = "Cannot write embedded asset.";
            return FALSE;
        }
        output.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        ++context->exported;
        return TRUE;
    }

    bool ExportEmbeddedAssets(const std::filesystem::path& publicDir, std::string* outError)
    {
        ResourceExportContext context;
        context.publicDir = publicDir;
        const HMODULE moduleHandle = GetModuleHandleA(nullptr);
        if (!EnumResourceNamesA(moduleHandle, MAKEINTRESOURCEA(10), ExportResourceCallback, reinterpret_cast<LONG_PTR>(&context))) {
            if (outError)
                *outError = context.error.empty() ? "Embedded asset export failed." : context.error;
            return false;
        }
        if (context.exported == 0) {
            if (outError)
                *outError = "No embedded WEBRadar assets found.";
            return false;
        }
        return true;
    }
}

namespace webradar::remote
{
    void Start()
    {
        Sender().Start();
    }

    void Stop()
    {
        Sender().Stop();
    }

    void Configure(const Settings& settings)
    {
        Sender().Configure(settings);
    }

    void Publish(const std::string& compactPayload)
    {
        Sender().Publish(compactPayload);
    }

    Stats GetStats()
    {
        return Sender().GetStats();
    }

    bool HasActiveConsumerDemand()
    {
        return Sender().HasDemand();
    }

    Settings CaptureSettingsFromGlobals()
    {
        Settings settings;
        settings.enabled = g::webRadarRemoteEnabled;
        settings.host = g::webRadarRemoteHost;
        settings.webPort = g::webRadarRemoteWebPort;
        settings.sshPort = g::webRadarRemoteSshPort;
        settings.login = g::webRadarRemoteLogin;
        settings.password = g::webRadarRemotePassword;
        settings.remotePath = g::webRadarRemotePath;
        return settings;
    }

    void ApplySettingsToGlobals(const Settings& settings)
    {
        g::webRadarRemoteEnabled = settings.enabled;
        g::webRadarRemoteHost = settings.host;
        g::webRadarRemoteWebPort = settings.webPort;
        g::webRadarRemoteSshPort = settings.sshPort;
        g::webRadarRemoteLogin = settings.login;
        g::webRadarRemotePassword = settings.password;
        g::webRadarRemotePath = settings.remotePath;
    }

    bool LoadSettings(std::string_view profileName)
    {
        Settings settings;
        auto path = BuildSettingsPath(profileName);
        const auto legacyPath = BuildLegacySettingsPath(profileName);
        if (!std::filesystem::exists(path) && std::filesystem::exists(legacyPath))
            path = legacyPath;
        std::ifstream file(path, std::ios::binary);
        if (file.is_open()) {
            nlohmann::json root = nlohmann::json::parse(file, nullptr, false);
            if (!root.is_discarded() && root.is_object()) {
                settings.enabled = root.value("EnableWeb", settings.enabled);
                settings.host = root.value("Host", settings.host);
                settings.webPort = root.value("WebPort", settings.webPort);
                settings.sshPort = root.value("SshPort", settings.sshPort);
                settings.login = root.value("Login", settings.login);
                settings.password = root.value("Password", settings.password);
                settings.remotePath = root.value("RemotePath", settings.remotePath);
            }
        }

        settings.webPort = std::clamp(settings.webPort, 1, 65535);
        settings.sshPort = std::clamp(settings.sshPort, 1, 65535);
        if (Trim(settings.login).empty())
            settings.login = "root";
        if (Trim(settings.remotePath).empty())
            settings.remotePath = "/opt/kevqdma-webradar";
        ApplySettingsToGlobals(settings);
        Configure(settings);
        return true;
    }

    bool SaveSettings(std::string_view profileName)
    {
        const Settings settings = CaptureSettingsFromGlobals();
        const auto path = BuildSettingsPath(profileName);
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        nlohmann::ordered_json root;
        root["EnableWeb"] = settings.enabled;
        root["Host"] = settings.host;
        root["WebPort"] = std::clamp(settings.webPort, 1, 65535);
        root["SshPort"] = std::clamp(settings.sshPort, 1, 65535);
        root["Login"] = settings.login;
        root["Password"] = settings.password;
        root["RemotePath"] = settings.remotePath;
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file.is_open())
            return false;
        file << root.dump(4);
        return true;
    }

    bool TestPing(const Settings& settings, int* outMs, std::string* outError)
    {
        WSADATA wsa = {};
        WSAStartup(MAKEWORD(2, 2), &wsa);
        SOCKET socketHandle = ConnectTcp(settings.host, settings.sshPort, outMs, outError);
        const bool ok = socketHandle != INVALID_SOCKET;
        CloseSocket(socketHandle);
        WSACleanup();
        return ok;
    }

    bool TestHttpStatus(const Settings& settings, std::string* outError)
    {
        WSADATA wsa = {};
        WSAStartup(MAKEWORD(2, 2), &wsa);
        const bool ok = HttpGetStatus(settings, outError);
        WSACleanup();
        return ok;
    }

    bool TestSshConnection(const Settings& settings, std::string* outError)
    {
        SshSession ssh;
        if (!ssh.Connect(settings, 15000, outError))
            return false;

        std::string output;
        std::string commandError;
        if (!RunSshCommand(ssh, "echo KEVQ_OK", &output, &commandError)) {
            if (outError)
                *outError = commandError.empty() ? "SSH authentication failed." : commandError;
            return false;
        }
        const std::string trimmed = Trim(output);
        if (trimmed.find("KEVQ_OK") == std::string::npos) {
            if (outError)
                *outError = "SSH test returned unexpected response.";
            return false;
        }
        return true;
    }

    bool CheckServerReady(const Settings& settings, std::string* outError)
    {
        const std::string remotePath = NormalizeRemotePath(settings.remotePath);
        if (!IsSafeRemotePath(remotePath)) {
            if (outError)
                *outError = "Remote path must be an absolute Linux path with letters, digits, '/', '.', '_' or '-'.";
            return false;
        }

        SshSession ssh;
        if (!ssh.Connect(settings, 15000, outError))
            return false;

        std::ostringstream check;
        check
            << "set -e; "
            << "ROOT=" << ShellQuote(remotePath) << "; "
            << "test -d \"$ROOT\" || { echo 'missing remote directory'; exit 10; }; "
            << "test -f \"$ROOT/server.js\" || { echo 'missing server.js'; exit 11; }; "
            << "test -f \"$ROOT/package.json\" || { echo 'missing package.json'; exit 12; }; "
            << "test -f \"$ROOT/public/index.html\" || { echo 'missing public/index.html'; exit 13; }; "
            << "command -v node >/dev/null 2>&1 || { echo 'node is not installed'; exit 14; }; "
            << "command -v npm >/dev/null 2>&1 || { echo 'npm is not installed'; exit 15; }; "
            << "test -d \"$ROOT/node_modules/express\" || { echo 'missing express dependency'; exit 16; }; "
            << "test -d \"$ROOT/node_modules/ws\" || { echo 'missing ws dependency'; exit 17; }; "
            << "systemctl is-active --quiet kevqdma-webradar || { systemctl --no-pager --lines=20 status kevqdma-webradar 2>&1; exit 18; }; "
            << "echo READY";

        std::string output;
        if (!RunSshCommand(ssh, check.str(), &output, outError, 60000))
            return false;

        if (output.find("READY") == std::string::npos) {
            if (outError)
                *outError = "Server readiness check returned unexpected response.";
            return false;
        }

        std::string httpError;
        if (!TestHttpStatus(settings, &httpError)) {
            if (outError)
                *outError = "Server files are ready, but HTTP check failed: " + httpError;
            return false;
        }
        return true;
    }

    bool ExportDeployPackage(const Settings& settings, std::filesystem::path* outPath, std::string* outError)
    {
        (void)settings;
        const auto deployRoot = app::paths::GetRuntimeDataDirectory() / "webradar_web_deploy";
        const auto publicDir = deployRoot / "public";
        const auto assetDir = app::paths::ResolveWebRadarAssetDirectory();

        std::error_code ec;
        std::filesystem::remove_all(deployRoot, ec);
        ec.clear();
        std::filesystem::create_directories(publicDir, ec);
        if (ec) {
            if (outError)
                *outError = "Cannot create deploy directory.";
            return false;
        }

        std::string exportError;
        if (!ExportEmbeddedAssets(publicDir, &exportError)) {
            if (assetDir.empty() || !std::filesystem::exists(assetDir)) {
                if (outError)
                    *outError = exportError.empty() ? "WEBRadar assets directory not found." : exportError;
                return false;
            }
            std::filesystem::copy(assetDir, publicDir,
                std::filesystem::copy_options::recursive |
                std::filesystem::copy_options::overwrite_existing,
                ec);
            if (ec) {
                if (outError)
                    *outError = exportError.empty() ? "Cannot copy WEBRadar assets." : exportError;
                return false;
            }
        }

        {
            std::ofstream serverFile(deployRoot / "server.js", std::ios::binary | std::ios::trunc);
            if (!serverFile.is_open()) {
                if (outError)
                    *outError = "Cannot write server.js.";
                return false;
            }
            serverFile << RemoteServerJs();
        }
        {
            std::ofstream packageFile(deployRoot / "package.json", std::ios::binary | std::ios::trunc);
            if (!packageFile.is_open()) {
                if (outError)
                    *outError = "Cannot write package.json.";
                return false;
            }
            packageFile << PackageJson();
        }

        if (outPath)
            *outPath = deployRoot;
        return true;
    }

    bool DeployToServer(const Settings& settings, std::filesystem::path* outPath, std::string* outError, DeployProgressCallback progress)
    {
        if (Trim(settings.host).empty()) {
            if (outError)
                *outError = "Host is empty.";
            return false;
        }
        if (Trim(settings.login).empty()) {
            if (outError)
                *outError = "SSH login is empty.";
            return false;
        }

        const std::string remotePath = NormalizeRemotePath(settings.remotePath);
        if (!IsSafeRemotePath(remotePath)) {
            if (outError)
                *outError = "Remote path must be an absolute Linux path with letters, digits, '/', '.', '_' or '-'.";
            return false;
        }

        std::filesystem::path deployRoot;
        ReportDeployProgress(progress, "Deploy: preparing WebRadar package...");
        if (!ExportDeployPackage(settings, &deployRoot, outError))
            return false;
        struct ScopedDeployCleanup {
            std::filesystem::path path;
            ~ScopedDeployCleanup()
            {
                if (path.empty())
                    return;
                std::error_code ec;
                std::filesystem::remove_all(path, ec);
            }
        } cleanup{ deployRoot };

        SshSession ssh;
        ReportDeployProgress(progress, "Deploy: connecting to VDS by SSH...");
        if (!ssh.Connect(settings, 20000, outError))
            return false;

        const std::string stagingPath = remotePath + "/.deploy_tmp";
        ReportDeployProgress(progress, "Deploy: preparing remote directory...");
        std::ostringstream prepareRemote;
        prepareRemote
            << "set -e; "
            << "ROOT=" << ShellQuote(remotePath) << "; "
            << "TMP=" << ShellQuote(stagingPath) << "; "
            << "mkdir -p \"$ROOT\"; "
            << "rm -rf \"$TMP\"; "
            << "mkdir -p \"$TMP\"";
        if (!RunSshCommand(ssh, prepareRemote.str(), nullptr, outError, 30000))
            return false;

        ReportDeployProgress(progress, "Deploy: uploading WebRadar files...");
        if (!SftpUploadDirectory(ssh, deployRoot, stagingPath, outError))
            return false;

        ReportDeployProgress(progress, "Deploy: replacing WebRadar files...");
        std::ostringstream replaceRemote;
        replaceRemote
            << "set -e; "
            << "ROOT=" << ShellQuote(remotePath) << "; "
            << "TMP=" << ShellQuote(stagingPath) << "; "
            << "BACKUP=\"$ROOT/.deploy_backup\"; "
            << "rollback() { "
            << "rm -rf \"$ROOT/public\" \"$ROOT/server.js\" \"$ROOT/package.json\"; "
            << "if [ -d \"$BACKUP/public\" ]; then mv \"$BACKUP/public\" \"$ROOT/public\"; fi; "
            << "if [ -f \"$BACKUP/server.js\" ]; then mv \"$BACKUP/server.js\" \"$ROOT/server.js\"; fi; "
            << "if [ -f \"$BACKUP/package.json\" ]; then mv \"$BACKUP/package.json\" \"$ROOT/package.json\"; fi; "
            << "rm -rf \"$TMP\" \"$BACKUP\"; "
            << "}; "
            << "test -f \"$TMP/server.js\"; "
            << "test -f \"$TMP/package.json\"; "
            << "test -f \"$TMP/public/index.html\"; "
            << "rm -rf \"$BACKUP\"; "
            << "mkdir -p \"$BACKUP\"; "
            << "if [ -d \"$ROOT/public\" ]; then mv \"$ROOT/public\" \"$BACKUP/public\"; fi; "
            << "if [ -f \"$ROOT/server.js\" ]; then mv \"$ROOT/server.js\" \"$BACKUP/server.js\"; fi; "
            << "if [ -f \"$ROOT/package.json\" ]; then mv \"$ROOT/package.json\" \"$BACKUP/package.json\"; fi; "
            << "trap 'rollback' ERR; "
            << "rm -f \"$ROOT/README_DEPLOY.txt\"; "
            << "mv \"$TMP/public\" \"$ROOT/public\"; "
            << "mv \"$TMP/server.js\" \"$ROOT/server.js\"; "
            << "mv \"$TMP/package.json\" \"$ROOT/package.json\"; "
            << "trap - ERR; "
            << "rm -rf \"$BACKUP\" \"$TMP\"";
        if (!RunSshCommand(ssh, replaceRemote.str(), nullptr, outError, 30000))
            return false;

        const int webPort = std::clamp(settings.webPort, 1, 65535);
        std::vector<std::string> serviceLines = {
            "[Unit]",
            "Description=KevqDMA WebRadar",
            "After=network-online.target",
            "Wants=network-online.target",
            "",
            "[Service]",
            "Type=simple",
            "WorkingDirectory=" + remotePath,
            "Environment=PORT=" + std::to_string(webPort),
            "Environment=BROADCAST_HZ=60",
            "ExecStart=/usr/bin/node server.js",
            "Restart=always",
            "RestartSec=2",
            "DynamicUser=yes",
            "ProtectSystem=strict",
            "ProtectHome=yes",
            "PrivateTmp=yes",
            "PrivateDevices=yes",
            "NoNewPrivileges=yes",
            "ProtectKernelTunables=yes",
            "ProtectKernelModules=yes",
            "ProtectControlGroups=yes",
            "RestrictNamespaces=yes",
            "RestrictRealtime=yes",
            "RestrictSUIDSGID=yes",
            "LockPersonality=yes",
            "MemoryDenyWriteExecute=yes",
            "SystemCallArchitectures=native",
            "ReadWritePaths=" + remotePath,
            "CapabilityBoundingSet=",
            "AmbientCapabilities=",
            "",
            "[Install]",
            "WantedBy=multi-user.target"
        };

        std::ostringstream installSystem;
        installSystem
            << "set -e; "
            << "if ! command -v node >/dev/null 2>&1 || ! command -v npm >/dev/null 2>&1; then "
            << "export DEBIAN_FRONTEND=noninteractive; "
            << "apt-get update -y; "
            << "apt-get install -y nodejs npm; "
            << "fi; "
            << "node --version; npm --version";

        ReportDeployProgress(progress, "Deploy: installing Node.js/npm if missing...");
        libssh2_session_set_timeout(ssh.Get(), 300000);
        if (!RunSshCommand(ssh, installSystem.str(), nullptr, outError, 300000))
            return false;

        ReportDeployProgress(progress, "Deploy: installing WebRadar npm dependencies...");
        std::ostringstream installNpm;
        installNpm
            << "set -e; "
            << "cd " << ShellQuote(remotePath) << "; "
            << "npm install --omit=dev --no-audit --no-fund --loglevel=error";
        if (!RunSshCommand(ssh, installNpm.str(), nullptr, outError, 180000))
            return false;

        std::ostringstream installService;
        installService
            << "set -e; "
            << "printf '%s\\n'";
        for (const std::string& line : serviceLines)
            installService << " " << ShellQuote(line);
        installService
            << " > /etc/systemd/system/kevqdma-webradar.service; "
            << "systemctl daemon-reload; "
            << "systemctl enable kevqdma-webradar >/dev/null; "
            << "systemctl restart kevqdma-webradar; "
            << "if command -v ufw >/dev/null 2>&1; then ufw allow " << webPort << "/tcp >/dev/null 2>&1 || true; fi; "
            << "sleep 1; "
            << "systemctl is-active --quiet kevqdma-webradar; "
            << "systemctl --no-pager --lines=20 status kevqdma-webradar >/dev/null";

        ReportDeployProgress(progress, "Deploy: installing and starting systemd service...");
        libssh2_session_set_timeout(ssh.Get(), 60000);
        if (!RunSshCommand(ssh, installService.str(), nullptr, outError, 60000))
            return false;

        ReportDeployProgress(progress, "Deploy: checking HTTP endpoint...");
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::string httpError;
        bool httpOk = false;
        for (int attempt = 0; attempt < 10; ++attempt) {
            if (TestHttpStatus(settings, &httpError)) {
                httpOk = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(700));
        }
        if (!httpOk) {
            std::string serviceLog;
            std::string serviceError;
            RunSshCommand(ssh, "systemctl --no-pager --lines=40 status kevqdma-webradar 2>&1 || true", &serviceLog, &serviceError, 30000);
            if (outError) {
                const std::string details = TrimProcessOutput(serviceLog);
                *outError = "Deploy completed, but HTTP check failed: " + httpError + (details.empty() ? "" : " | " + details);
            }
            return false;
        }

        if (outPath)
            outPath->clear();
        return true;
    }
}
