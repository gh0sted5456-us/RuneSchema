#pragma once
#include "Loader/AssetClonePolicy.h"
namespace RC::Unreal {class UObject;}
namespace PS::AssetMetadata {
// Game-thread records are keyed by a validated weak object identity. Cache JSON
// never feeds this store. A declaration only describes the definition that supplied it.
void Record(RC::Unreal::UObject*,const Declaration&,const std::string& owner,bool installedDefinition);
Declaration Lookup(RC::Unreal::UObject*);
bool HasInstalledDefinition(RC::Unreal::UObject*);
bool IsManaged(RC::Unreal::UObject*);
void MarkIncomplete(RC::Unreal::UObject*) noexcept;
bool IsIncomplete(RC::Unreal::UObject*);
void Forget(RC::Unreal::UObject*);
void Clear();
}
