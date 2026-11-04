#pragma once
#include "Core/CacheBundleIO.h"
#include "Core/ItemCategory.h"
#include "Core/HelpyCacheSources.h"
#include "Core/ContentSource.h"
#include "Core/ItemFamily.h"
#include <nlohmann/json.hpp>
#include <map>

namespace PS::ItemGroupings {
using Json=nlohmann::json;
struct Rule {std::string Category,Family,Owner,Path;};
using Plan=std::map<std::string,Rule>;
inline std::string Key(const std::string& path,const std::string& owner){return HelpyCacheSources::Lower(path)+"|"+owner;}
inline Plan Parse(const Json& document){
    if(!document.is_object())throw std::runtime_error("Taglines must be an object.");Plan result;
    const auto add=[&](const std::string& path,const Json& value){
        if(path.empty()||path.front()!='/'||path.size()>2048||path.find("..")!=path.npos||path.find_first_of("\\:*?\r\n\t")!=path.npos||std::any_of(path.begin(),path.end(),[](unsigned char c){return c<32||c==127;})||!value.is_object())throw std::runtime_error("Grouping keys must be exact Unreal item paths.");
        Rule rule{value.value("Category",std::string{}),value.value("Family",std::string{}),HelpyCacheSources::Lower(value.value("Owner",std::string{})),path};
        if(!rule.Owner.empty()&&!ContentSource::Valid(rule.Owner))throw std::runtime_error("Invalid tagline source owner.");
        if(!rule.Category.empty()&&(rule.Category=="All"||std::none_of(std::begin(ItemCategory::Categories),std::end(ItemCategory::Categories),[&](const auto* category){return rule.Category==category;})))throw std::runtime_error("Unknown item grouping category.");
        if(rule.Family.size()>192||rule.Family.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_")!=rule.Family.npos)throw std::runtime_error("Family keys use lowercase letters, digits and underscores.");
        const auto key=Key(path,rule.Owner);if(!result.emplace(key,std::move(rule)).second)throw std::runtime_error("Duplicate tagline path and owner.");
    };
    if(document.contains("Rules")){if(!document["Rules"].is_array()||document["Rules"].size()>65536)throw std::runtime_error("Invalid taglines Rules array.");for(const auto& rule:document["Rules"])add(rule.at("Path").get<std::string>(),rule);}
    else if(document.contains("Items")&&document["Items"].is_object()&&document["Items"].size()<=65536){for(const auto& [path,rule]:document["Items"].items())add(path,rule);}
    else throw std::runtime_error("Taglines need a Rules array or an Items object.");return result;
}
inline Json Document(const Plan& plan){auto rules=Json::array();for(const auto& [key,rule]:plan)rules.push_back({{"Path",rule.Path},{"Owner",rule.Owner},{"Category",rule.Category},{"Family",rule.Family}});return {{"Format",1},{"Rules",std::move(rules)}};}
inline Plan Read(const std::filesystem::path& path){return Parse(Json::parse(CacheBundleIO::Read(path,64*1024*1024),[](int depth,Json::parse_event_t,Json&){if(depth>16)throw std::runtime_error("Taglines nesting is too deep.");return true;},true,true));}
inline Plan Bound(Plan plan,const std::string& owner){if(owner.empty())return plan;Plan result;for(auto& [key,rule]:plan){if(!rule.Owner.empty()&&rule.Owner!=owner)throw std::runtime_error("Taglines escape their mod owner.");rule.Owner=owner;result.emplace(Key(rule.Path,owner),std::move(rule));}return result;}
inline void Apply(Json& row,const Plan& plan,const std::string& owner={}){
    const auto path=row.find("Path");if(path==row.end()||!path->is_string())return;auto found=plan.find(Key(path->get_ref<const std::string&>(),owner));if(found==plan.end())found=plan.find(Key(path->get_ref<const std::string&>(),{}));if(found==plan.end())return;
    row["HelpyCategory"]=found->second.Category;row["HelpyFamily"]=found->second.Family;
}
inline void Merge(Plan& destination,const Plan& incoming){for(const auto& [key,rule]:incoming)destination[key]=rule;}
inline Plan Generate(const Json& records,const Plan& edits={}){Plan result=edits;for(const auto& record:records)if(record.value("Kind",std::string{})=="items"){
    auto row=record.at("Data");const auto owner=record.at("Owner").get<std::string>(),path=record.at("Path").get<std::string>();row["Path"]=path;Apply(row,edits,owner);Rule rule{ItemCategory::Resolve(row),ItemFamily::RowKey(row),owner,path};result[Key(path,owner)]=std::move(rule);
}return result;}
inline void Import(const std::filesystem::path& cache,const Plan& incoming){const auto file=cache/"taglines.json";CacheBundleIO::MetadataLock lock(file);CacheBundleIO::Safe(file);auto plan=std::filesystem::is_regular_file(file)?Read(file):Plan{};Merge(plan,incoming);const auto document=Document(plan).dump();if(document.size()>64*1024*1024)throw std::runtime_error("Taglines exceed cache metadata bounds.");CacheBundleIO::Publish(file,document);}
inline std::vector<std::pair<std::filesystem::path,std::string>> Files(const std::filesystem::path& cache,const std::filesystem::path& root){
    std::vector<std::pair<std::filesystem::path,std::string>> result{{cache/"taglines.json",{}}};
    const auto mods=root/"mods";CacheBundleIO::Safe(mods);if(std::filesystem::is_directory(mods))for(const auto& mod:std::filesystem::directory_iterator(mods)){
        CacheBundleIO::Safe(mod.path());if(!mod.is_directory())continue;if(result.size()>2048)throw std::runtime_error("Too many installed grouping sources.");result.emplace_back(mod.path()/"cache/taglines.json","mods/"+HelpyCacheSources::Lower(mod.path().filename().string()));
    }
    result.emplace_back(root/"plugins/RuneSchema.Helpy/settings/item-groupings.json",std::string{});return result;
}
inline Plan Load(const std::filesystem::path& cache,const std::filesystem::path& root){Plan result;for(const auto& [file,owner]:Files(cache,root))if(std::filesystem::is_regular_file(file)){
    const auto incoming=Bound(Read(file),owner);if(file.filename()=="item-groupings.json")for(const auto& [key,rule]:incoming)if(rule.Owner.empty())for(auto& [existingKey,existing]:result)if(HelpyCacheSources::Lower(existing.Path)==HelpyCacheSources::Lower(rule.Path)){existing.Category=rule.Category;existing.Family=rule.Family;}
    Merge(result,incoming);
}return result;}
}
