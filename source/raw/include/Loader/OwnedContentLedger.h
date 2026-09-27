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
inline bool SnapshotComplete=false;
inline std::filesystem::path LedgerPath(const std::filesystem::path& settingsDirectory) {
    return settingsDirectory / "safesave" / "OwnedContentLedger.json";
}
inline std::filesystem::path LegacyLedgerPath(const std::filesystem::path& settingsDirectory) {
    return settingsDirectory / "OwnedContentLedger.json";
}
inline void Validate(const Record& value) {
    static const std::set<std::string> Kinds{"Item","Recipe","Building","Quest","Journal","Lore"};
    const bool journalIdentity=value.Kind=="Journal" || value.Kind=="Lore";
    const bool validIdentity=journalIdentity
        ? !value.PersistenceID.empty() && value.PersistenceID.size()<=1024
            && std::none_of(value.PersistenceID.begin(),value.PersistenceID.end(),
                [](unsigned char character){return character<32 || character==127;})
        : IsCanonicalPersistenceId(value.PersistenceID);
    if(!Kinds.contains(value.Kind))throw std::runtime_error("Owned-content Kind is unsupported: "+value.Kind);
    if(value.Owner.empty() || value.Owner.size()>256)
        throw std::runtime_error("Owned-content Owner is empty or too long");
    if(!validIdentity)throw std::runtime_error("Owned-content PersistenceID is invalid for "+value.Kind);
    if(value.InternalName.empty() || value.InternalName.size()>1024)
        throw std::runtime_error("Owned-content InternalName is empty or too long");
    if(value.Source.size()>2048)throw std::runtime_error("Owned-content Source is too long");
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
    SnapshotComplete=true;
    SnapshotActive=true;
}

inline void MarkSnapshotIncomplete() {
    std::lock_guard lock(SnapshotMutex);
    if(SnapshotActive)SnapshotComplete=false;
}

inline void Merge(const std::filesystem::path& path,const std::vector<Record>& current) {
    std::lock_guard lock(SnapshotMutex);
    if(SnapshotActive && path.lexically_normal()==SnapshotPath) {
        for(const auto& value:current) {
            Validate(value);
            // PersistenceID is the durable save identity. Owner, object path,
            // internal name, and kind describe the definition that supplies it
            // during this run and may legitimately change when a mod is moved,
            // renamed, or reorganized. The live registries remain responsible
            // for rejecting two simultaneously loaded objects with one ID.
            SnapshotCurrent[value.PersistenceID]=value;
        }
        return;
    }
    std::map<std::string,Record> records;
    for(auto value:Read(path))records.emplace(value.PersistenceID,std::move(value));
    for(const auto& value:current) {
        Validate(value);
        records[value.PersistenceID]=value;
    }
    Write(path,records);
}

inline std::vector<Record> CompareSnapshot(const std::filesystem::path& path) {
    std::lock_guard lock(SnapshotMutex);
    if(!SnapshotActive || path.lexically_normal()!=SnapshotPath)
        throw std::runtime_error("RuneSchema owned-content snapshot was not started");
    if(!SnapshotComplete)
        throw std::runtime_error("one or more mod sections did not load; pruning was skipped and the previous identity snapshot was retained");
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
    SnapshotComplete=false;
    SnapshotActive=false;
}
inline bool LooksLikeItemClone(const std::string& destination,const std::string& source) {
    auto lower=[](std::string value){std::transform(value.begin(),value.end(),value.begin(),
        [](unsigned char c){return static_cast<char>(std::tolower(c));});return value;};
    const auto target=lower(destination),origin=lower(source);
    return target.find("/items/")!=std::string::npos
        || origin.find("/items/")!=std::string::npos
        || origin.find("item_")!=std::string::npos;
}

}
