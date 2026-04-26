#include "Features/Radar/map_registry.h"

#include "Features/WebRadar/embedded_assets.h"
#include "app/Config/project_paths.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <utility>

#include <json/json.hpp>

namespace
{
    std::vector<radar::MapDefinition> BuildFallbackMaps()
    {
        return {
            { "de_mirage", "Mirage", -3230.0, 1890.0, -3407.0, 1713.0, -3230.0, 1713.0, 5.00, false, "/data/de_mirage/radar.webp", "/data/de_mirage/radar.webp" },
            { "de_inferno", "Inferno", -2087.0, 2930.6, -1147.6, 3870.0, -2087.0, 3870.0, 4.90, false, "/data/de_inferno/radar.webp", "/data/de_inferno/radar.webp" },
            { "de_dust2", "Dust II", -2476.0, 2029.6, -1266.6, 3239.0, -2476.0, 3239.0, 4.40, false, "/data/de_dust2/radar.webp", "/data/de_dust2/radar.webp" },
            { "de_nuke", "Nuke", -3453.0, 3715.0, -4281.0, 2887.0, -3453.0, 2887.0, 7.00, false, "/data/de_nuke/radar.webp", "/data/de_nuke/radar.webp" },
            { "de_overpass", "Overpass", -4831.0, 493.8, -3543.8, 1781.0, -4831.0, 1781.0, 5.20, false, "/data/de_overpass/radar.webp", "/data/de_overpass/radar.webp" },
            { "de_train", "Train", -2308.0, 1872.046848, -2102.046848, 2078.0, -2308.0, 2078.0, 4.082077, false, "/data/de_train/radar.webp", "/data/de_train/radar.webp" },
            { "ar_baggage", "Baggage", -1838.0, 2360.4, -2340.4, 1858.0, -1838.0, 1858.0, 4.10, false, "/data/ar_baggage/radar.webp", "/data/ar_baggage/radar.webp" },
            { "ar_shoots", "Shoots", -2953.0, 2167.0, -2956.0, 2164.0, -2953.0, 2164.0, 5.00, false, "/data/ar_shoots/radar.webp", "/data/ar_shoots/radar.webp" },
            { "ar_shoots_night", "Shoots (Night)", -2953.0, 2167.0, -2956.0, 2164.0, -2953.0, 2164.0, 5.00, false, "/data/ar_shoots_night/radar.webp", "/data/ar_shoots_night/radar.webp" },
            { "de_ancient", "Ancient", -2953.0, 2167.0, -2956.0, 2164.0, -2953.0, 2164.0, 5.00, false, "/data/de_ancient/radar.webp", "/data/de_ancient/radar.webp" },
            { "de_ancient_night", "Ancient (Night)", -2953.0, 2167.0, -2956.0, 2164.0, -2953.0, 2164.0, 5.00, false, "/data/de_ancient_night/radar.webp", "/data/de_ancient_night/radar.webp" },
            { "de_anubis", "Anubis", -2796.0, 2549.28, -2017.28, 3328.0, -2796.0, 3328.0, 5.22, false, "/data/de_anubis/radar.webp", "/data/de_anubis/radar.webp" },
            { "de_vertigo", "Vertigo", -3168.0, 928.0, -2334.0, 1762.0, -3168.0, 1762.0, 4.00, false, "/data/de_vertigo/radar.webp", "/data/de_vertigo/radar.webp" },
            { "cs_office", "Office", -1838.0, 2360.4, -2340.4, 1858.0, -1838.0, 1858.0, 4.10, false, "/data/cs_office/radar.webp", "/data/cs_office/radar.webp" },
            { "cs_italy", "Italy", -2647.0, 2063.4, -2118.4, 2592.0, -2647.0, 2592.0, 4.60, false, "/data/cs_italy/radar.webp", "/data/cs_italy/radar.webp" },
            { "aim_custom", "Aim / Workshop (Dynamic)", -1024.0, 1024.0, -1024.0, 1024.0, 0.0, 0.0, 1.00, true, "/data/aim_custom/radar.webp", "/data/aim_custom/radar.webp" },
        };
    }

