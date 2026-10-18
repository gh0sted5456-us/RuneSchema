#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <stdexcept>
namespace PS::MapFilters {
using Json=nlohmann::json;
inline constexpr size_t MaxBytes=8*1024*1024,MaxMarkers=50000;
inline constexpr const char* Credit="Markers: RuneScape: Dragonwilds Wiki, via Wildsforge — CC BY-NC-SA 3.0";
inline constexpr const char* SourceUrl="https://wildsforge.org/map/";
inline constexpr const char* LicenseUrl="https://creativecommons.org/licenses/by-nc-sa/3.0/";
inline std::string Fold(std::string s){for(auto& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;}
inline std::string Text(const Json& value,size_t limit=256){
    if(!value.is_string())throw std::runtime_error("Map text has an invalid type");auto text=value.get<std::string>();
    if(text.size()>limit||std::any_of(text.begin(),text.end(),[](unsigned char c){return c<32&&c!='\n'&&c!='\t';}))throw std::runtime_error("Map text exceeds its safe bounds");return text;
}
struct Marker {double X=0,Y=0;size_t Category=0;std::string Name,Note,Search;};
struct Category {std::string Key,Name,Group,GroupLabel;size_t Count=0;};
struct Dataset {std::string Built;std::vector<Category> Categories;std::vector<Marker> Markers;};
inline Dataset Parse(const std::string& bytes){
    if(bytes.empty()||bytes.size()>MaxBytes)throw std::runtime_error("Map dataset exceeds its size limit");
    const auto json=Json::parse(bytes,[](int depth,Json::parse_event_t,Json&){if(depth>16)throw std::runtime_error("Map dataset is too deeply nested");return true;});
    if(!json.is_object()||json.value("source",std::string{})!="RuneScape: Dragonwilds Wiki"||json.value("licence",std::string{})!="CC BY-NC-SA 3.0")throw std::runtime_error("Map source/licence is not recognized");
    Dataset result;result.Built=Text(json.at("built"),10);
    if(result.Built.size()!=10||result.Built[4]!='-'||result.Built[7]!='-'||!std::all_of(result.Built.begin(),result.Built.end(),[](unsigned char c){return std::isdigit(c)||c=='-';}))throw std::runtime_error("Map data date is invalid");
    const auto& groups=json.at("groups");if(!groups.is_array()||groups.empty()||groups.size()>32)throw std::runtime_error("Map groups exceed their bound");
    std::set<std::string> ids;
    for(const auto& group:groups){
        const auto id=Text(group.at("id"),40),label=Text(group.at("label"),128);
        if(id.empty()||!std::all_of(id.begin(),id.end(),[](unsigned char c){return (c>='a'&&c<='z')||c=='-'||c=='_';})||!ids.insert(id).second)throw std::runtime_error("Map group identity is invalid");
        const auto& categories=group.at("categories");if(!categories.is_array()||result.Categories.size()+categories.size()>2048)throw std::runtime_error("Map categories exceed their bound");
        for(const auto& category:categories){
            const auto name=Text(category.at("name"),160),key=id+":"+name;
            if(name.empty()||!ids.insert(key).second)throw std::runtime_error("Map category identity is invalid");
            const auto& points=category.at("points");if(!points.is_array()||result.Markers.size()+points.size()>MaxMarkers)throw std::runtime_error("Map markers exceed their bound");
            const auto index=result.Categories.size();result.Categories.push_back({key,name,id,label,0});std::set<std::pair<double,double>> positions;
            for(const auto& point:points){
                if(!point.is_array()||point.size()<2||point.size()>4||!point[0].is_number()||!point[1].is_number())throw std::runtime_error("Map coordinate pair is invalid");
                const double x=point[0].get<double>(),y=point[1].get<double>();
                if(!std::isfinite(x)||!std::isfinite(y)||std::abs(x)>2000000||std::abs(y)>2000000)throw std::runtime_error("Map coordinates exceed their safe bounds");
                auto title=point.size()>2?Text(point[2],256):name;if(title.empty())title=name;
                const auto note=point.size()>3?Text(point[3],4096):std::string{};
                if(!positions.emplace(x,y).second)continue;
                result.Markers.push_back({x,y,index,title,note,Fold(name+" "+title+" "+label+" "+id)});++result.Categories.back().Count;
            }
        }
    }
    if(result.Markers.empty())throw std::runtime_error("Empty map update rejected; previous cache retained");return result;
}
struct Selection {
    bool ShowCached=true,UpdateAfterGamePatch=true;
    bool Players=true,Lodestones=true,Placements=true,Resources=false,AI=false,Chests=false;
    bool ObserveNearbyResources=true;double ResourceRadiusMetres=200;
    std::set<std::string> Hidden;
    bool Shows(const Category& category)const{return ShowCached&&!Hidden.contains(category.Key);}
    Json ToJson()const{return {{"ShowCached",ShowCached},{"UpdateAfterGamePatch",UpdateAfterGamePatch},{"Hidden",Hidden},{"Players",Players},{"Lodestones",Lodestones},{"Placements",Placements},{"Resources",Resources},{"AI",AI},{"Chests",Chests},{"ObserveNearbyResources",ObserveNearbyResources},{"ResourceRadiusMetres",ResourceRadiusMetres}};}
    static Selection From(const Json& json){Selection result;
        if(!json.is_object())return result;
        if(json.contains("ShowCached")&&json["ShowCached"].is_boolean())result.ShowCached=json["ShowCached"];
        if(json.contains("UpdateAfterGamePatch")&&json["UpdateAfterGamePatch"].is_boolean())result.UpdateAfterGamePatch=json["UpdateAfterGamePatch"];
        const auto read=[&](const char* key,bool& value){if(json.contains(key)&&json[key].is_boolean())value=json[key].get<bool>();};
        read("Players",result.Players);read("Lodestones",result.Lodestones);read("Placements",result.Placements);read("Resources",result.Resources);read("AI",result.AI);
        read("Chests",result.Chests);
        read("ObserveNearbyResources",result.ObserveNearbyResources);if(json.contains("ResourceRadiusMetres")&&json["ResourceRadiusMetres"].is_number()){const double radius=json["ResourceRadiusMetres"].get<double>();if(std::isfinite(radius))result.ResourceRadiusMetres=std::clamp(radius,25.0,2000.0);}
        if(json.contains("Hidden")&&json["Hidden"].is_array()&&json["Hidden"].size()<=2048)for(const auto& key:json["Hidden"])if(key.is_string()&&key.get_ref<const std::string&>().size()<=256)result.Hidden.insert(key.get<std::string>());return result;
    }
};
inline bool MatchesFolded(const std::string& folded,const std::string& needle){
    size_t at=0;
    while(at<needle.size()){while(at<needle.size()&&std::isspace(static_cast<unsigned char>(needle[at])))++at;auto end=at;while(end<needle.size()&&!std::isspace(static_cast<unsigned char>(needle[end])))++end;if(end>at&&folded.find(needle.substr(at,end-at))==std::string::npos)return false;at=end;}return true;
}
inline bool Matches(const std::string& folded,const std::string& query){return MatchesFolded(folded,Fold(query));}
// Runtime-supplied installed CL, not a news post, clock tick, mod fingerprint,
// UE4SS/RuneSchema version or a different storefront's prior cache.
inline std::string ConfirmedBuild(const std::string& gameVersion){
    if(gameVersion.size()>160||!gameVersion.starts_with("++dominion+"))return {};const auto at=gameVersion.rfind("CL-");if(at==std::string::npos)return {};
    auto end=at+3;while(end<gameVersion.size()&&std::isdigit(static_cast<unsigned char>(gameVersion[end])))++end;
    if(end==at+3||end-at>15)return {};return gameVersion.substr(at,end-at);
}
inline bool NeedsUpdate(bool hasCache,const std::string& cached,const std::string& installed,bool enabled){return enabled&&hasCache&&!cached.empty()&&!installed.empty()&&cached!=installed;}
}
