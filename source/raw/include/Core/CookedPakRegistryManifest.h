#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <optional>
#include <set>
#include <nlohmann/json.hpp>
#include "Core/NativeDataFamilies.h"

namespace PS::CookedPakRegistryManifest {
    struct Lane {
        std::string Name;
        std::vector<std::string> Paths;
    };

    struct Manifest {
        std::string Owner;
        std::string Source;
        std::vector<Lane> AssetLanes;
        std::set<std::string> SchemaPaths;
        nlohmann::json LoaderDefinitions=nlohmann::json::object();
        std::vector<std::string> WearableMeshData;
        std::vector<std::string> MeleeAttackClasses;
        std::vector<std::string> RangedAttackClasses;
        std::vector<std::string> CacheAssets; // Exact original artwork declarations; never native gameplay registration.
        std::uint64_t Fingerprint = 0;
    };

    namespace Detail {
        inline std::mutex Mutex;
        inline std::vector<Manifest> Manifests;

        inline bool ValidAssetPath(const std::string& value)
        {
            if (value.empty() || value.size() > 1024 || value.front() != '/'
                || value.find("..") != std::string::npos
                || value.find_first_of("\r\n\t") != std::string::npos)
                return false;
            const auto mountEnd = value.find('/', 1);
            if (mountEnd == std::string::npos || mountEnd == 1) return false;
            const auto mount = value.substr(1, mountEnd - 1);
            if (mount == "Script" || mount == "Engine") return false;
            return std::all_of(mount.begin(), mount.end(), [](unsigned char c) {
                return std::isalnum(c) || c == '_' || c == '-' || c == '.';
            });
        }

        inline void Hash(std::uint64_t& value, std::string_view text)
        {
            constexpr std::uint64_t Prime = 1099511628211ull;
            for (const auto byte : text)
            {
                value ^= static_cast<unsigned char>(byte);
                value *= Prime;
            }
            value ^= 0xff;
            value *= Prime;
        }

