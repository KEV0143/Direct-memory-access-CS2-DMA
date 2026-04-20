#pragma once
#include <cstddef>
#include <string>

namespace webradar {

struct EmbeddedAsset
{
    const void* data;
    size_t      size;
};




bool FindEmbeddedAsset(const std::string& urlPath, EmbeddedAsset* out);

} 
