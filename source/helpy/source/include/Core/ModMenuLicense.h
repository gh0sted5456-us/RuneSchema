#pragma once
#include "Core/RsmkResources.h"
#include <windows.h>
#include <string_view>

namespace PS::ModMenuLicense {
inline std::string_view Text(HMODULE module=GetModuleHandleW(nullptr)){
    const auto resource=FindResourceW(module,MAKEINTRESOURCEW(IDR_MOD_MENU_LICENSE),RT_RCDATA);
    const auto data=resource?static_cast<const char*>(LockResource(LoadResource(module,resource))):nullptr;
    return data?std::string_view(data,SizeofResource(module,resource)):std::string_view{};
}
}