    bool AppendMapsFromJson(const nlohmann::json& root, std::vector<radar::MapDefinition>& maps)
    {
        if (root.is_discarded() || !root.contains("maps") || !root["maps"].is_array())
            return false;

        for (const auto& item : root["maps"]) {
            if (!item.is_object() || !item.contains("name") || !item["name"].is_string())
                continue;

            radar::MapDefinition map = {};
            map.name = item["name"].get<std::string>();
            map.displayName = item.value("display_name", map.name);
            map.dynamic = item.value("dynamic", false);

            if (item.contains("origin") && item["origin"].is_object()) {
                const auto& origin = item["origin"];
                if (origin.contains("x") && origin["x"].is_number())
                    map.originX = origin["x"].get<double>();
                if (origin.contains("y") && origin["y"].is_number())
                    map.originY = origin["y"].get<double>();
            }

            if (item.contains("bounds") && item["bounds"].is_object()) {
                const auto& bounds = item["bounds"];
                if (bounds.contains("min_x") && bounds["min_x"].is_number())
                    map.minX = bounds["min_x"].get<double>();
                if (bounds.contains("max_x") && bounds["max_x"].is_number())
                    map.maxX = bounds["max_x"].get<double>();
                if (bounds.contains("min_y") && bounds["min_y"].is_number())
                    map.minY = bounds["min_y"].get<double>();
                if (bounds.contains("max_y") && bounds["max_y"].is_number())
                    map.maxY = bounds["max_y"].get<double>();
            }

            if (item.contains("scale") && item["scale"].is_number())
                map.scale = item["scale"].get<double>();

            if (item.contains("images") && item["images"].is_object()) {
                const auto& images = item["images"];
                if (images.contains("radar") && images["radar"].is_string())
                    map.radarImage = images["radar"].get<std::string>();
                if (images.contains("background") && images["background"].is_string())
                    map.backgroundImage = images["background"].get<std::string>();
            }

            if (map.name.empty() ||
                !std::isfinite(map.originX) ||
                !std::isfinite(map.originY) ||
                !std::isfinite(map.scale) ||
                map.scale <= 0.001) {
                continue;
            }

            if (!map.dynamic) {
                const bool validBounds =
                    std::isfinite(map.minX) &&
                    std::isfinite(map.maxX) &&
                    std::isfinite(map.minY) &&
                    std::isfinite(map.maxY) &&
                    std::fabs(map.maxX - map.minX) >= 128.0 &&
                    std::fabs(map.maxY - map.minY) >= 128.0;
                if (!validBounds)
                    continue;
            }

            if (map.radarImage.empty())
                map.radarImage = std::format("/data/{}/radar.webp", map.name);
            if (map.backgroundImage.empty())
                map.backgroundImage = map.radarImage;

            maps.push_back(std::move(map));
        }

        return !maps.empty();
    }

    std::vector<radar::MapDefinition> LoadMapsFromEmbeddedAsset()
    {
        webradar::EmbeddedAsset asset{};
        if (!webradar::FindEmbeddedAsset("/maps.json", &asset))
            return {};

        const auto* first = static_cast<const char*>(asset.data);
        const auto* last = first + asset.size;
        nlohmann::json root = nlohmann::json::parse(first, last, nullptr, false);
        std::vector<radar::MapDefinition> maps;
        AppendMapsFromJson(root, maps);
        return maps;
    }

    std::vector<radar::MapDefinition> LoadMapsFromAssetFile()
    {
        const auto rawPath = app::paths::ResolveWebRadarAssetPath("maps.json");
        if (rawPath.empty())
            return {};

        std::error_code ec;
        const auto path = std::filesystem::weakly_canonical(rawPath, ec);
        const auto& candidate = ec ? rawPath : path;
        if (!std::filesystem::exists(candidate, ec))
            return {};

        std::ifstream file(candidate, std::ios::in | std::ios::binary);
        if (!file.is_open())
            return {};

        nlohmann::json root = nlohmann::json::parse(file, nullptr, false);
        std::vector<radar::MapDefinition> maps;
        AppendMapsFromJson(root, maps);
        return maps;
    }

