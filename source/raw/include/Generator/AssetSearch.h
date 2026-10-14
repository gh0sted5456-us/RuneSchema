#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <cstddef>
#include <cstdint>
namespace PS::AssetSearch {
nlohmann::json Search(const std::string& query,const std::string& loader="assets");
nlohmann::json Capture(const nlohmann::json& entry,const std::string& loader="assets",bool readValues=true);
nlohmann::json Reference(const nlohmann::json& entry,const std::string& loader);
nlohmann::json ItemRoster();
nlohmann::json ItemMetadata(const std::string& path);
nlohmann::json ItemWindow(const std::string& query,std::size_t offset,std::size_t limit,std::uint64_t startSeed=0,bool randomOrder=false,
    const std::string& fieldSet="BasicRow",std::size_t scanBudget=1536,std::uint64_t timeBudgetMicros=800);
}
