#pragma once
#include "Core/ConfigFiles.h"
#include "Core/JsonDocument.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace PS::PersistenceDiagnostics {
using Json=nlohmann::json;

inline std::string KindFromSource(const std::filesystem::path& source) {
    auto value=source.generic_string();
    std::transform(value.begin(),value.end(),value.begin(),
        [](unsigned char c){return static_cast<char>(std::tolower(c));});
    for(const auto& [needle,kind]:{
        std::pair{"recipes/","Recipe"},std::pair{"quests/","Quest"},
        std::pair{"journal/","Journal"},std::pair{"lore/","Lore"},
        std::pair{"buildings/","Building"},std::pair{"assets/","Asset"},
        std::pair{"vendors/","Vendor"},std::pair{"equipment/","Equipment"}})
        if(value.find(needle)!=std::string::npos)return kind;
    return "Unclassified";
}

inline void Collect(const Json& value,const std::string& owner,
    const std::filesystem::path& source,const std::string& pointer,
    Json& records,std::size_t& visited) {
    if(++visited>1'000'000 || records.size()>32768)
        throw std::runtime_error("Persistence diagnostic ledger exceeded its bounded scan");
    if(value.is_object()) {
        const auto id=value.find("PersistenceID");
        if(id!=value.end() && id->is_string() && !id->get_ref<const std::string&>().empty()) {
            const auto identity=id->get<std::string>();
            if(identity.size()>256)throw std::runtime_error("PersistenceID exceeds diagnostic limit");
            std::string internal;
            const auto name=value.find("InternalName");
            if(name!=value.end() && name->is_string())internal=name->get<std::string>();
            if(internal.size()>1024)throw std::runtime_error("InternalName exceeds diagnostic limit");
            records.push_back({{"Owner",owner},{"Kind",KindFromSource(source)},
                {"PersistenceID",identity},{"InternalName",internal.empty()?Json(nullptr):Json(internal)},
                {"Source",source.generic_string()},{"JsonPointer",pointer}});
        }
        for(const auto& [key,child]:value.items())
            Collect(child,owner,source,pointer+"/"+key,records,visited);
    } else if(value.is_array()) {
        for(std::size_t index=0;index<value.size();++index)
            Collect(value[index],owner,source,pointer+"/"+std::to_string(index),records,visited);
    }
}

inline void Write(const std::filesystem::path& modsRoot,
    const std::vector<std::filesystem::path::string_type>& orderedMods,
    const std::filesystem::path& output) {
    Json mods=Json::array(),records=Json::array(),errors=Json::array();
    std::size_t visited=0;
    for(const auto& nativeOwner:orderedMods) {
        const std::filesystem::path ownerPath=modsRoot/nativeOwner;
        const auto owner=ownerPath.filename().string();
        mods.push_back(owner);
        JsonHelpers::ParseJsonFilesInPathWithSourceIsolated(ownerPath,
            [&](const Json& document,const std::filesystem::path& relative) {
                Collect(document,owner,std::filesystem::path(owner)/relative,"",records,visited);
            },[&](const std::filesystem::path& path,const std::string& error) {
                if(errors.size()<1024)errors.push_back({{"Owner",owner},
                    {"Source",path.lexically_relative(modsRoot).generic_string()},
                    {"Error",error}});
            });
    }
    std::sort(records.begin(),records.end(),[](const Json& left,const Json& right) {
        return std::tie(left.at("Owner"),left.at("PersistenceID"),left.at("Source"),left.at("JsonPointer"))
            < std::tie(right.at("Owner"),right.at("PersistenceID"),right.at("Source"),right.at("JsonPointer"));
    });
    const Json document={{"Kind","RuneSchemaPersistenceDiagnosticLedger"},{"SchemaVersion",1},
        {"Authority","Diagnostic only; never consulted for cleanup, validation, loading, or save mutation."},
        {"Mods",mods},{"Records",records},{"Errors",errors}};
    ConfigFiles::Write(output,document.dump(2));
}
}
