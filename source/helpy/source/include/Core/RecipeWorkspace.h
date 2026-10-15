#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <string>
#include <cctype>
namespace PS::RecipeWorkspace {
using Json=nlohmann::json;
inline void CheckRows(const Json& rows,const char* label,bool allowEmpty=false) {
    if(!rows.is_array()||(!allowEmpty&&rows.empty())||rows.size()>32)throw std::runtime_error(std::string(label)+(allowEmpty?" requires 0..32 items":" requires 1..32 items"));
    std::set<std::string> seen;
    for(const auto& row:rows) {
        if(!row.is_object()||!row.contains("ItemData")||!row["ItemData"].is_string()||!row.contains("Count")||!row["Count"].is_number_integer())
            throw std::runtime_error(std::string(label)+" must contain ItemData and integer Count");
        const auto path=row["ItemData"].get<std::string>();
        if(path.empty()||path.size()>1024||path.front()!='/'||path.find_first_of("\r\n\t")!=path.npos||!seen.insert(path).second
            ||row["Count"]<1||row["Count"]>10000)throw std::runtime_error(std::string(label)+" contains an invalid or duplicate item");
    }
}
inline Json Properties(const Json& options,const std::string& legacyOutput={}) {
    const auto inputs=options.at("Ingredients");
    const auto outputs=options.contains("Outputs")?options.at("Outputs"):Json::array({{{"ItemData",legacyOutput},{"Count",options.value("Count",1)}}});
    CheckRows(inputs,"Inputs",true);CheckRows(outputs,"Outputs");
    Json result={{"ItemsConsumed",inputs},{"ItemsCreated",outputs}};
    for(const auto* field:{"MinProcessingTime","MaxProcessingTime"})if(options.contains(field)) {
        const auto& value=options.at(field);
        if(!value.is_number()||!std::isfinite(value.get<double>())||value<0||value>86400)
            throw std::runtime_error("Processing duration must be 0..86400 seconds");
        result[field]=value;
    }
    if(result.contains("MinProcessingTime")!=result.contains("MaxProcessingTime"))throw std::runtime_error("Supply both processing duration bounds");
    if(result.contains("MinProcessingTime")&&result["MinProcessingTime"]>result["MaxProcessingTime"])
        throw std::runtime_error("Minimum processing duration exceeds maximum");
    return result;
}
inline void AddItem(Json& rows,const Json& item) {
    const auto path=item.value("Path",std::string{});
    Json incoming={{"ItemData",path},{"Count",item.contains("Count")?item["Count"]:Json(1)}};
    CheckRows(Json::array({incoming}),"Item");
    if(!rows.is_array())rows=Json::array();
    for(auto& row:rows)if(row.value("ItemData",std::string{})==path){row["Count"]=std::min(10000,row.value("Count",1)+incoming["Count"].get<int>());return;}
    if(rows.size()>=32)throw std::runtime_error("A recipe side supports at most 32 distinct items");
    rows.push_back(std::move(incoming));
}
inline void ReplaceItem(Json& rows,std::size_t index,const std::string& path) {
    CheckRows(rows,"Recipe side",true);if(index>=rows.size())throw std::runtime_error("Recipe row no longer exists");
    auto replacement=rows;replacement[index]["ItemData"]=path;CheckRows(replacement,"Recipe side",true);rows=std::move(replacement);
}
inline bool References(const Json& value,const std::string& path,int depth=0) {
    if(depth>8)return false;if(value.is_string())return value.get_ref<const std::string&>()==path;
    if(value.is_array()||value.is_object())for(const auto& child:value)if(References(child,path,depth+1))return true;
    return false;
}
template<class Read> bool ContainsRecipeArray(int count,const std::string& recipePath,std::size_t& remaining,Read&& read){
    if(count<0||count>65536||static_cast<std::size_t>(count)>remaining)throw std::runtime_error("Station recipe inspection exceeds its reference budget");
    remaining-=static_cast<std::size_t>(count);
    for(int i=0;i<count;++i)if(read(i)==recipePath)return true;
    return false;
}
inline Json MatchingPlacements(const Json& station,const Json& values,const std::string& recipePath) {
    auto matches=Json::array();
    if(!station.value("Array",std::string{}).empty()){if(References(values,recipePath))matches.push_back(station);return matches;}
    if(values.is_array())for(const auto& category:values){
        if(matches.size()>=32)break;
        if(!category.is_object()||!category.contains("Collection")||!References(category["Collection"],recipePath))continue;
        auto placement=station;if(category.contains("Label")&&category["Label"].is_string())placement["Category"]=category["Label"];
        matches.push_back(std::move(placement));
    }return matches;
}
inline std::string StationSource(const Json& station) {
    auto path=station.value("BuildingPiece",std::string{});if(path.empty())path=station.value("Path",std::string{});
    const auto end=path.find('/',path.starts_with("/Game/Mods/")?11:1);
    return path.starts_with('/')?path.substr(0,end):std::string("Unknown");
}
inline Json StationPresentation(const Json& row,const Json& building) {
    Json result={{"Name",""},{"Icon",""}};
    for(const auto* field:{"DisplayNameOverride","DisplayName","Name","StationName"})
        if(row.contains(field)&&row[field].is_string()&&!row[field].get_ref<const std::string&>().empty()) {result["Name"]=row[field];break;}
    if(result["Name"]==""&&building.contains("DisplayName")&&building["DisplayName"].is_string())result["Name"]=building["DisplayName"];
    for(const auto* source:{&row,&building}){
        for(const auto* field:{"DisplayIcon","Icon","StationIcon"})if(source->contains(field)&&(*source)[field].is_string()){
            const auto& path=(*source)[field].get_ref<const std::string&>();if(path.starts_with('/')&&path.size()<=1024){result["Icon"]=path;break;}
        }if(result["Icon"]!="")break;
    }return result;
}
inline bool StationMatches(const Json& station,std::string query,const std::string& source={}) {
    if(!source.empty()&&StationSource(station)!=source)return false;
    auto text=station.value("Name",std::string{})+" "+station.value("Path",std::string{})+" "+station.value("Row",std::string{})+" "+station.value("Array",std::string{});
    const auto lower=[](std::string& s){for(auto& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));};
    lower(query);lower(text);return query.empty()||text.find(query)!=std::string::npos;
}
inline Json LoadedStation(const Json& placements) {
    if(!placements.is_array()||placements.empty()||!placements.front().is_object())return Json::object();
    for(const auto& placement:placements)if(placement.is_object()&&placement.value("PhysicalStation",true))return placement;
    return placements.front();
}
}
