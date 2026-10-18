#pragma once
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <stdexcept>
namespace PS::IconCacheCoverage {
using Json=nlohmann::json;
// A finished loop is not proof of coverage. Count only verified pixel files,
// and bind the expected path set to the exact source snapshot and selection.
inline bool Complete(const Json& state){
    return state.value("Stage",std::string{})=="Complete"&&!state.value("Active",true)
        &&state.value("Total",size_t{})>0&&state.value("Verified",size_t{})==state.value("Total",size_t{})
        &&state.value("Failed",size_t{})==0&&state.value("DiscoveryFailed",size_t{})==0;
}
inline std::set<std::string> Expected(const Json& coverage,const std::string& source,const Json& keys){
    const auto stage=coverage.value("Stage",std::string{});
    if(coverage.value("Active",true)||(stage!="Complete"&&stage!="Completed with failures")||coverage.value("Source",std::string{})!=source||!coverage.contains("SourceKeys")||coverage["SourceKeys"]!=keys)
        throw std::runtime_error("A finished build for the selected current sources is required. Refresh sources and retry.");
    if(!coverage.contains("Expected")||!coverage["Expected"].is_array()||coverage["Expected"].size()>32768)
        throw std::runtime_error("The full-build path manifest is unavailable. Build the cache again.");
    std::set<std::string> paths;
    for(const auto& path:coverage["Expected"]){
        if(!path.is_string()||!paths.insert(path.get<std::string>()).second)throw std::runtime_error("Invalid full-build path manifest.");
    }
    if(paths.size()!=coverage.value("Total",size_t{}))throw std::runtime_error("Full-build coverage count does not match its path manifest.");
    return paths;
}
inline void RequirePaths(const std::set<std::string>& expected,const std::set<std::string>& actual){
    if(expected!=actual){size_t missing=0;for(const auto& path:expected)missing+=!actual.contains(path);
        throw std::runtime_error("Full export blocked: "+std::to_string(missing)+" required icons are missing or stale. Run Build / update and retry.");}
}
}
