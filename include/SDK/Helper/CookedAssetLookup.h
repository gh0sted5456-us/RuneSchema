#pragma once
// Copied registry metadata only: no retained FAssetData buffers or UObject owners.
// Refresh/Build are game-thread operations. Readers receive value snapshots.
#include "Generator/ClonePresentation.h"
#include "Loader/AssetProvenance.h"
#include "Unreal/UObject.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/UnrealFlags.hpp"
#include "Unreal/UAssetRegistry.hpp"
#include "Unreal/UAssetRegistryHelpers.hpp"
#include "Unreal/FAssetData.hpp"
#include "Helpers/String.hpp"
#include <algorithm>
#include <mutex>
#include <set>
#include <string>
#include <vector>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace PS::CookedAssets {
struct Record {std::string path,name,type;};
inline std::mutex Mutex;
inline bool Ready=false;
inline std::set<std::string> Paths,ClassPaths;
inline std::vector<Record> Visuals;
inline std::size_t Skipped=0;
template<class Text> std::string NativeString(const Text& value) {
    if constexpr(requires {RC::to_string(value);})return RC::to_string(value);
    else if constexpr(requires {value.GetCharArray();}) {
        const auto& chars=value.GetCharArray();
        if(chars.Num()>0&&chars.Num()<4096&&chars.GetData()&&chars.GetData()[chars.Num()-1]==0)
            return RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1));
    }
    return {};
}
template<class Asset> std::string AssetClass(Asset& asset) {
    if constexpr(requires {asset.AssetClassPath().GetAssetName().ToString();})return NativeString(asset.AssetClassPath().GetAssetName().ToString());
    else if constexpr(requires {asset.AssetClassPath().ToString();})return NativeString(asset.AssetClassPath().ToString());
    else if constexpr(requires {asset.AssetClass().ToString();})return NativeString(asset.AssetClass().ToString());
    else return {};
}
inline void Invalidate() {std::scoped_lock lock(Mutex);Ready=false;Paths.clear();ClassPaths.clear();Visuals.clear();Skipped=0;}
inline void Build(RC::Unreal::TArray<RC::Unreal::FAssetData>& assets) {
    std::set<std::string> paths,classPaths;std::vector<Record> visuals;std::size_t skipped=0;
    if(assets.Num()<0||assets.Num()>262144)throw std::runtime_error("Mounted asset registry exceeds the authoring safety limit");
    for(auto& asset:assets) {
        try {
            const auto package=NativeString(asset.PackageName().ToString()),name=NativeString(asset.AssetName().ToString());
            if(!package.starts_with('/')||package.size()>2048||name.empty()||name.size()>512){++skipped;continue;}
            const auto path=package+"."+name;if(!paths.insert(path).second)continue;
            auto type=AssetClass(asset);
            const auto leaf=ClonePresentation::ClassLeaf(type);
            if(leaf.find("Blueprint")!=std::string::npos||leaf=="Class")classPaths.insert(path.ends_with("_C")?path:path+"_C");
            if(ClonePresentation::VisualAssetType(type))visuals.push_back({path,name,std::move(type)});
        }catch(...) {++skipped;}
    }
    std::sort(visuals.begin(),visuals.end(),[](const auto& a,const auto& b){return a.path<b.path;});
    std::scoped_lock lock(Mutex);Paths=std::move(paths);ClassPaths=std::move(classPaths);Visuals=std::move(visuals);Skipped=skipped;Ready=true;
}
inline bool Contains(const std::string& path) {std::scoped_lock lock(Mutex);return Ready&&Paths.contains(path);}
inline bool ContainsReference(const std::string& path) {std::scoped_lock lock(Mutex);return Ready&&(Paths.contains(path)||ClassPaths.contains(path));}
inline bool Available() {std::scoped_lock lock(Mutex);return Ready;}
inline void Refresh() {
    using namespace RC::Unreal;
    if(!bFAssetDataAvailable){Invalidate();throw std::runtime_error("Cooked-asset registry metadata is unavailable; only verified loaded-package assets are eligible");}
    auto interface=UAssetRegistryHelpers::GetAssetRegistry();auto* registry=static_cast<UAssetRegistry*>(interface.ObjectPointer);
    TArray<FAssetData> assets;
    if(!registry||!registry->GetAllAssets(assets,true)){Invalidate();throw std::runtime_error("Cannot verify cooked assets against the mounted registry");}
    Build(assets);
}
inline void Ensure() {if(!Available())Refresh();}
inline bool TryEnsure() {
    if(Available())return true;
    try {Refresh();return true;}catch(const std::exception&) {return false;}
}
// Game-thread only. A missing FAssetData layout must not label every *loaded*
// disk asset as a runtime clone. Never infer installed provenance from /Game alone.
// Known clone records and incomplete/transient/template objects are always rejected.
template<class Pointer> inline auto* RawObjectPointer(Pointer pointer) {
    if constexpr(requires {pointer.Get();})return pointer.Get();
    else return pointer;
}
inline bool VerifiedLoadedObject(RC::Unreal::UObject* object) {
    using namespace RC::Unreal;
    if(!object||!object->GetClassPrivate()||object->HasAnyFlags(static_cast<EObjectFlags>(
        RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject|RF_DefaultSubObject|
        RF_NeedInitialization|RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|
        RF_BeginDestroyed|RF_FinishDestroyed)))return false;
    const auto provenance=AssetProvenance::Lookup(object);
    if(provenance.is_object()&&provenance.value("Kind",std::string{})=="RuneSchemaAssetClone"
        &&provenance.value("Confirmed",false))return false;
    const auto path=RC::to_string(object->GetPathName());
    if(Available())return ContainsReference(path);
    auto* package=RawObjectPointer(object->GetOuterPrivate());
    if(!package||!package->GetClassPrivate()||RC::to_string(package->GetClassPrivate()->GetName())!="Package"
        ||package->HasAnyFlags(static_cast<EObjectFlags>(RF_Transient|RF_BeginDestroyed|RF_FinishDestroyed)))return false;
    const auto packagePath=RC::to_string(package->GetPathName());
    if(!packagePath.starts_with('/')||packagePath.starts_with("/Script/")||packagePath.starts_with("/Temp/")
        ||packagePath.starts_with("/Engine/Transient")||packagePath.find('.')!=std::string::npos
        ||packagePath.find(':')!=std::string::npos)return false;
    if(path!=packagePath+"."+RC::to_string(object->GetName()))return false;
    // RF_WasLoaded is serialized-load evidence; RF_LoadCompleted is deprecated.
    // This is a loaded-package fallback, not a complete mounted-asset census.
    return object->HasAnyFlags(RF_WasLoaded)&&object->HasAnyFlags(RF_Public);
}
inline nlohmann::json VisualRoster() {
    std::scoped_lock lock(Mutex);auto rows=nlohmann::json::array();
    for(const auto& r:Visuals)rows.push_back({{"Path",r.path},{"Name",r.name},{"Class",r.type}});
    return rows;
}
} // namespace PS::CookedAssets
