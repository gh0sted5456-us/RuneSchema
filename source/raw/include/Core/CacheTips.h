#pragma once
#include "Core/CacheBundleIO.h"
#include "Core/ContentSource.h"
#include <nlohmann/json.hpp>
#include <map>
#include <algorithm>

namespace PS::CacheTips {
using Json=nlohmann::json;using Plan=std::map<std::string,Json>;
inline Plan Parse(const Json& document,const std::string& owner={}){
    const auto& tips=document.is_array()?document:document.at("Tips");if(!tips.is_array()||tips.size()>8192)throw std::runtime_error("Invalid cache tips array.");Plan result;
    for(const auto& tip:tips){const auto text=tip.is_string()?tip.get<std::string>():tip.at("Text").get<std::string>();const auto source=tip.is_string()?owner:tip.value("Owner",owner);
        if(!ContentSource::Valid(source)||(!owner.empty()&&source!=owner)||text.empty()||text.size()>512||std::any_of(text.begin(),text.end(),[](unsigned char c){return c<32||c==127;}))throw std::runtime_error("Invalid tip text or source owner.");
        result[source+"|"+text]={{"Owner",source},{"Text",text}};
    }return result;
}
inline Json Document(const Plan& plan){auto tips=Json::array();for(const auto& [key,tip]:plan)tips.push_back(tip);return {{"Format",1},{"Tips",std::move(tips)}};}
inline Plan Read(const std::filesystem::path& file,const std::string& owner={}){return Parse(Json::parse(CacheBundleIO::Read(file,8*1024*1024),[](int depth,Json::parse_event_t,Json&){if(depth>8)throw std::runtime_error("Cache tips nesting is too deep.");return true;},true,true),owner);}
inline void Merge(Plan& into,const Plan& from){into.insert(from.begin(),from.end());}
inline void Import(const std::filesystem::path& cache,const Plan& incoming){const auto file=cache/"tips.json";CacheBundleIO::MetadataLock lock(file);auto tips=std::filesystem::is_regular_file(file)?Read(file):Plan{};Merge(tips,incoming);CacheBundleIO::Publish(file,Document(tips).dump());}
inline void Annotate(Json& rows,const Plan& tips){for(auto& row:rows){if(!row.value("Enabled",false)||!row.value("Present",false))continue;auto name=row.at("Name").get<std::string>();std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});const auto owner="mods/"+name;auto& entries=row["Metadata"]["tips"];if(!entries.is_array())entries=Json::array();for(const auto& [key,tip]:tips)if(tip.at("Owner")==owner&&std::find(entries.begin(),entries.end(),tip.at("Text"))==entries.end())entries.push_back(tip.at("Text"));}}
}
