#pragma once
#include <nlohmann/json.hpp>
#include <string>
namespace PS::AssetSearch {
nlohmann::json Search(const std::string& query,const std::string& loader="assets");
nlohmann::json Capture(const nlohmann::json& entry,const std::string& loader="assets",bool readValues=true);
nlohmann::json Reference(const nlohmann::json& entry,const std::string& loader);
nlohmann::json ItemRoster();
}
