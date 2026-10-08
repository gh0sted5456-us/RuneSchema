#pragma once
#include <nlohmann/json.hpp>
#include <string>
namespace RC::Unreal { class UObject; }
namespace PS::AssetProvenance {
void Record(RC::Unreal::UObject* object,const std::string& source,const std::string& mod,
    bool createdNow,bool registered,int errors,const nlohmann::json& definition);
nlohmann::json Lookup(RC::Unreal::UObject* object);
void Flush();
void Clear();
}
