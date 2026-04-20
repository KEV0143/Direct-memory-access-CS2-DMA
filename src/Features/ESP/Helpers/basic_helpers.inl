static float Clamp01(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static ImU32 ColorToImU32(const float c[4]) {
    return IM_COL32(
        static_cast<int>(std::clamp(c[0], 0.0f, 1.0f) * 255.0f),
        static_cast<int>(std::clamp(c[1], 0.0f, 1.0f) * 255.0f),
        static_cast<int>(std::clamp(c[2], 0.0f, 1.0f) * 255.0f),
        static_cast<int>(std::clamp(c[3], 0.0f, 1.0f) * 255.0f));
}

static bool IsMaskBitSet(uint64_t mask, int bit)
{
    if (bit < 0 || bit >= 64)
        return false;
    return ((mask >> bit) & 1ULL) != 0ULL;
}

static float NormalizeYawDeltaRad(float value)
{
    constexpr float tau = 2.0f * std::numbers::pi_v<float>;
    return std::remainder(value, tau);
}

static bool isFiniteVec(const Vector3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

static bool isValidWorldPos(const Vector3& v)
{
    constexpr float kMaxWorldXY = 32768.0f;
    constexpr float kMaxWorldZ = 16384.0f;
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) &&
           std::fabs(v.x) <= kMaxWorldXY &&
           std::fabs(v.y) <= kMaxWorldXY &&
           std::fabs(v.z) <= kMaxWorldZ &&
           (std::fabs(v.x) + std::fabs(v.y) + std::fabs(v.z) > 1.0f);
}

static uint64_t TickNowUs()
{
    static const auto s_epoch = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(now - s_epoch).count());
}

static uint64_t TickNowMs()
{
    return TickNowUs() / 1000u;
}

static ImFont* GetEspNameFont()
{
    if (g::fontSegoeBold)
        return g::fontSegoeBold;

    if (g::fontDefault)
        return g::fontDefault;
    return ImGui::GetFont();
}

static const char* WorldMarkerName(WorldMarkerType type, uint16_t weaponId)
{
    switch (type) {
    case WorldMarkerType::DroppedWeapon: return WeaponNameFromItemId(weaponId);
    case WorldMarkerType::Smoke: return "Smoke";
    case WorldMarkerType::Inferno: return "Fire";
    case WorldMarkerType::Decoy: return "Decoy";
    case WorldMarkerType::Explosive: return "Grenade";
    case WorldMarkerType::SmokeProjectile: return "Smoke Projectile";
    case WorldMarkerType::MolotovProjectile: return (weaponId == 48) ? "Incendiary Projectile" : "Molotov Projectile";
    case WorldMarkerType::DecoyProjectile: return "Decoy Projectile";
    default:
        break;
    }
    return nullptr;
}
