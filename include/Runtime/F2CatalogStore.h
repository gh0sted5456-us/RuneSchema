#pragma once
#include "Generator/F2CatalogPlan.h"
#include "Runtime/F2PreferenceFile.h"
#include "Runtime/F2CacheMigration.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace PS::F2CatalogStore {
using json=nlohmann::json;
inline constexpr std::size_t MaxBytes=16*1024*1024;
inline std::int64_t Now() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
inline std::optional<json> Read(const std::filesystem::path& path) {
    const auto text=F2PreferenceFile::Read(path,MaxBytes);if(!text)return {};
    return json::parse(*text,[](int depth,json::parse_event_t,json&){
        if(depth>24)throw std::runtime_error("F2 catalogue JSON nesting exceeds limit");
        return true;
    });
}
inline void Write(const std::filesystem::path& path,const json& document) {
    F2PreferenceFile::Write(path,document.dump(2),[&](const auto& stage){
        auto parsed=Read(stage);if(!parsed||*parsed!=document)throw std::runtime_error("F2 catalogue cache readback failed");
    },MaxBytes);
}
inline json EncodeSources(const F2Catalog::Sources& sources) {
    return {{"Version",1},{"ResourceRoots",sources.resources},{"EnemyRoots",sources.enemies},
        {"UseRsdwReferenceIndex",sources.onlineReference},{"RsdwDataset",sources.dataset},
        {"RsdwRef",sources.reference},{"ReferenceUpdatePolicy","Manual"}};
}
inline F2Catalog::Sources DecodeSources(const json& document) {
    if(!document.is_object()||document.value("Version",0)!=1)throw std::runtime_error("Unsupported F2 catalogue settings version");
    F2Catalog::Sources sources;
    const auto roots=[&](const char* key,std::vector<std::string>& target) {
        if(!document.contains(key))return;
        const auto& rows=document.at(key);
        if(!rows.is_array()||rows.size()>F2Catalog::MaxRoots)throw std::runtime_error("Invalid F2 scan roots");
        target.clear();for(const auto& row:rows) {
            if(!row.is_string()||!F2Catalog::ValidRoot(row.get<std::string>()))throw std::runtime_error("Invalid F2 scan root; use /Mount/Path without trailing slash");
            target.push_back(row.get<std::string>());
        }
    };
    roots("ResourceRoots",sources.resources);roots("EnemyRoots",sources.enemies);
    sources.onlineReference=document.value("UseRsdwReferenceIndex",true);
    sources.dataset=document.value("RsdwDataset",std::string("1.0.0.2"));
    sources.reference=document.value("RsdwRef",std::string("main"));
    // Legacy ReferenceCacheHours is accepted but no longer expires or fetches an index.
    sources.referenceCacheHours=0;
    if(document.contains("ReferenceUpdatePolicy")&&document.at("ReferenceUpdatePolicy")!="Manual")
        throw std::runtime_error("Helpy uses manual RSDW updates; set ReferenceUpdatePolicy to Manual");
    if(!F2Catalog::SimpleToken(sources.dataset)||!F2Catalog::SimpleToken(sources.reference))
        throw std::runtime_error("Invalid Helpy reference index settings");
    return sources;
}
inline json EncodeEntries(const std::vector<F2Catalog::Candidate>& entries) {
    if(entries.size()>F2Catalog::MaxEntries)throw std::runtime_error("F2 index entry limit exceeded");
    auto out=json::array();for(const auto& entry:entries) {
        if(!F2Catalog::ValidObjectPath(entry.path)||entry.name.size()>512)throw std::runtime_error("Invalid F2 index entry");
        out.push_back({{"Path",entry.path},{"Name",entry.name},{"Kind",F2Catalog::KindName(entry.kind)}});
    }
    return out;
}
inline std::vector<F2Catalog::Candidate> DecodeEntries(const json& document,std::vector<std::string>& errors) {
    if(!document.is_object()||document.value("Version",0)!=1||!document.contains("Entries")
        ||!document["Entries"].is_array()||document["Entries"].size()>F2Catalog::MaxEntries)
        throw std::runtime_error("Unsupported F2 path index; existing file retained");
    F2Catalog::Plan plan;std::size_t row=0;
    for(const auto& entry:document["Entries"]) {
        ++row;try {
            if(!entry.is_object())throw std::runtime_error("Expected an object");
            const auto path=entry.at("Path").get<std::string>();
            const auto kind=F2Catalog::ParseKind(entry.value("Kind",std::string("Resource")));
            if(!kind)throw std::runtime_error("Unknown Kind (use Item, Enemy, Resource or Class)");
            plan.Add({path,entry.value("Name",std::string{}),*kind});
        }catch(const std::exception& e){errors.push_back("Index row "+std::to_string(row)+": "+e.what());}
    }
    return std::move(plan.entries);
}
// These two generated cache formats must be valid before a legacy runtime
// copy is imported into RuneSchema's own settings directory. User-authored
// indexes retain their existing per-entry error reporting in DecodeEntries.
inline std::optional<json> ReadCacheWithLegacy(const std::filesystem::path& current,
    const std::filesystem::path& legacy,bool& migrated) {
    migrated=false;
    const auto validated=[](const std::filesystem::path& path)->std::optional<json> {
        auto document=Read(path);
        if(document) {
            std::vector<std::string> errors;
            (void)DecodeEntries(*document,errors);
            if(!errors.empty())throw std::runtime_error("Invalid F2 cache: "+errors.front()+"; existing file retained");
        }
        return document;
    };
    const auto result=F2CacheMigration::Prepare(current,legacy,[&](const auto& staging) {
        if(!validated(staging))throw std::runtime_error("Missing F2 cache migration staging file");
    },MaxBytes);
    migrated=result==F2CacheMigration::Result::CopiedLegacy;
    return validated(current);
}
// Compatibility is separate from age. A successful reference index never expires.
// Old v8 SourceSettings are compared by discovery identity only, not TTL/online toggle.
inline bool ReferenceFresh(const json& cached,const F2Catalog::Sources& sources) {
    if(!cached.is_object()||cached.value("Dataset",std::string{})!=sources.dataset
        ||cached.value("Reference",std::string{})!=sources.reference
        ||!cached.contains("SourceSettings")||!cached["SourceSettings"].is_object())return false;
    const auto& settings=cached["SourceSettings"];
    for(const auto* key:{"ResourceRoots","EnemyRoots"}) {
        if(!settings.contains(key)||!settings[key].is_array())return false;
        auto saved=settings[key].get<std::vector<std::string>>();
        auto current=std::string_view(key)=="ResourceRoots"?sources.resources:sources.enemies;
        std::sort(saved.begin(),saved.end());std::sort(current.begin(),current.end());
        if(saved!=current)return false;
    }
    return true;
}
} // namespace PS::F2CatalogStore