    std::vector<radar::MapDefinition> LoadMapDefinitions()
    {
        auto maps = LoadMapsFromAssetFile();
        if (!maps.empty())
            return maps;

        maps = LoadMapsFromEmbeddedAsset();
        if (!maps.empty())
            return maps;

        return BuildFallbackMaps();
    }

    bool EqualsIgnoreCase(std::string_view lhs, std::string_view rhs)
    {
        if (lhs.size() != rhs.size())
            return false;

        for (size_t i = 0; i < lhs.size(); ++i) {
            const auto a = static_cast<unsigned char>(lhs[i]);
            const auto b = static_cast<unsigned char>(rhs[i]);
            if (std::tolower(a) != std::tolower(b))
                return false;
        }

        return true;
    }

    std::string NormalizeMapCandidate(std::string_view rawName)
    {
        size_t begin = 0;
        size_t end = rawName.size();
        while (begin < end && std::isspace(static_cast<unsigned char>(rawName[begin])) != 0)
            ++begin;
        while (end > begin && std::isspace(static_cast<unsigned char>(rawName[end - 1])) != 0)
            --end;
        if (begin >= end)
            return {};

        std::string text(rawName.substr(begin, end - begin));
        std::replace(text.begin(), text.end(), '\\', '/');

        const size_t slash = text.find_last_of('/');
        if (slash != std::string::npos)
            text.erase(0, slash + 1);

        const size_t dot = text.find_last_of('.');
        if (dot != std::string::npos)
            text.erase(dot);

        std::string normalized;
        normalized.reserve(text.size());
        for (const unsigned char ch : text) {
            if (std::isalnum(ch) != 0 || ch == '_')
                normalized.push_back(static_cast<char>(std::tolower(ch)));
        }

        if (normalized.empty() || normalized.size() >= 64)
            return {};

        return normalized;
    }

    int QuantizeMapCoord(double value)
    {
        constexpr double kStep = 16.0;
        return static_cast<int>(std::lround(value / kStep) * kStep);
    }
}

