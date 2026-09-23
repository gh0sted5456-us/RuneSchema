#pragma once
#include "Core/ConfigFiles.h"
#include "Core/JsonDocument.h"
#include "Loader/ItemIdentity.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace DragonWilds::OwnedContent {
inline std::mutex ActiveDeclarationMutex;
inline std::set<std::string> ActiveDeclarationPaths;
inline void RegisterActiveDeclarationPath(const std::string& path) {
    std::lock_guard lock(ActiveDeclarationMutex);ActiveDeclarationPaths.insert(path);
}
inline bool IsActiveDeclarationPath(const std::string& path) {
    std::lock_guard lock(ActiveDeclarationMutex);return ActiveDeclarationPaths.contains(path);
}
struct Record {
    std::string Kind;
    std::string Owner;
    std::string PersistenceID;
    std::string InternalName;
    std::string Source;
    bool InternalNameAsserted=false;
};
inline std::mutex SnapshotMutex;
inline bool SnapshotActive=false;
inline std::filesystem::path SnapshotPath;
inline std::map<std::string,Record> SnapshotPrevious;
inline std::map<std::string,Record> SnapshotCurrent;
inline std::filesystem::path LedgerPath(const std::filesystem::path& settingsDirectory) {
    return settingsDirectory / "safesave" / "OwnedContentLedger.json";
}
inline std::filesystem::path LegacyLedgerPath(const std::filesystem::path& settingsDirectory) {
    return settingsDirectory / "OwnedContentLedger.json";
}
inline void Validate(const Record& value) {
    static const std::set<std::string> Kinds{"Item","Recipe","Building","Quest","Journal","Lore"};
    if(!Kinds.contains(value.Kind) || value.Owner.empty() || value.Owner.size()>256
        || !IsCanonicalPersistenceId(value.PersistenceID) || value.InternalName.empty()
        || value.InternalName.size()>1024 || value.Source.size()>2048)
        throw std::runtime_error("Invalid RuneSchema owned-content record");
}

inline std::string DeclaredInternalName(const std::string& path) {
    const auto dot=path.rfind('.');
    if(dot==std::string::npos || dot+1>=path.size())throw std::runtime_error("Declared cooked path requires Package.Asset form");
    return path.substr(dot+1);
}

inline std::vector<Record> Declarations(const nlohmann::json& document,const std::string& owner,
    const std::string& forcedKind={}) {
    if(!document.is_object() || !document.contains("$declaration"))return {};
    const auto& raw=document.at("$declaration");
    const auto rows=raw.is_array()?raw:nlohmann::json::array({raw});
    if(rows.empty() || rows.size()>4096)throw std::runtime_error("$declaration requires 1..4096 records");
    std::vector<Record> result;std::set<std::string> identities;
    for(const auto& row:rows) {
        if(!row.is_object())throw std::runtime_error("$declaration record must be an object");
        for(const auto& [key,value]:row.items())
            if(key!="Kind" && key!="Path" && key!="PersistenceID" && key!="InternalName")
                throw std::runtime_error("Unknown $declaration field: "+key);
        auto kind=row.value("Kind",forcedKind);
        if(!forcedKind.empty() && kind!=forcedKind)
            throw std::runtime_error("$declaration Kind must match its loader: "+forcedKind);
        const auto path=row.value("Path",std::string{}),id=row.value("PersistenceID",std::string{});
        if(path.empty() || path.size()>2048 || path.front()!='/' || path.find_first_of("\r\n\t")!=std::string::npos)
            throw std::runtime_error("$declaration Path must be an exact cooked object path");
        Record value{kind,owner,id,row.value("InternalName",DeclaredInternalName(path)),path,row.contains("InternalName")};
        Validate(value);
        if(!identities.insert(kind+"\n"+id).second)throw std::runtime_error("Duplicate $declaration identity");
        result.push_back(std::move(value));
    }
    return result;
}
inline std::vector<Record> Read(const std::filesystem::path& path) {
    if(!std::filesystem::exists(path))return {};
    const auto document=nlohmann::json::parse(PS::ConfigFiles::Read(path,8*1024*1024));
    if(!document.is_object() || document.value("Kind",std::string{})!="RuneSchemaOwnedContent"
        || document.value("Version",0)!=1 || !document.contains("Records")
        || !document.at("Records").is_array() || document.at("Records").size()>16384)
        throw std::runtime_error("Unsupported RuneSchema owned-content ledger");
    std::vector<Record> result;std::set<std::string> ids;
    for(const auto& row:document.at("Records")) {
        if(!row.is_object())throw std::runtime_error("Malformed RuneSchema owned-content row");
        Record value{row.value("Kind",std::string{}),row.value("Owner",std::string{}),
            row.value("PersistenceID",std::string{}),row.value("InternalName",std::string{}),
            row.value("Source",std::string{})};
        Validate(value);
        if(!ids.insert(value.PersistenceID).second)
            throw std::runtime_error("Duplicate RuneSchema owned-content identity");
        result.push_back(std::move(value));
    }
    return result;
}
inline void Write(const std::filesystem::path& path,const std::map<std::string,Record>& records) {
    nlohmann::json rows=nlohmann::json::array();
    for(const auto& [id,value]:records)rows.push_back({{"Kind",value.Kind},{"Owner",value.Owner},
        {"PersistenceID",value.PersistenceID},{"InternalName",value.InternalName},{"Source",value.Source}});
    PS::ConfigFiles::Write(path,nlohmann::json{{"Kind","RuneSchemaOwnedContent"},{"Version",1},
        {"Policy","Previous successful active-content identity snapshot; missing identities are permanently removed from saves on the next load."},
        {"Records",rows}}.dump(2));
}

