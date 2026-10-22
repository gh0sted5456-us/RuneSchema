#pragma once
#include "Core/SpawnCatalogSearch.h"
#include <set>
#include <map>
#include <algorithm>
#include <stdexcept>
namespace PS::SpawnBrowserPreferences {
// Native class identity survives changing loaded/mounted catalog keys. These
// preferences never remove a definition or modify spawn permission/cache data.
inline std::string Identity(const nlohmann::json& row){
    const auto type=SpawnCatalogSearch::Text(row,"Type");
    if(type!="Resource"&&type!="NPC"&&type!="AI")return {};
    auto path=SpawnCatalogSearch::Text(row,"Class");
    if(path.empty())path=SpawnCatalogSearch::Text(row,"Key");
    if(path.empty()||path.size()>2048||std::any_of(path.begin(),path.end(),[](unsigned char c){return c<32||c==127;}))return {};
    return type+"|"+SpawnCatalogSearch::Fold(path);
}
struct Choices {
    static constexpr size_t Limit=1024;
    std::set<std::string> Hidden;
    struct Browser {std::string Query;bool ShowHidden=false;int Mode=0;bool operator==(const Browser&)const=default;};
    std::map<std::string,Browser> Filters;
    static bool FilterValid(const std::string& kind,const Browser& filter){return (kind=="Resources"||kind=="NPCs"||kind=="Enemies")&&filter.Query.size()<=256
        &&std::none_of(filter.Query.begin(),filter.Query.end(),[](unsigned char c){return c<32||c==127;})&&filter.Mode>=0&&filter.Mode<=(kind=="Resources"?2:0);}
    Browser Filter(const std::string& kind)const{const auto it=Filters.find(kind);return it==Filters.end()?Browser{}:it->second;}
    bool SetFilter(const std::string& kind,const Browser& filter){if(!FilterValid(kind,filter))throw std::runtime_error("Invalid spawn browser filter");if(Filters.contains(kind)&&Filters.at(kind)==filter)return false;Filters[kind]=filter;return true;}
    Choices Merge(const Choices& other)const{auto next=*this;next.Hidden.insert(other.Hidden.begin(),other.Hidden.end());if(next.Hidden.size()>Limit)throw std::runtime_error("Combined preset exceeds 1,024 hidden entries; nothing changed");for(const auto& [kind,filter]:other.Filters)next.Filters[kind]=filter;return next;}
    static bool Valid(const std::string& id){
        return id.size()<=2057&&(id.starts_with("Resource|")||id.starts_with("NPC|")||id.starts_with("AI|"))
            &&id.back()!='|'&&std::none_of(id.begin(),id.end(),[](unsigned char c){return c<32||c==127;});
    }
    bool Contains(const nlohmann::json& row)const{return Hidden.contains(Identity(row));}
    bool Set(const nlohmann::json& row,bool hide){
        const auto id=Identity(row);if(id.empty())return false;
        if(!hide){Hidden.erase(id);return true;}
        if(Hidden.size()>=Limit&&!Hidden.contains(id))return false;
        Hidden.insert(id);return true;
    }
    bool Visible(const nlohmann::json& row,bool showHidden)const{return showHidden||!Contains(row);}
    nlohmann::json Json()const{auto filters=nlohmann::json::object();for(const auto& [kind,filter]:Filters)filters[kind]={{"Query",filter.Query},{"ShowHidden",filter.ShowHidden},{"Mode",filter.Mode}};return {{"Version",2},{"Hidden",Hidden},{"Filters",std::move(filters)}};}
    static Choices Parse(const nlohmann::json& document){
        if(!document.is_object()||!document.contains("Version")||!document["Version"].is_number_integer()||(document["Version"]!=1&&document["Version"]!=2)
            ||!document.contains("Hidden")||!document["Hidden"].is_array()||document["Hidden"].size()>Limit)throw std::runtime_error("Invalid spawn-browser preferences; existing file retained");
        const bool legacy=document["Version"]==1;
        if(document.size()!=(legacy?2:3)||(!legacy&&(!document.contains("Filters")||!document["Filters"].is_object()||document["Filters"].size()>3)))throw std::runtime_error("Preset must contain browser choices only; no placement or profile fields");
        Choices out;for(const auto& id:document["Hidden"]){
            if(!id.is_string()||!Valid(id.get<std::string>()))throw std::runtime_error("Invalid hidden spawn identity; existing file retained");
            const auto value=id.get<std::string>();const auto split=value.find('|');out.Hidden.insert(value.substr(0,split+1)+SpawnCatalogSearch::Fold(value.substr(split+1)));
        }
        if(!legacy)for(const auto& [kind,value]:document.at("Filters").items()){
            if(!value.is_object()||value.size()!=3||!value.at("Query").is_string()||!value.at("ShowHidden").is_boolean()||!value.at("Mode").is_number_integer()||value.at("Mode")<0||value.at("Mode")>2)throw std::runtime_error("Invalid spawn browser filter");
            out.SetFilter(kind,{value.at("Query").get<std::string>(),value.at("ShowHidden").get<bool>(),value.at("Mode").get<int>()});
        }return out;
    }
};
}