namespace radar {

const std::vector<MapDefinition>& GetMapDefinitions()
{
    static const std::vector<MapDefinition> maps = LoadMapDefinitions();
    return maps;
}

std::string NormalizeMapName(std::string_view name)
{
    const std::string normalized = NormalizeMapCandidate(name);
    if (normalized.empty())
        return {};

    for (const auto& map : GetMapDefinitions()) {
        if (EqualsIgnoreCase(map.name, normalized))
            return map.name;
    }

    return {};
}

const MapDefinition* FindMapByName(std::string_view name)
{
    const std::string normalized = NormalizeMapCandidate(name);
    if (normalized.empty())
        return nullptr;

    for (const auto& map : GetMapDefinitions()) {
        if (EqualsIgnoreCase(map.name, normalized))
            return &map;
    }

    return nullptr;
}

const MapDefinition* ResolveMapByBounds(
    double minX,
    double minY,
    double maxX,
    double maxY,
    bool allowLooseMatch)
{
    if (!std::isfinite(minX) || !std::isfinite(maxX) ||
        !std::isfinite(minY) || !std::isfinite(maxY)) {
        return nullptr;
    }

    const double runtimeSpanX = std::fabs(maxX - minX);
    const double runtimeSpanY = std::fabs(maxY - minY);
    const double runtimeCenterX = (minX + maxX) * 0.5;
    const double runtimeCenterY = (minY + maxY) * 0.5;

    const MapDefinition* best = nullptr;
    const MapDefinition* second = nullptr;
    double bestLegacyScore = 1e18;
    double bestSpanScore = 1e18;
    double bestCenterScore = 1e18;
    double secondLegacyScore = 1e18;
    double secondSpanScore = 1e18;
    double secondCenterScore = 1e18;

    for (const auto& map : GetMapDefinitions()) {
        if (map.dynamic)
            continue;

        const double mapSpanX = std::fabs(map.maxX - map.minX);
        const double mapSpanY = std::fabs(map.maxY - map.minY);
        const double mapCenterX = (map.minX + map.maxX) * 0.5;
        const double mapCenterY = (map.minY + map.maxY) * 0.5;

        const double legacyScore =
            std::fabs(minX - map.minX) +
            std::fabs(maxX - map.maxX) +
            std::fabs(minY - map.minY) +
            std::fabs(maxY - map.maxY);
        const double spanScore =
            std::fabs(runtimeSpanX - mapSpanX) +
            std::fabs(runtimeSpanY - mapSpanY);
        const double centerScore =
            std::fabs(runtimeCenterX - mapCenterX) +
            std::fabs(runtimeCenterY - mapCenterY);

        const bool better =
            legacyScore < bestLegacyScore - 0.1 ||
            (std::fabs(legacyScore - bestLegacyScore) <= 0.1 &&
             (spanScore < bestSpanScore - 0.1 ||
              (std::fabs(spanScore - bestSpanScore) <= 0.1 &&
               centerScore < bestCenterScore)));
        if (!better) {
            const bool secondBetter =
                legacyScore < secondLegacyScore - 0.1 ||
                (std::fabs(legacyScore - secondLegacyScore) <= 0.1 &&
                 (spanScore < secondSpanScore - 0.1 ||
                  (std::fabs(spanScore - secondSpanScore) <= 0.1 &&
                   centerScore < secondCenterScore)));
            if (secondBetter) {
                second = &map;
                secondLegacyScore = legacyScore;
                secondSpanScore = spanScore;
                secondCenterScore = centerScore;
            }
            continue;
        }

        second = best;
        secondLegacyScore = bestLegacyScore;
        secondSpanScore = bestSpanScore;
        secondCenterScore = bestCenterScore;
        best = &map;
        bestLegacyScore = legacyScore;
        bestSpanScore = spanScore;
        bestCenterScore = centerScore;
    }

    if (!best)
        return nullptr;

    const bool exactBoundsMatch = bestLegacyScore <= 320.0;
    const bool closeSpanMatch =
        allowLooseMatch &&
        bestLegacyScore <= 900.0 &&
        bestSpanScore <= 128.0 &&
        bestCenterScore <= 384.0;
    const bool ambiguousBounds =
        second != nullptr &&
        secondLegacyScore <= 900.0 &&
        std::fabs(secondLegacyScore - bestLegacyScore) <= 64.0 &&
        secondSpanScore <= 192.0 &&
        secondCenterScore <= 512.0;
    if (ambiguousBounds)
        return nullptr;

    return (exactBoundsMatch || closeSpanMatch) ? best : nullptr;
}

std::string BuildMapKeyFromBounds(double minX, double minY, double maxX, double maxY)
{
    if (const MapDefinition* map = ResolveMapByBounds(minX, minY, maxX, maxY, true))
        return map->name;

    return std::format(
        "dynamic_{}_{}__{}_{}",
        QuantizeMapCoord(minX),
        QuantizeMapCoord(minY),
        QuantizeMapCoord(maxX),
        QuantizeMapCoord(maxY));
}

std::string BuildMapsJson()
{
    nlohmann::json root;
    root["maps"] = nlohmann::json::array();
    for (const auto& map : GetMapDefinitions()) {
        root["maps"].push_back({
            {"name", map.name},
            {"display_name", map.displayName.empty() ? map.name : map.displayName},
            {"origin", { {"x", map.originX}, {"y", map.originY} }},
            {"scale", map.scale},
            {"dynamic", map.dynamic},
            {"bounds", {
                {"min_x", map.minX},
                {"max_x", map.maxX},
                {"min_y", map.minY},
                {"max_y", map.maxY}
            }},
            {"images", {
                {"radar", map.radarImage},
                {"background", map.backgroundImage}
            }}
        });
    }

    return root.dump();
}

}
