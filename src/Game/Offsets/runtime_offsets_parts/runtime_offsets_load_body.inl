    try
    {
        const Values defaults = BuildDefaults();
        Values loaded = defaults;
        std::vector<std::string> missingKeys;
        std::vector<std::string> invalidKeys;

        const std::filesystem::path jsonPath = FindOffsetsJsonPath(true);
        if (jsonPath.empty())
        {
            g_values = defaults;
            if (message)
                *message = "Offsets load failed: unable to resolve offsets.json path. Using built-in defaults.";
            return false;
        }

        bool createdFile = false;
        bool migratedLegacyIni = false;
        bool normalizedJson = false;
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
                LoadValuesFromJson(root, defaults, loaded, &missingKeys, &invalidKeys);
                normalizedJson = !missingKeys.empty() || !invalidKeys.empty();
            }
            else
            {
                invalidJson = true;
                normalizedJson = true;
            }
        }
        else
        {
            const auto legacyIniPath = FindLegacyOffsetsPath("offsets.ini");
            if (!legacyIniPath.empty())
            {
                LoadValuesFromLegacyIni(ParseLegacyIniFile(legacyIniPath), defaults, loaded, &missingKeys, &invalidKeys);
                migratedLegacyIni = true;
                normalizedJson = !missingKeys.empty() || !invalidKeys.empty();
            }
            else
            {
                createdFile = true;
            }
        }

        g_values = loaded;

        const auto zeroFields = ValidateLoadedValues(loaded);

        if (createdFile || migratedLegacyIni || normalizedJson || invalidJson || !jsonExists || legacyStateExists)
        {
            if (!WriteOffsetsJson(jsonPath, loaded, &storedState))
            {
                g_values = defaults;
                if (message)
                    *message = "Offsets load failed: cannot create " + jsonPath.string() + ". Using built-in defaults.";
                return false;
            }
        }

        CleanupObsoleteOffsetsIniFiles();

        std::ostringstream oss;
        const auto displayPath = jsonPath.filename().string();
        if (createdFile)
            oss << "Offsets file created: " << displayPath << ". ";
        else if (migratedLegacyIni)
            oss << "Legacy offsets.ini migrated to: " << displayPath << ". ";
        else
            oss << "Offsets loaded from: " << displayPath << ". ";

        if (invalidJson)
            oss << "Invalid JSON was replaced with defaults. ";
        if (!invalidKeys.empty())
            oss << "Defaults used for invalid values: " << JoinKeys(invalidKeys) << ". ";
        if (!missingKeys.empty())
            oss << "Defaults used for missing keys: " << JoinKeys(missingKeys) << ". ";
        if (!zeroFields.empty())
            oss << "WARNING: zero/negative offsets detected: " << JoinKeys(zeroFields) << ".";

        if (message)
            *message = oss.str();

        return true;
    }
    catch (...)
    {
        g_values = BuildDefaults();
        if (message)
            *message = "Offsets load failed: runtime exception. Using built-in defaults.";
        return false;
    }
