#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <cstdint>
namespace RC::Unreal { class UObject; }
namespace PS::AssetProvenance {
void Record(RC::Unreal::UObject* object,const std::string& source,const std::string& mod,
    bool createdNow,bool registered,int errors,const nlohmann::json& definition);
nlohmann::json Lookup(RC::Unreal::UObject* object);
// Validated paths only; callers must still resolve/type-check before use.
std::vector<std::string> LivePaths();
uint64_t Revision() noexcept;
void Flush();
void Clear();
}
