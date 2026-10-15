#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <cctype>
#include <cmath>
#include <cstdio>
namespace PS::ItemPresentation {
using Json=nlohmann::json;
struct Fact {std::string Label,Value;};
inline std::string Text(const Json& row,const char* key) {
    if(!row.is_object())return {};const auto it=row.find(key);
    return it!=row.end()&&it->is_string()?it->get<std::string>():std::string{};
}
inline std::string Label(const std::string& name) {
    std::string result;
    for(size_t i=0;i<name.size();++i){const unsigned char c=name[i];
        if(c=='_'){if(!result.empty()&&result.back()!=' ')result+=' ';continue;}
        if(i&&std::isupper(c)&&(std::islower(static_cast<unsigned char>(name[i-1]))||
            (i+1<name.size()&&std::isupper(static_cast<unsigned char>(name[i-1]))&&std::islower(static_cast<unsigned char>(name[i+1])))))result+=' ';
        result+=static_cast<char>(c);
    }return result;
}
inline bool DisplayField(const std::string& name) {
    if(name.empty()||name.size()>100)return false;
    for(const auto* token:{"Guid","GUID","ID","Id","Path","Class","Asset","Icon","Mesh","Material","SoftDelete","Flags","Table","Row","Reference","Index","Seed","Socket","Tag","Name","Description","PowerLevel","Persistence"})
        if(name.find(token)!=std::string::npos)return false;
    return true;
}
inline std::string Value(const Json& value) {
    if(value.is_number()){const auto number=value.get<double>();if(!std::isfinite(number))return {};char text[64]{};std::snprintf(text,sizeof(text),"%.6g",number);return text;}
    if(value.is_string()){auto text=value.get<std::string>();if(const auto scope=text.rfind("::");scope!=text.npos)text=Label(text.substr(scope+2));if(text.size()<=120&&!text.starts_with('/')&&text.find('\n')==text.npos)return text;}
    return {};
}
inline std::vector<Fact> Facts(const Json& details) {
    std::vector<Fact> result;std::map<std::string,bool> seen;
    if(!details.is_object()||!details.contains("Fields")||!details["Fields"].is_array())return result;
    for(const auto& field:details["Fields"]){if(!field.is_object()||!field.contains("Value"))continue;
        if(field.contains("HasValue")&&(!field["HasValue"].is_boolean()||!field["HasValue"].get<bool>()))continue;
        const auto native=Text(field,"NativeName"),name=native.empty()?Text(field,"Name"):native;
        const auto value=Value(field["Value"]);if(!DisplayField(name)||value.empty())continue;
        const auto label=Label(name);if(seen.contains(label))continue;seen[label]=true;result.push_back({label,value});if(result.size()==16)break;
    }return result;
}
inline std::string Description(const Json& row) {
    for(const auto* key:{"Description","ItemDescription","TooltipDescription","DescriptionText","FlavorText"}){const auto text=Text(row,key);if(!text.empty())return text.substr(0,4096);}
    if(row.is_object()&&row.contains("Fields")&&row["Fields"].is_array())for(const auto& field:row["Fields"]){
        const auto name=Text(field,"NativeName").empty()?Text(field,"Name"):Text(field,"NativeName");
        for(const auto* key:{"Description","ItemDescription","TooltipDescription","DescriptionText","FlavorText"})if(name==key){const auto value=Text(field,"Value");if(!value.empty())return value.substr(0,4096);}
    }return {};
}
inline std::string StationIcon(const Json& station) {
    for(const auto* key:{"DisplayIcon","Icon","StationIcon"}){const auto path=Text(station,key);
        if(path.starts_with('/')&&!path.starts_with("//")&&path.size()<=1024&&path.find('.')!=path.npos)return path;
    }return {};
}
}
