#pragma once
// Local UI preferences only. No save-game changes and no asset/clone resurrection.
#include "Generator/QuickMenuDecorations.h"
#include "Runtime/F2PreferenceFile.h"
#include "Runtime/F2CacheMigration.h"
#include <nlohmann/json.hpp>

namespace PS::F2FavoriteStore {
inline constexpr std::size_t MaxBytes=F2PreferenceFile::MaxBytes;
inline nlohmann::json Encode(const QuickDecorations::Favorites& records) {
    if(records.size()>QuickDecorations::MaxFavorites)throw std::runtime_error("Too many Helpy favorites");
    auto rows=nlohmann::json::array();
    for(const auto& [path,name]:records) {
        if(!QuickDecorations::ValidFavoriteKey(path)||name.size()>512)throw std::runtime_error("Invalid Helpy favorite entry");
        rows.push_back({{"Key",path},{"Name",name}});
    }
    return {{"Version",2},{"Favorites",std::move(rows)}};
}
inline QuickDecorations::Favorites Decode(const nlohmann::json& document) {
    if(!document.is_object()||!document.contains("Version")||(document["Version"]!=1&&document["Version"]!=2)
        ||!document.contains("Favorites")||!document["Favorites"].is_array()
        ||document["Favorites"].size()>QuickDecorations::MaxFavorites)
        throw std::runtime_error("Unsupported Helpy favorites document; existing file left unchanged");
    QuickDecorations::Favorites records;
    const char* keyField=document["Version"]==1?"Path":"Key";
    for(const auto& row:document["Favorites"]) {
        if(!row.is_object()||!row.contains(keyField)||!row[keyField].is_string()
            ||!row.contains("Name")||!row["Name"].is_string())throw std::runtime_error("Invalid Helpy favorite entry");
        const auto path=row[keyField].get<std::string>(),name=row["Name"].get<std::string>();
        if(!(document["Version"]==1?QuickDecorations::ValidFavoritePath(path):QuickDecorations::ValidFavoriteKey(path))||name.size()>512)throw std::runtime_error("Invalid Helpy favorite path/name");
        records[path]=name;
    }
    return records;
}
inline QuickDecorations::Favorites Load(const std::filesystem::path& file) {
    const auto text=F2PreferenceFile::Read(file);if(!text)return {};
    auto doc=nlohmann::json::parse(*text,[](int depth,nlohmann::json::parse_event_t,nlohmann::json&){
        if(depth>8)throw std::runtime_error("Helpy favorites document is too deeply nested");
        return true;
    });
    return Decode(doc);
}
inline void Save(const std::filesystem::path& file,const QuickDecorations::Favorites& records) {
    const auto document=Encode(records);const auto text=document.dump(2);
    F2PreferenceFile::Write(file,text,[&](const std::filesystem::path& staging){
        if(Load(staging)!=records)throw std::runtime_error("Helpy favorites readback failed");
    });
}
inline QuickDecorations::Favorites LoadWithLegacy(const std::filesystem::path& current,
    const std::filesystem::path& legacy,bool& migrated) {
    const auto result=F2CacheMigration::Prepare(current,legacy,[](const auto& staging){(void)Load(staging);});
    migrated=result==F2CacheMigration::Result::CopiedLegacy;
    return Load(current);
}
} // namespace PS::F2FavoriteStore
