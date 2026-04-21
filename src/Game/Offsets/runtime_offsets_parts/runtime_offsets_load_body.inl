    try
    {
        Values loaded = {};
        std::vector<std::string> missingKeys;
        std::vector<std::string> invalidKeys;

        const std::filesystem::path jsonPath = FindOffsetsJsonPath(true);
        if (jsonPath.empty())
        {
            g_values = {};
            if (message)
                *message = "Offsets load failed: unable to resolve offsets.json path.";
            return false;
        }

        bool migratedLegacyIni = false;
        bool invalidJson = false;
        std::error_code ec;
        const bool jsonExists = std::filesystem::exists(jsonPath, ec);
        ec.clear();
        const bool legacyStateExists = std::filesystem::exists(GetOffsetsStatePath(), ec);
        const OffsetState storedState = ReadOffsetState(jsonPath, GetOffsetsStatePath());

        if (jsonExists)
        {
            json root;
            if (TryParseOffsetsJsonFile(jsonPath, root))
            {
                LoadValuesFromJson(root, loaded, &missingKeys, &invalidKeys);
            }
            else
            {
                invalidJson = true;
            }
        }
        else
        {
            const auto legacyIniPath = FindLegacyOffsetsPath("offsets.ini");
            if (!legacyIniPath.empty())
            {
                LoadValuesFromLegacyIni(ParseLegacyIniFile(legacyIniPath), loaded, &missingKeys, &invalidKeys);
                migratedLegacyIni = true;
            }
            else
            {
                g_values = {};
                if (message)
                    *message = "Offsets load failed: offsets.json is missing.";
                return false;
            }
        }

        const auto requiredZeroFields = ValidateLoadedValues(loaded, true);
        if (invalidJson || !requiredZeroFields.empty())
        {
            g_values = {};
            if (message)
            {
                std::ostringstream oss;
                oss << "Offsets load failed: ";
                if (invalidJson)
                    oss << "invalid JSON in " << jsonPath.filename().string() << ".";
                else
                    oss << "required offsets are missing or invalid: " << JoinKeys(requiredZeroFields) << ".";
                if (!missingKeys.empty())
                    oss << " Missing keys: " << JoinKeys(missingKeys) << ".";
                if (!invalidKeys.empty())
                    oss << " Invalid keys: " << JoinKeys(invalidKeys) << ".";
                *message = oss.str();
            }
            return false;
        }

        g_values = loaded;

        if (migratedLegacyIni || legacyStateExists)
        {
            if (!WriteOffsetsJson(jsonPath, loaded, &storedState))
            {
                g_values = {};
                if (message)
                    *message = "Offsets load failed: cannot write " + jsonPath.string() + ".";
                return false;
            }
        }

        CleanupObsoleteOffsetsIniFiles();

        std::ostringstream oss;
        const auto displayPath = jsonPath.filename().string();
        if (migratedLegacyIni)
            oss << "Legacy offsets.ini migrated to: " << displayPath << ". ";
        else
            oss << "Offsets loaded from: " << displayPath << ". ";

        const auto optionalZeroFields = ValidateLoadedValues(loaded, false);
        if (!optionalZeroFields.empty() && optionalZeroFields.size() > requiredZeroFields.size())
            oss << "Optional zero offsets kept as-is: " << JoinKeys(optionalZeroFields) << ". ";
        if (!invalidKeys.empty())
            oss << "Optional invalid keys kept as zero: " << JoinKeys(invalidKeys) << ". ";
        if (!missingKeys.empty())
            oss << "Optional missing keys kept as zero: " << JoinKeys(missingKeys) << ". ";

        if (message)
            *message = oss.str();

        return true;
    }
    catch (...)
    {
        g_values = {};
        if (message)
            *message = "Offsets load failed: runtime exception.";
        return false;
    }
