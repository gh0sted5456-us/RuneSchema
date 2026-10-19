#pragma once
#include "Core/ContentSource.h"
#include "Core/SourceBanner.h"
#include <nlohmann/json.hpp>
#include <map>
#include <set>
#include <cmath>
#include <vector>
#include <string>
#include <string_view>
#include <stdexcept>
namespace PS::CacheBundle {
using Json=nlohmann::json;
inline constexpr size_t MaxRecords=65536,MaxMetadata=64*1024*1024;
inline bool Kind(std::string_view kind){return kind=="items"||kind=="journal"||kind=="art"||kind=="portraits"||kind=="resources";}
inline bool Asset(const std::string& path){const auto lower=HelpyCacheSources::Lower(path);return !ContentSource::Root(path).empty()&&path.size()<=2048&&path.find('.')!=path.npos&&!HelpyCacheSources::AssetKey(path).empty()&&!lower.starts_with("/engine/transient/")&&!lower.starts_with("/temp/");}
inline bool Text(const Json& value,size_t limit){if(!value.is_string())return false;const auto& text=value.get_ref<const std::string&>();return text.size()<=limit&&text.find('\0')==text.npos;}
struct Record {std::string Kind,Owner,Source,Path,PersistenceID;Json Data;};
inline Record Parse(const Json& value){
    if(!value.is_object())throw std::runtime_error("[RS-CACHE-002] Invalid bundle record.");
    Record row;row.Kind=value.at("Kind").get<std::string>();row.Owner=value.at("Owner").get<std::string>();row.Path=value.at("Path").get<std::string>();
    row.Source=HelpyCacheSources::Source(row.Path);row.PersistenceID=value.value("PersistenceID",std::string{});
    if(!Kind(row.Kind)||!ContentSource::Valid(row.Owner)||!Asset(row.Path)||!Text(Json(row.PersistenceID),256)||value.value("Source",row.Source)!=row.Source)
        throw std::runtime_error("[RS-CACHE-002] Invalid category, owner, asset path or persistence ID.");
    const auto& data=value.at("Data");if(!data.is_object()||!data.contains("Name")||!Text(data.at("Name"),4096)||data.dump().size()>(row.Kind=="art"&&data.contains("SourceBannerPixels")?524288:row.Kind=="journal"?262144:32768))
        throw std::runtime_error("[RS-CACHE-002] Invalid cached presentation data.");
    row.Data={{"Path",row.Path},{"Name",data.at("Name")},{"PersistenceID",row.PersistenceID},{"PersistenceIDKnown",!row.PersistenceID.empty()},{"ObservedCurrent",false},{"BundleReference",true}};
    for(const char* field:{"Icon","Description","Type","Category","Section","InternalName","Tags"}){
        if(!data.contains(field))continue;
        if(!Text(data[field],std::string_view(field)=="Description"?65536:2048))throw std::runtime_error("[RS-CACHE-002] Invalid presentation text.");
        if(std::string_view(field)=="Icon"&&!data[field].get_ref<const std::string&>().empty()&&!Asset(data[field].get<std::string>()))throw std::runtime_error("[RS-CACHE-002] Invalid artwork path.");
        row.Data[field]=data[field];
    }
    for(const char* field:{"PowerLevel","MaxStackSize","StackSize"})if(data.contains(field)){
        if(!data[field].is_number())throw std::runtime_error("[RS-CACHE-002] Invalid presentation number.");
        const auto number=data[field].get<double>();if(!std::isfinite(number)||number<0||number>1e9)throw std::runtime_error("[RS-CACHE-002] Invalid presentation number.");row.Data[field]=data[field];
    }
    if(data.contains("Pages")){
        if(row.Kind!="journal"||!data["Pages"].is_array()||data["Pages"].size()>128)throw std::runtime_error("[RS-CACHE-002] Invalid journal pages.");
        for(const auto& page:data["Pages"])if(!Text(page,65536))throw std::runtime_error("[RS-CACHE-002] Invalid journal page.");
        row.Data["Pages"]=data["Pages"];
    }
    if(data.contains("SourceBannerPixels")){
        if(row.Kind!="art"||row.PersistenceID!="")throw std::runtime_error("[RS-CACHE-002] Source banners must be inert art records.");
        (void)SourceBanner::Decode(data["SourceBannerPixels"]);row.Data["SourceBannerPixels"]=data["SourceBannerPixels"];
    }
    // Never deserialize runtime handles, read/unlock flags, grant/equip commands,
    // arbitrary definitions or claimed live availability from a shared bundle.
    row.Data["CacheOwner"]=row.Owner;
    row.Data["SourceKind"]=row.Owner=="core"?"vanilla":row.Owner.starts_with("plugins/")?"expansion":"modded";
    row.Data["SourceName"]=row.Owner=="core"?"Dragonwilds":row.Owner.substr(row.Owner.find('/')+1);
    row.Data["SourceEvidence"]="Bundle presentation reference; runtime ownership is checked separately";
    if(row.Kind=="journal")row.Data["Native"]=true;
    return row;
}
inline Json Document(const Record& row){return {{"Kind",row.Kind},{"Owner",row.Owner},{"Source",row.Source},{"Path",row.Path},{"PersistenceID",row.PersistenceID},{"Data",row.Data}};}
inline std::vector<Record> Records(const Json& records){
    if(!records.is_array()||records.size()>MaxRecords||records.dump().size()>MaxMetadata)throw std::runtime_error("[RS-CACHE-002] Bundle metadata exceeds its bound.");
    std::vector<Record> result;std::set<std::string> unique;
    for(const auto& value:records){auto row=Parse(value);if(!unique.insert(row.Owner+"|"+row.Kind+"|"+row.Path).second)throw std::runtime_error("[RS-CACHE-002] Duplicate bundle identity.");result.push_back(std::move(row));}return result;
}
inline bool Owned(const Record& row,const HelpyCacheSources::Plan& plan,const std::set<std::string>& nativePlugins){
    const auto root=HelpyCacheSources::Lower(ContentSource::Root(row.Path));
    if(row.Owner=="core")return (root=="/game"||root=="/engine")&&row.Source.find('/')==row.Source.npos;
    if(row.Owner.starts_with("plugins/"))return nativePlugins.contains(row.Source)&&row.Owner=="plugins/"+row.Source;
    const auto owner=row.Owner.substr(5);
    // A declared dependency on /Game or an official plugin is not item ownership.
    if(root=="/game"||root=="/engine"||nativePlugins.contains(row.Source)){
        // An authored patch of a vanilla asset is not proof that the mod created
        // it. Without a dedicated mod namespace, keep the native owner.
        return false;
    }
    const auto declared=plan.DeclaredOwners.find(row.Source);
    // Shared mount points (e.g. Dye + trims) route to the named active owner.
    // This permits reference storage, not an original-provider badge.
    return declared!=plan.DeclaredOwners.end()&&declared->second.contains(owner);
}
inline bool OriginalOwnerProven(const Record& row,const HelpyCacheSources::Plan& plan,const std::set<std::string>& nativePlugins){
    if(!Owned(row,plan,nativePlugins))return false;if(!row.Owner.starts_with("mods/"))return true;
    const auto owner=row.Owner.substr(5),key=HelpyCacheSources::AssetKey(row.Path);
    for(const auto& evidence:{plan.AssetOwners,plan.AuthoredAssets})if(evidence){const auto found=evidence->find(key);if(found!=evidence->end())return found->second.size()==1&&found->second.contains(owner);}
    const auto found=plan.DeclaredOwners.find(row.Source);return found!=plan.DeclaredOwners.end()&&found->second.size()==1;
}
inline std::string Route(const Record& row){return row.Owner+"|"+row.Kind+"|"+row.Source;}
inline std::string Owner(const Json& row){
    const auto kind=row.value("SourceKind",std::string{}),name=row.value("SourceName",std::string{});
    if(kind=="vanilla")return "core";
    if(kind=="expansion"){const auto group=HelpyCacheSources::Source(row.value("Path",std::string{}));return group.empty()?std::string{}:"plugins/"+group;}
    if(kind=="modded")return ContentSource::Mod(name);
    return {};
}
inline Json FromRow(const std::string& kind,const Json& data){
    const auto owner=Owner(data);if(owner.empty())return nullptr;
    try{
        auto presentation=data;
        // Native catalogues use -1 for an unknown/not-applicable power level.
        // Omit that optional reading, never the item's stable path and ID.
        if(presentation.contains("PowerLevel")&&presentation["PowerLevel"].is_number()&&presentation["PowerLevel"].get<double>()==-1)
            presentation.erase("PowerLevel");
        return Document(Parse({{"Kind",kind},{"Owner",owner},{"Path",data.at("Path")},{"PersistenceID",data.value("PersistenceID",std::string{})},{"Data",presentation}}));
    }catch(...){return nullptr;}
}
}

