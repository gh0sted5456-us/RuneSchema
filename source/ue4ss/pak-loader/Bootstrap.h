#pragma once

#include "PakLoaderApi.h"
#include <windows.h>
#include <filesystem>

namespace PS::PakLoader {
// Pass an address inside the existing UE4SS DLL from on_cpp_mods_loaded().
inline Result StartFromMod(const void* modAddress) noexcept
{
    static const auto result = [modAddress]() noexcept {
        HMODULE mod{};
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
            | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(modAddress), &mod))
            return Result::MountUnavailable;
        try {
            std::wstring filename(32768, L'\0');
            const auto size = GetModuleFileNameW(mod, filename.data(), static_cast<DWORD>(filename.size()));
            if (!size || size >= filename.size()) return Result::MountUnavailable;
            filename.resize(size);
            const auto dll = std::filesystem::path(filename).parent_path().parent_path() / "runeschema-pakloader.dll";
            const auto attributes = GetFileAttributesW(dll.c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES
                || (attributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)))
                return Result::MountUnavailable;
            const auto helper = LoadLibraryExW(dll.c_str(), nullptr,
                LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (!helper) return Result::MountUnavailable;
            const auto start = reinterpret_cast<Start>(GetProcAddress(helper, StartExport));
            const auto status = start ? start() : Result::MountUnavailable;
            FreeLibrary(helper);
            return status;
        } catch (...) {
            return Result::MountUnavailable;
        }
    }();
    return result;
}
}