        inline std::string Fold(std::string value){for(auto& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return value;}
        inline bool OwnedObjectPath(const std::string& path,const std::string& owner){
            if(owner.empty()||owner.size()>96||!std::all_of(owner.begin(),owner.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.';})
                ||!ValidAssetPath(path)||path.find_first_of("\\: ")!=path.npos
                ||std::any_of(path.begin(),path.end(),[](unsigned char c){return c<33||c>126;}))return false;
            const auto dot=path.find('.',path.find_last_of('/')+1);
            const auto folded=Fold(path),name=Fold(owner);
            if(name=="game"||name=="engine"||name=="script"||name=="runeschema"||name=="fmodel")return false;
            return dot!=path.npos&&dot<path.size()-1&&path.find('.',dot+1)==path.npos
                &&(folded.starts_with("/"+name+"/")||folded.starts_with("/game/mods/"+name+"/"));
        }

        inline std::vector<std::string> ReadPaths(const nlohmann::json& root,
            const char* field, std::size_t& total,std::set<std::string>* schemaPaths=nullptr)
        {
            if (!root.contains(field)) return {};
            const auto& value = root.at(field);
            if (!value.is_array() || value.size() > 4096)
                throw std::runtime_error(std::string("NativeRegistries.") + field
                    + " must be an array with at most 4096 cooked asset paths");
            std::vector<std::string> result;
            result.reserve(value.size());
            for (const auto& row : value)
            {
                std::string path;
                if(row.is_string())path=row.get<std::string>();
                else if(row.is_object()&&row.size()==2&&row.contains("Path")&&row["Path"].is_string()
                    &&row.contains("Source")&&row["Source"]=="Schema"&&schemaPaths){
                    path=row["Path"].get<std::string>();schemaPaths->insert(path);
                }else throw std::runtime_error(std::string("NativeRegistries.")+field+" requires an object path or {Path, Source: Schema} reference");
                if (!ValidAssetPath(path))
                    throw std::runtime_error(std::string("NativeRegistries.") + field
                        + " contains an invalid cooked asset path");
                if (std::ranges::find(result, path) != result.end())
                    throw std::runtime_error(std::string("NativeRegistries.") + field
                        + " contains a duplicate cooked asset path");
                result.push_back(std::move(path));
                if (++total > 16384)
                    throw std::runtime_error(
                        "NativeRegistries exceeds the 16384-path document limit");
            }
            return result;
        }
    }

    inline std::optional<Manifest> Parse(const nlohmann::json& document, const std::string& owner,
        const std::string& source)
    {
        if (!document.is_object() || (!document.contains("NativeRegistries")&&!document.contains("LoaderDefinitions")))
            return std::nullopt;
        const auto native = document.value("NativeRegistries",nlohmann::json::object());
        if (!native.is_object())
            throw std::runtime_error("NativeRegistries must be an object");
        static constexpr std::array<const char*, 15> Allowed{{
            "Items", "Recipes", "Quests", "CombatSpells", "UtilitySpells",
            "Journals", "Lore",
            "EquipmentEffects", "WearableMeshData", "MeleeAttackClasses",
            "RangedAttackClasses", "CacheAssets", "Skills", "SkillPerks", "Enums"}};
        for (const auto& [key, unused] : native.items())
        {
            (void)unused;
            if (std::ranges::find(Allowed, key) == Allowed.end()&&!PS::NativeDataFamilies::Find(key))
                throw std::runtime_error("NativeRegistries contains unsupported lane '"
                    + key + "'");
        }

        Manifest result;
        result.Owner = owner;
        result.Source = source;
        std::size_t total = 0;
        for (const auto* lane : {"Items", "Recipes", "Quests", "CombatSpells",
            "UtilitySpells", "EquipmentEffects", "Journals", "Lore"})
        {
            auto paths = Detail::ReadPaths(native, lane, total,&result.SchemaPaths);
            if (!paths.empty()) result.AssetLanes.push_back({lane, std::move(paths)});
        }
        for(const auto* lane:{"Skills","SkillPerks","Enums"}){
            auto paths=Detail::ReadPaths(native,lane,total);std::set<std::string> unique;
            for(const auto& path:paths){
                if(!Detail::OwnedObjectPath(path,owner))throw std::runtime_error(std::string("NativeRegistries.")+lane+" requires full mod-owned cooked object paths");
                if(!unique.insert(Detail::Fold(path)).second)throw std::runtime_error("Duplicate mod-owned object path (case-insensitive)");
            }
            std::ranges::sort(paths);
            if(!paths.empty())result.AssetLanes.push_back({lane,std::move(paths)});
        }
        for(const auto& family:PS::NativeDataFamilies::Families){
            auto paths=Detail::ReadPaths(native,family.Lane,total);std::set<std::string> unique;
            for(const auto& path:paths){
                const auto dot=path.find('.',path.find_last_of('/')+1);
                if(dot==path.npos||dot==path.size()-1||path.find('.',dot+1)!=path.npos||path.find_first_of("\\: ")!=path.npos
                    ||std::any_of(path.begin(),path.end(),[](unsigned char c){return c<33||c>126;}))throw std::runtime_error(std::string("NativeRegistries.")+family.Lane+" requires exact cooked object paths");
                if(!unique.insert(Detail::Fold(path)).second)throw std::runtime_error("Duplicate cooked object path (case-insensitive)");
            }
            std::ranges::sort(paths);if(!paths.empty())result.AssetLanes.push_back({family.Lane,std::move(paths)});
        }
        result.WearableMeshData = Detail::ReadPaths(
            native, "WearableMeshData", total,&result.SchemaPaths);
        result.MeleeAttackClasses = Detail::ReadPaths(
            native, "MeleeAttackClasses", total);
        result.RangedAttackClasses = Detail::ReadPaths(
            native, "RangedAttackClasses", total);
        result.CacheAssets = Detail::ReadPaths(native,"CacheAssets",total);
        for(const auto& path:result.CacheAssets){
            const auto dot=path.find('.',path.find_last_of('/')+1);
            if(dot==path.npos||dot==path.size()-1||path.find_first_of("\\:")!=path.npos
                ||std::any_of(path.begin(),path.end(),[](unsigned char c){return c<32||c==127;}))
                throw std::runtime_error("NativeRegistries.CacheAssets requires full original artwork object paths");
        }
        if(document.contains("LoaderDefinitions")){
            const auto& definitions=document.at("LoaderDefinitions");
            if(!definitions.is_object())throw std::runtime_error("LoaderDefinitions must be an object");
            for(const auto& [lane,entries]:definitions.items()){
                if(lane!="Recipes"&&lane!="Journals"&&lane!="Lore")throw std::runtime_error("Unsupported cooked loader lane: "+lane);
                if(!entries.is_object()||entries.size()>4096)throw std::runtime_error("Cooked loader lane must be an object with at most 4096 entries");
                for(const auto& [path,body]:entries.items()){
                    if(result.SchemaPaths.contains(path))
                        throw std::runtime_error("LoaderDefinitions edits existing cooked assets; use the owning loose loader for schema-created assets, and NativeRegistries to announce their runtime paths");
                    if(!Detail::ValidAssetPath(path)||path.rfind('.')==std::string::npos||path.rfind('.')<=path.rfind('/')||path.back()=='.'||!body.is_object())
                        throw std::runtime_error("Cooked loader definitions require full object-path keys and object bodies");
                    for(const auto* directive:{"$Patch","$Clone","$Target","VendorID","RuneSchemaVendors","VanillaVendors"})if(body.contains(directive))
                        throw std::runtime_error("Cooked declarations reference existing assets; use loose definitions for patches, clones or vendor offers");
                    if(body.contains("Unlock")&&!body.at("Unlock").is_boolean())throw std::runtime_error("Cooked Unlock must be boolean");
                    if(++total>16384)throw std::runtime_error("Cooked registry exceeds the 16384-reference document limit");
                }
            }
            result.LoaderDefinitions=definitions;
        }
        if (!total)
            throw std::runtime_error("Cooked registry must declare at least one asset or loader definition");

        std::uint64_t hash = 14695981039346656037ull;
        Detail::Hash(hash, owner);
        for (const auto& lane : result.AssetLanes)
        {
            Detail::Hash(hash, lane.Name);
            for (const auto& path : lane.Paths) Detail::Hash(hash, path);
        }
        Detail::Hash(hash, "WearableMeshData");
        for (const auto& path : result.WearableMeshData) Detail::Hash(hash, path);
        Detail::Hash(hash, "MeleeAttackClasses");
        for (const auto& path : result.MeleeAttackClasses) Detail::Hash(hash, path);
        Detail::Hash(hash, "RangedAttackClasses");
        for (const auto& path : result.RangedAttackClasses) Detail::Hash(hash, path);
        Detail::Hash(hash,"CacheAssets");for(const auto& path:result.CacheAssets)Detail::Hash(hash,path);
        Detail::Hash(hash,result.LoaderDefinitions.dump());
        if(!result.SchemaPaths.empty()){Detail::Hash(hash,"SchemaReferences");for(const auto& path:result.SchemaPaths)Detail::Hash(hash,path);}
        result.Fingerprint = hash;
        return result;
    }

    inline void Upsert(std::vector<Manifest>& manifests,const Manifest& result) {
        auto existing = std::ranges::find_if(manifests,
            [&](const Manifest& value) {return value.Owner==result.Owner&&value.Source==result.Source;});
        if(existing==manifests.end())manifests.push_back(result);else *existing=result;
    }
    inline bool HasPublishedDeclarations(){std::scoped_lock lock(Detail::Mutex);return !Detail::Manifests.empty();}
    inline std::vector<Manifest> Snapshot()
    {
        std::scoped_lock lock(Detail::Mutex);
        return Detail::Manifests;
    }

    struct Definition {std::string Owner,Source,Path;nlohmann::json Body;bool Explicit=false;};
    inline std::vector<Definition> MergeDefinitions(const std::vector<Manifest>& manifests,const std::string& lane) {
        if(lane!="Recipes"&&lane!="Journals"&&lane!="Lore")throw std::runtime_error("Unsupported cooked definition lane");
        std::map<std::string,Definition> merged;
        const auto add=[&](Definition value){
            if(!value.Body.contains("Unlock"))value.Body["Unlock"]=false;
            auto [entry,inserted]=merged.emplace(value.Path,value);
            if(inserted)return;
            if(value.Explicit&&!entry->second.Explicit){entry->second=std::move(value);return;}
            if(!value.Explicit)return;
            if(entry->second.Body!=value.Body)throw std::runtime_error("Conflicting cooked "+lane+" declaration: "+value.Path);
        };
        for(const auto& manifest:manifests){
            for(const auto& assets:manifest.AssetLanes)if(assets.Name==lane)for(const auto& path:assets.Paths){
                // A loose /recipes or /journal loader owns construction and
                // policy for its runtime asset. Do not synthesize a second,
                // cooked-style definition before that asset exists.
                if(manifest.SchemaPaths.contains(path))continue;
                add({manifest.Owner,manifest.Source,path,nlohmann::json::object(),false});
            }
            if(manifest.LoaderDefinitions.contains(lane))for(const auto& [path,body]:manifest.LoaderDefinitions.at(lane).items())
                add({manifest.Owner,manifest.Source,path,body,true});
        }
        // One native journal asset must not receive competing policies from
        // the journal and lore services during the same initialization.
        if(lane=="Journals"||lane=="Lore"){
            const std::string other=lane=="Journals"?"Lore":"Journals";
            for(const auto& manifest:manifests){
                for(const auto& assets:manifest.AssetLanes)if(assets.Name==other)for(const auto& path:assets.Paths)
                    if(merged.contains(path))throw std::runtime_error("Cooked journal path declared in both Journals and Lore: "+path);
                if(manifest.LoaderDefinitions.contains(other))for(const auto& [path,body]:manifest.LoaderDefinitions.at(other).items())
                    if(merged.contains(path))throw std::runtime_error("Cooked journal path declared in both Journals and Lore: "+path);
            }
        }
        std::vector<Definition> result;for(auto& [path,value]:merged)result.push_back(std::move(value));return result;
    }
    inline std::vector<Definition> Definitions(const std::string& lane) {return MergeDefinitions(Snapshot(),lane);}

    // Declaration publication only: every native class/reference still has to
    // pass the normal registrar/loader checks before it can authorize cleanup.
    inline void PublishBatch(const std::vector<Manifest>& batch) {
        std::scoped_lock lock(Detail::Mutex);
        auto next=Detail::Manifests;
        for(const auto& manifest:batch)Upsert(next,manifest);
        std::map<std::string,std::pair<std::string,std::string>> ownership;
        for(const auto& manifest:next)for(const auto& lane:manifest.AssetLanes)if(lane.Name=="Skills"||lane.Name=="SkillPerks"||lane.Name=="Enums")
            for(const auto& path:lane.Paths){const auto claim=std::pair{Detail::Fold(manifest.Owner),lane.Name};
                const auto [found,inserted]=ownership.emplace(Detail::Fold(path),claim);
                if(!inserted&&found->second!=claim)throw std::runtime_error("Conflicting owner or native lane for cooked object: "+path);
            }
        for(const auto* lane:{"Recipes","Journals","Lore"})(void)MergeDefinitions(next,lane);
        Detail::Manifests.swap(next);
    }
    inline bool Publish(const nlohmann::json& document, const std::string& owner,
        const std::string& source, Manifest* published = nullptr)
    {
        const auto result=Parse(document,owner,source);
        if(!result)return false;
        // Loose and single cooked documents use the same all-or-nothing
        // conflict validation as sharded declarations.
        PublishBatch({*result});
        if(published)*published=*result;
        return true;
    }

    inline void Reset()
    {
        std::scoped_lock lock(Detail::Mutex);
        Detail::Manifests.clear();
    }
}
