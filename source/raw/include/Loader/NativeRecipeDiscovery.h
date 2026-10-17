#pragma once
#include "Loader/RecipeDiscovery.h"
#include <set>
#include <string>
#include <vector>
namespace RC::Unreal {class UObject;class UClass;}
namespace DragonWilds::NativeRecipeDiscovery {
struct Request {RC::Unreal::UObject* Recipe;PS::RecipeDiscovery::Rule Rule;};
struct Result {std::size_t Added=0,Detached=0;};
Result Apply(const std::vector<Request>& requests,RC::Unreal::UClass* progressClass,std::set<std::string>& ownedRows);
}
