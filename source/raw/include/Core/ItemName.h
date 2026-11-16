#pragma once
#include <nlohmann/json.hpp>
#include <string>
namespace PS::ItemName {
using Json=nlohmann::json;
inline std::string Text(const Json& row,const char* key){if(!row.is_object())return {};const auto at=row.find(key);return at!=row.end()&&at->is_string()?at->get<std::string>():std::string{};}
inline std::string Clean(std::string value){
    const auto first=value.find_first_not_of(" \t\r\n");if(first==value.npos)return {};
    value=value.substr(first,value.find_last_not_of(" \t\r\n")-first+1);
    auto folded=value;for(auto& c:folded)if(c>='A'&&c<='Z')c=char(c+32);
    if(folded=="none"||folded.find("missing string")!=folded.npos||folded.find("string table entry")!=folded.npos)return {};
    if((value.starts_with("ITEM_")||value.starts_with("JOURNAL_")||value.starts_with("UI_"))&&value.find_first_of(" \t\r\n")==value.npos)return {};
    return value;
}
inline bool Resolved(const Json& row){
    const auto name=Clean(Text(row,"Name"));if(name.empty()||name==Text(row,"InternalName"))return false;
    auto leaf=Text(row,"Path");if(const auto at=leaf.find_last_of("/.");at!=leaf.npos)leaf.erase(0,at+1);
    return leaf.empty()||name!=leaf||(!leaf.starts_with("DA_")&&!leaf.starts_with("BP_")&&leaf.find('_')==leaf.npos);
}
inline void Repair(Json& row){
    if(!row.is_object())return;
    const auto display=Clean(Text(row,"DisplayName"));
    if(!display.empty())row["Name"]=display;
}
inline Json Merge(Json previous,const Json& incoming){
    Repair(previous);auto merged=previous;merged.update(incoming);
    if(Resolved(incoming)&&Clean(Text(incoming,"DisplayName")).empty()&&!Clean(Text(previous,"DisplayName")).empty())merged["DisplayName"]=incoming.at("Name");
    Repair(merged);
    // An unresolved observation must not replace a readable cached label.
    if(!Resolved(merged)&&Resolved(previous))merged["Name"]=previous.at("Name");
    if(Clean(Text(merged,"DisplayName")).empty()&&!Clean(Text(previous,"DisplayName")).empty())merged["DisplayName"]=previous.at("DisplayName");
    return merged;
}
}
