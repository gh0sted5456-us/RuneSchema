#pragma once
#include "Runtime/F2PreferenceFile.h"
#include <filesystem>
#include <stdexcept>

namespace PS::F2CacheMigration {
enum class Result { NoLegacyFile, CurrentExists, CopiedLegacy };

// Read legacy data only when the global settings copy is absent. The caller
// validates staged data with the catalogue's real JSON codec before publication.
// Never delete the legacy file, merge user settings, or replace a current file.
template<class Validate>
Result Prepare(const std::filesystem::path& current,const std::filesystem::path& legacy,
    Validate&& validate,std::size_t maxBytes=F2PreferenceFile::MaxBytes) {
    if(current.lexically_normal()==legacy.lexically_normal())
        throw std::runtime_error("Cache migration source and destination must differ");
    if(std::filesystem::is_symlink(current))
        throw std::runtime_error("F2 settings cache destination is a symlink");
    // Existence includes a directory or invalid file: the ordinary reader will
    // report it, instead of silently importing a legacy file over user data.
    if(std::filesystem::exists(current))return Result::CurrentExists;
    const auto previous=F2PreferenceFile::Read(legacy,maxBytes);
    if(!previous)return Result::NoLegacyFile;
    F2PreferenceFile::Write(current,*previous,[&](const auto& staging) {
        validate(staging);
        const auto verified=F2PreferenceFile::Read(staging,maxBytes);
        if(!verified||*verified!=*previous)
            throw std::runtime_error("F2 legacy cache migration readback differs");
    },maxBytes,F2PreferenceFile::WriteMode::CreateOnly);
    return Result::CopiedLegacy;
}
} // namespace PS::F2CacheMigration