inline void BeginSnapshot(const std::filesystem::path& path) {
    // Preserve the last known-good snapshot during an upgrade. The legacy
    // location is read once only when the canonical SafeSave file is absent.
    if(!std::filesystem::exists(path)) {
        const auto settingsDirectory=path.parent_path().parent_path();
        const auto legacy=LegacyLedgerPath(settingsDirectory);
        if(std::filesystem::exists(legacy)) {
            std::map<std::string,Record> migrated;
            for(auto value:Read(legacy))migrated.emplace(value.PersistenceID,std::move(value));
            Write(path,migrated);
            std::error_code ignored;
            std::filesystem::remove(legacy,ignored);
        }
    }
    std::map<std::string,Record> previous;
    for(auto value:Read(path))previous.emplace(value.PersistenceID,std::move(value));
    std::lock_guard lock(SnapshotMutex);
    SnapshotPath=path.lexically_normal();
    SnapshotPrevious=std::move(previous);
    SnapshotCurrent.clear();
    SnapshotActive=true;
}

inline void Merge(const std::filesystem::path& path,const std::vector<Record>& current) {
    std::lock_guard lock(SnapshotMutex);
    if(SnapshotActive && path.lexically_normal()==SnapshotPath) {
        for(const auto& value:current) {
            Validate(value);
            const auto old=SnapshotPrevious.find(value.PersistenceID);
            if(old!=SnapshotPrevious.end() && old->second.Owner!=value.Owner)
                throw std::runtime_error("RuneSchema owned-content identity transfer refused");
            const auto found=SnapshotCurrent.find(value.PersistenceID);
            if(found!=SnapshotCurrent.end() && found->second.Owner!=value.Owner)
                throw std::runtime_error("RuneSchema owned-content identity collision");
            SnapshotCurrent[value.PersistenceID]=value;
        }
        return;
    }
    std::map<std::string,Record> records;
    for(auto value:Read(path))records.emplace(value.PersistenceID,std::move(value));
    for(const auto& value:current) {
        Validate(value);
        const auto found=records.find(value.PersistenceID);
        if(found!=records.end() && found->second.Owner!=value.Owner)
            throw std::runtime_error("RuneSchema owned-content identity transfer refused");
        records[value.PersistenceID]=value;
    }
    Write(path,records);
}

inline std::vector<Record> CompareSnapshot(const std::filesystem::path& path) {
    std::lock_guard lock(SnapshotMutex);
    if(!SnapshotActive || path.lexically_normal()!=SnapshotPath)
        throw std::runtime_error("RuneSchema owned-content snapshot was not started");
    std::vector<Record> missing;
    for(const auto& [id,value]:SnapshotPrevious)
        if(!SnapshotCurrent.contains(id))missing.push_back(value);
    return missing;
}

