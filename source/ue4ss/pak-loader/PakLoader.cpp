#include "PakLoaderApi.h"
#include "../../raw/include/Core/UE4SSPackages.h"

#include <map>

using PS::PakLoader::Result;
namespace fs = std::filesystem;

namespace PS::PakLoader {
bool Enroll(const fs::path& paks);
}

namespace {
Result ReadOwnPaks(fs::path& paks) noexcept;
}

extern "C" __declspec(dllexport) Result RuneSchemaPakLoader_QueryV1() noexcept
{
    fs::path paks;
    return ReadOwnPaks(paks);
}

extern "C" __declspec(dllexport) Result RuneSchemaPakLoader_StartV1() noexcept
{
    // Each mod copy validates and registers at most once during this process.
    static const auto result = []() noexcept {
        fs::path paks;
        const auto status = ReadOwnPaks(paks);
        if (status != Result::Ready) return status;
        try {
            if (PS::PakLoader::Enroll(paks)) return Result::Ready;
        } catch (...) {}
        return Result::MountUnavailable;
    }();
    return result;
}

namespace {
Result ReadOwnPaks(fs::path& paks) noexcept
{
    using PS::UE4SSPackages::Linked;
    try {
        HMODULE module{};
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
            | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&RuneSchemaPakLoader_QueryV1), &module))
            return Result::InvalidConfig;
        std::wstring filename(32768, L'\0');
        const auto size = GetModuleFileNameW(module, filename.data(), static_cast<DWORD>(filename.size()));
        if (!size || size >= filename.size()) return Result::InvalidConfig;
        filename.resize(size);
        const auto root = fs::path(filename).parent_path();
        const auto config = root / "config.ini";
        if (!root.is_absolute() || Linked(root) || Linked(config) || !fs::is_regular_file(config))
            return Result::InvalidConfig;
        wchar_t name[256]{};
        const auto length = GetPrivateProfileStringW(L"RuneSchemaPakLoader", L"ModName", L"",
            name, static_cast<DWORD>(std::size(name)), config.c_str());
        if (!length || length >= std::size(name) - 1
            || CompareStringOrdinal(name, -1, root.filename().c_str(), -1, TRUE) != CSTR_EQUAL)
            return Result::InvalidConfig;

        paks = root / "paks";
        if (!fs::exists(paks)) return Result::NoPaks;
        if (Linked(paks) || !fs::is_directory(paks)) return Result::InvalidContainers;

        constexpr unsigned pak = 1, utoc = 2, ucas = 4;
        const auto less = [](const std::wstring& a, const std::wstring& b) {
            return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_LESS_THAN;
        };
        std::map<std::wstring, unsigned, decltype(less)> containers(less);
        for (const auto& entry : fs::directory_iterator(paks)) {
            if (Linked(entry.path()) || !entry.is_regular_file()) return Result::InvalidContainers;
            const auto extension = entry.path().extension().native();
            unsigned part = 0;
            if (CompareStringOrdinal(extension.c_str(), -1, L".pak", -1, TRUE) == CSTR_EQUAL) part = pak;
            if (CompareStringOrdinal(extension.c_str(), -1, L".utoc", -1, TRUE) == CSTR_EQUAL) part = utoc;
            if (CompareStringOrdinal(extension.c_str(), -1, L".ucas", -1, TRUE) == CSTR_EQUAL) part = ucas;
            if (!part) continue;
            const auto stem = entry.path().stem().native();
            if (stem.empty() || entry.file_size() == 0) return Result::InvalidContainers;
            // Compare stems with the same case folding Windows applies to sibling files.
            auto& parts = containers[stem];
            if (parts & part) return Result::InvalidContainers;
            parts |= part;
        }
        if (containers.empty()) return Result::NoPaks;
        for (const auto& [stem, parts] : containers)
            if (parts != pak && parts != (pak | utoc | ucas)) return Result::InvalidContainers;
        return Result::Ready;
    } catch (...) {
        return Result::InvalidContainers;
    }
}
}
