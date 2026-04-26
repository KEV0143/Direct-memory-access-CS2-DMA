#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace radar {

struct MapDefinition
{
    std::string name;
    std::string displayName;
    double minX = 0.0;
    double maxX = 0.0;
    double minY = 0.0;
    double maxY = 0.0;
    double originX = 0.0;
    double originY = 0.0;
    double scale = 1.0;
    bool dynamic = false;
    std::string radarImage;
    std::string backgroundImage;
};

const std::vector<MapDefinition>& GetMapDefinitions();
std::string NormalizeMapName(std::string_view name);
const MapDefinition* FindMapByName(std::string_view name);
const MapDefinition* ResolveMapByBounds(
    double minX,
    double minY,
    double maxX,
    double maxY,
    bool allowLooseMatch = true);
std::string BuildMapKeyFromBounds(double minX, double minY, double maxX, double maxY);
std::string BuildMapsJson();

}
