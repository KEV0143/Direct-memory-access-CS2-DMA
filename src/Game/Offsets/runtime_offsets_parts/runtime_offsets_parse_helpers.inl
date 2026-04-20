    std::string Trim(const std::string& text)
    {
        size_t start = 0;
        while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])))
            ++start;

        size_t end = text.size();
        while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])))
            --end;

        return text.substr(start, end - start);
    }

    std::string ToHex(std::ptrdiff_t value)
    {
        std::ostringstream oss;
        oss << "0x" << std::uppercase << std::hex << static_cast<std::uint64_t>(value);
        return oss.str();
    }

    bool TryParseOffset(std::string text, std::ptrdiff_t& out)
    {
        text = Trim(text);
        if (text.empty())
            return false;

        int base = 10;
        if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
            base = 16;

        try
        {
            out = static_cast<std::ptrdiff_t>(std::stoll(text, nullptr, base));
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    int ParsePatchBuildNumber(std::string_view patchVersion)
    {
        std::string digits;
        digits.reserve(patchVersion.size());
        for (const char ch : patchVersion)
        {
            if (ch >= '0' && ch <= '9')
                digits.push_back(ch);
        }

        if (digits.empty())
            return 0;

        try
        {
            return std::stoi(digits);
        }
        catch (...)
        {
            return 0;
        }
    }

    std::optional<std::chrono::sys_seconds> ParseCanonicalUtcTimestamp(std::string_view canonical)
    {
        if (canonical.size() < 14)
            return std::nullopt;

        auto parsePart = [&](size_t start, size_t len) -> int {
            int value = 0;
            for (size_t i = 0; i < len; ++i)
            {
                const char ch = canonical[start + i];
                if (ch < '0' || ch > '9')
                    return -1;
                value = (value * 10) + (ch - '0');
            }
            return value;
        };

        const int year = parsePart(0, 4);
        const int month = parsePart(4, 2);
        const int day = parsePart(6, 2);
        const int hour = parsePart(8, 2);
        const int minute = parsePart(10, 2);
        const int second = parsePart(12, 2);
        if (year < 1970 || month <= 0 || day <= 0 || hour < 0 || minute < 0 || second < 0)
            return std::nullopt;

        const std::chrono::year_month_day ymd {
            std::chrono::year { year },
            std::chrono::month { static_cast<unsigned int>(month) },
            std::chrono::day { static_cast<unsigned int>(day) }
        };
        if (!ymd.ok())
            return std::nullopt;

        return std::chrono::sys_days { ymd } +
               std::chrono::hours { hour } +
               std::chrono::minutes { minute } +
               std::chrono::seconds { second };
    }

    std::optional<std::chrono::sys_seconds> ParseSteamVersionTimestamp(
        std::string_view versionDate,
        std::string_view versionTime)
    {
        static constexpr std::pair<std::string_view, unsigned int> kMonths[] = {
            { "Jan", 1 }, { "Feb", 2 }, { "Mar", 3 }, { "Apr", 4 },
            { "May", 5 }, { "Jun", 6 }, { "Jul", 7 }, { "Aug", 8 },
            { "Sep", 9 }, { "Oct", 10 }, { "Nov", 11 }, { "Dec", 12 }
        };

        std::istringstream dateStream { std::string(versionDate) };
        std::string monthToken;
        int day = 0;
        int year = 0;
        if (!(dateStream >> monthToken >> day >> year))
            return std::nullopt;

        unsigned int month = 0;
        for (const auto& [name, value] : kMonths)
        {
            if (monthToken == name)
            {
                month = value;
                break;
            }
        }
        if (month == 0)
            return std::nullopt;

        int hour = 0;
        int minute = 0;
        int second = 0;
        char colon1 = '\0';
        char colon2 = '\0';
        std::istringstream timeStream { std::string(versionTime) };
        if (!(timeStream >> hour >> colon1 >> minute >> colon2 >> second) ||
            colon1 != ':' || colon2 != ':')
            return std::nullopt;

        const std::chrono::year_month_day ymd {
            std::chrono::year { year },
            std::chrono::month { month },
            std::chrono::day { static_cast<unsigned int>(day) }
        };
        if (!ymd.ok())
            return std::nullopt;

        return std::chrono::sys_days { ymd } +
               std::chrono::hours { hour } +
               std::chrono::minutes { minute } +
               std::chrono::seconds { second };
    }

    bool SourceTimestampDefinitelyPredatesCurrentPatch(
        std::string_view selectedSourceTimestamp,
        const SteamInfSnapshot& currentPatch)
    {
        const auto sourceTime = ParseCanonicalUtcTimestamp(selectedSourceTimestamp);
        const auto patchTime = ParseSteamVersionTimestamp(currentPatch.versionDate, currentPatch.versionTime);
        if (!sourceTime || !patchTime)
            return false;

        return (*sourceTime + std::chrono::hours(18)) < *patchTime;
    }

    bool PatchInfoEquals(const runtime_offsets::PatchInfo& a, const runtime_offsets::PatchInfo& b)
    {
        if (a.patchVersion.empty() || b.patchVersion.empty())
            return false;

        return a.patchVersion == b.patchVersion &&
               a.clientVersion == b.clientVersion &&
               a.sourceRevision == b.sourceRevision &&
               a.lastFileSha == b.lastFileSha;
    }

    std::string JsonStringOrEmpty(const json& root, const char* key)
    {
        const auto it = root.find(key);
        if (it == root.end() || !it->is_string())
            return {};
        return it->get<std::string>();
    }

    int JsonIntOrZero(const json& root, const char* key)
    {
        const auto it = root.find(key);
        if (it == root.end())
            return 0;
        if (it->is_number_integer())
            return it->get<int>();
        if (it->is_number_unsigned())
            return static_cast<int>(it->get<unsigned int>());
        return 0;
    }
