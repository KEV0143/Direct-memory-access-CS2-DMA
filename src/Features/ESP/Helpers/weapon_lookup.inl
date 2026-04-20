static bool IsKnifeItemId(uint16_t id)
{
    return app::state::IsKnifeItemId(id);
}

namespace {
    enum class WeaponSlotKind : uint8_t {
        Unknown = 0,
        Knife,
        Sidearm,
        Primary,
        Utility,
        Objective,
        Gear,
    };

    struct WeaponLookupEntry {
        uint16_t id;
        const char* name;
        const char* visualKey;
        const char* iconGlyph;
        const char* iconFallback;
        WeaponSlotKind slotKind;
        int maxClip;
    };

    
    
    static constexpr WeaponLookupEntry kKnifeLookupEntry = {
        42, "Knife", "knife", "]", "KN", WeaponSlotKind::Knife, 30
    };

    static constexpr WeaponLookupEntry kWeaponLookup[] = {
        { 1,  "Deagle",     "deagle",          "A", "DE",  WeaponSlotKind::Sidearm,  7 },
        { 2,  "Elite",      "elite",           "B", "EL",  WeaponSlotKind::Sidearm, 30 },
        { 3,  "Five-SeveN", "fiveseven",       "C", "57",  WeaponSlotKind::Sidearm, 20 },
        { 4,  "Glock",      "glock",           "D", "GL",  WeaponSlotKind::Sidearm, 20 },
        { 7,  "AK-47",      "ak47",            "W", "AK",  WeaponSlotKind::Primary, 30 },
        { 8,  "AUG",        "aug",             "U", "AUG", WeaponSlotKind::Primary, 30 },
        { 9,  "AWP",        "awp",             "Z", "AWP", WeaponSlotKind::Primary, 10 },
        { 10, "FAMAS",      "famas",           "R", "FAM", WeaponSlotKind::Primary, 25 },
        { 11, "G3SG1",      "g3sg1",           "X", "G3",  WeaponSlotKind::Primary, 20 },
        { 13, "Galil",      "galilar",         "Q", "GAL", WeaponSlotKind::Primary, 35 },
        { 14, "M249",       "m249",            "g", "249", WeaponSlotKind::Primary, 100 },
        { 16, "M4A4",       "m4a1",            "S", "M4",  WeaponSlotKind::Primary, 30 },
        { 17, "MAC-10",     "mac10",           "K", "MAC", WeaponSlotKind::Primary, 30 },
        { 19, "P90",        "p90",             "P", "P90", WeaponSlotKind::Primary, 50 },
        { 23, "MP5-SD",     "mp5sd",           "x", "MP5", WeaponSlotKind::Primary, 30 },
        { 24, "UMP-45",     "ump45",           "L", "UMP", WeaponSlotKind::Primary, 25 },
        { 25, "XM1014",     "xm1014",          "b", "XM",  WeaponSlotKind::Primary,  7 },
        { 26, "PP-Bizon",   "bizon",           "M", "BZ",  WeaponSlotKind::Primary, 64 },
        { 27, "MAG-7",      "mag7",            "d", "M7",  WeaponSlotKind::Primary,  5 },
        { 28, "Negev",      "negev",           "f", "NEG", WeaponSlotKind::Primary, 150 },
        { 29, "Sawed-Off",  "sawedoff",        "c", "SO",  WeaponSlotKind::Primary,  7 },
        { 30, "Tec-9",      "tec9",            "I", "T9",  WeaponSlotKind::Sidearm, 18 },
        { 31, "Zeus",       "taser",           "m", "ZR",  WeaponSlotKind::Gear,     1 },
        { 32, "P2000",      "p2000",           "E", "P2K", WeaponSlotKind::Sidearm, 13 },
        { 33, "MP7",        "mp7",             "N", "MP7", WeaponSlotKind::Primary, 30 },
        { 34, "MP9",        "mp9",             "O", "MP9", WeaponSlotKind::Primary, 30 },
        { 35, "Nova",       "nova",            "e", "NV",  WeaponSlotKind::Primary,  8 },
        { 36, "P250",       "p250",            "F", "P25", WeaponSlotKind::Sidearm, 13 },
        { 38, "SCAR-20",    "scar20",          "Y", "SC",  WeaponSlotKind::Primary, 20 },
        { 39, "SG553",      "sg556",           "V", "SG",  WeaponSlotKind::Primary, 30 },
        { 40, "SSG08",      "ssg08",           "a", "SSG", WeaponSlotKind::Primary, 10 },
        { 43, "Flash",      "flashbang",       "i", "FB",  WeaponSlotKind::Utility, 30 },
        { 44, "HE",         "hegrenade",       "j", "HE",  WeaponSlotKind::Utility, 30 },
        { 45, "Smoke",      "smokegrenade",    "k", "SM",  WeaponSlotKind::Utility, 30 },
        { 46, "Molotov",    "molotov",         "l", "ML",  WeaponSlotKind::Utility, 30 },
        { 47, "Decoy",      "decoy",        nullptr, "DC", WeaponSlotKind::Utility, 30 },
        { 48, "Incendiary", "incgrenade",      "n", "IN",  WeaponSlotKind::Utility, 30 },
        { 49, "C4",         "c4",              "o", "C4",  WeaponSlotKind::Objective, 30 },
        { 57, "Healthshot", "health",       nullptr, "HP", WeaponSlotKind::Gear, 30 },
        { 60, "M4A1-S",     "m4a1_silencer",   "T", "A1S", WeaponSlotKind::Primary, 25 },
        { 61, "USP-S",      "usp_silencer",    "G", "USP", WeaponSlotKind::Sidearm, 12 },
        { 63, "CZ75",       "cz75a",           "h", "CZ",  WeaponSlotKind::Sidearm, 12 },
        { 64, "R8",         "revolver",        "J", "R8",  WeaponSlotKind::Sidearm,  8 },
    };

    static const WeaponLookupEntry* FindWeaponLookupEntry(uint16_t id)
    {
        for (const WeaponLookupEntry& entry : kWeaponLookup) {
            if (entry.id == id)
                return &entry;
        }
        if (IsKnifeItemId(id))
            return &kKnifeLookupEntry;
        return nullptr;
    }
}

static const char* WeaponNameFromItemId(uint16_t id)
{
    if (const WeaponLookupEntry* entry = FindWeaponLookupEntry(id))
        return entry->name;
    return nullptr;
}

static const char* WeaponVisualKeyFromItemId(uint16_t id)
{
    if (const WeaponLookupEntry* entry = FindWeaponLookupEntry(id))
        return entry->visualKey;
    return nullptr;
}

static const char* WeaponIconFromItemId(uint16_t id)
{
    if (const WeaponLookupEntry* entry = FindWeaponLookupEntry(id))
        return entry->iconGlyph;
    return nullptr;
}

static const char* WeaponIconFallbackTokenFromItemId(uint16_t id)
{
    if (const WeaponLookupEntry* entry = FindWeaponLookupEntry(id))
        return entry->iconFallback;
    return nullptr;
}

static bool IsPrimaryWeaponItemId(uint16_t id)
{
    if (const WeaponLookupEntry* entry = FindWeaponLookupEntry(id))
        return entry->slotKind == WeaponSlotKind::Primary;
    return false;
}

static int WeaponMaxClipFromItemId(uint16_t id)
{
    if (const WeaponLookupEntry* entry = FindWeaponLookupEntry(id))
        return entry->maxClip;
    return 30;
}
