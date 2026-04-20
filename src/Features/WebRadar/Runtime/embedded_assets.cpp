#include "Features/WebRadar/embedded_assets.h"

#include <Windows.h>

namespace webradar {

bool FindEmbeddedAsset(const std::string& urlPath, EmbeddedAsset* out)
{
    if (!out || urlPath.empty())
        return false;

    
    const HMODULE hModule = GetModuleHandleA(nullptr);
    const HRSRC hRes = FindResourceA(hModule, urlPath.c_str(), MAKEINTRESOURCEA(10));
    if (!hRes)
        return false;

    const HGLOBAL hData = LoadResource(hModule, hRes);
    if (!hData)
        return false;

    const void* ptr = LockResource(hData);
    const DWORD sz  = SizeofResource(hModule, hRes);
    if (!ptr || sz == 0)
        return false;

    out->data = ptr;
    out->size = static_cast<size_t>(sz);
    return true;
}

} 
