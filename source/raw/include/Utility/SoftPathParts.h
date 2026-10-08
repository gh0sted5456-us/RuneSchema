#pragma once
#include <string_view>

namespace PS {
struct SoftPathParts {
    std::wstring_view package, asset, subobject;
    bool valid{};
};
inline SoftPathParts SplitSoftPath(std::wstring_view path) {
    if (path.empty() || path.front() != L'/' || path.back() == L'.' || path.back() == L':') return {};
    const auto dot = path.find_first_of(L".:");
    if (dot == std::wstring_view::npos) return {path, {}, {}, true};
    if (path[dot] != L'.' || dot <= 1) return {};
    const auto colon = path.find(L':', dot + 1);
    const auto asset = path.substr(dot + 1, colon == std::wstring_view::npos ? colon : colon - dot - 1);
    if (asset.empty() || asset.find_first_of(L"./:") != std::wstring_view::npos) return {};
    const auto subobject = colon == std::wstring_view::npos ? std::wstring_view{} : path.substr(colon + 1);
    if (!subobject.empty() && (subobject.front() == L'.' || subobject.front() == L':'
        || subobject.find(L"..") != std::wstring_view::npos || subobject.find(L"::") != std::wstring_view::npos)) return {};
    return {path.substr(0, dot), asset, subobject, true};
}
}