inline void CommitSnapshot(const std::filesystem::path& path) {
    std::lock_guard lock(SnapshotMutex);
    if(!SnapshotActive || path.lexically_normal()!=SnapshotPath)
        throw std::runtime_error("RuneSchema owned-content snapshot was not started");
    Write(path,SnapshotCurrent);
    SnapshotPrevious.clear();
    SnapshotCurrent.clear();
    SnapshotPath.clear();
    SnapshotActive=false;
}
inline std::vector<Record> Absent(const std::vector<Record>& records,const std::set<std::string>& active) {
    const auto key=[](std::string value){std::transform(value.begin(),value.end(),value.begin(),
        [](unsigned char c){return static_cast<char>(std::tolower(c));});return value;};
    std::set<std::string> activeKeys;
    for(const auto& owner:active)activeKeys.insert(key(owner));
    std::vector<Record> result;
    for(const auto& value:records)if(!activeKeys.contains(key(value.Owner)))result.push_back(value);
    return result;
}

inline bool LooksLikeItemClone(const std::string& destination,const std::string& source) {
    auto lower=[](std::string value){std::transform(value.begin(),value.end(),value.begin(),
        [](unsigned char c){return static_cast<char>(std::tolower(c));});return value;};
    const auto target=lower(destination),origin=lower(source);
    return target.find("/items/")!=std::string::npos
        || origin.find("/items/")!=std::string::npos
        || origin.find("item_")!=std::string::npos;
}

inline std::vector<Record> DiscoverDisabledDefinitions(
    const std::filesystem::path& mods,const std::set<std::string>& active) {
    std::map<std::string,Record> found;
    if(!std::filesystem::is_directory(mods))return {};
    for(const auto& folder:std::filesystem::directory_iterator(mods)) {
        if(!folder.is_directory())continue;
        const auto owner=folder.path().filename().string();
        if(active.contains(owner))continue;
        const auto read=[&](const std::filesystem::path& path,const std::string& kind,bool clones) {
        PS::JsonHelpers::ParseJsonFilesInPathIsolated(path,
            [&](const nlohmann::json& document){
                if(!document.is_object())return;
                if(clones)for(const auto& [destination,value]:document.items()) {
                    if(!value.is_object() || !value.contains("$Clone")
                        || !value.at("$Clone").is_string())continue;
                    const auto source=value.at("$Clone").get<std::string>();
                    if(!LooksLikeItemClone(destination,source))continue;
                    Record record{"Item",owner,value.value("PersistenceID",std::string{}),
                        value.value("InternalName",std::string{}),destination};
                    Validate(record);
                    const auto existing=found.find(record.PersistenceID);
                    if(existing!=found.end() && existing->second.Owner!=record.Owner)
                        throw std::runtime_error("Disabled RuneSchema definitions reuse an owned identity");
                    found[record.PersistenceID]=std::move(record);
                }
                for(auto record:Declarations(document,owner,kind)) {
                    const auto existing=found.find(record.PersistenceID);
                    if(existing!=found.end() && existing->second.Owner!=record.Owner)
                        throw std::runtime_error("Disabled RuneSchema declarations reuse an owned identity");
                    found[record.PersistenceID]=std::move(record);
                }
            },[](const std::filesystem::path&,const std::string&){});
        };
        read(folder.path()/"assets","",true);
        read(folder.path()/"buildings","Building",false);
        read(folder.path()/"quests","Quest",false);
        read(folder.path()/"journal","Journal",false);
        read(folder.path()/"lore","Lore",false);
    }
    std::vector<Record> result;for(auto& [id,value]:found)result.push_back(std::move(value));
    return result;
}

inline std::vector<Record> ReadCloneManifest(const std::filesystem::path& path) {
    if(!std::filesystem::exists(path))return {};
    const auto document=nlohmann::json::parse(PS::ConfigFiles::Read(path,8*1024*1024));
    if(!document.is_object() || document.value("Kind",std::string{})!="RuneSchemaCloneManifest"
        || !document.contains("Records") || !document.at("Records").is_array()
        || document.at("Records").size()>4096)
        throw std::runtime_error("Unsupported RuneSchema clone manifest");
    std::vector<Record> result;
    for(const auto& row:document.at("Records")) {
        if(!row.is_object() || !row.value("Registered",false))continue;
        const auto source=row.value("SourceAsset",std::string{});
        const auto destination=row.value("AssetPath",std::string{});
        if(!LooksLikeItemClone(destination,source))continue;
        Record record{"Item",row.value("CreatingMod",std::string{}),
            row.value("PersistenceID",std::string{}),row.value("InternalName",std::string{}),
            destination.empty()?source:destination};
        Validate(record);result.push_back(std::move(record));
    }
    return result;
}
}
