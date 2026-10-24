#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <cctype>
#include <cmath>
#include <cstdio>
#include "Core/HelpyPersonalization.h"
namespace PS::ItemPresentation {
using Json=nlohmann::json;
struct Fact {std::string Label,Value;};
inline std::string Text(const Json& row,const char* key) {
    if(!row.is_object())return {};const auto it=row.find(key);
    return it!=row.end()&&it->is_string()?it->get<std::string>():std::string{};
}
inline std::string Icon(const Json& row,const Json* native=nullptr){
    // A draft is not the source item: do not overwrite its selected skin/icon
    // with a hover receipt for the unchanged source sharing its preview Path.
    const auto preview=row.is_object()?row.find("_HelpyIconPreview"):row.end();
    if(row.is_object()&&preview!=row.end()&&preview->is_boolean()&&preview->get<bool>())return Text(row,"Icon");
    const auto path=Text(row,"Path");
    if(native&&!path.empty()&&Text(*native,"Source")==path){
        const auto found=native->find("Icon");
        // An explicitly empty native icon must not resurrect an inherited icon.
        if(found!=native->end()&&found->is_string())return found->get<std::string>();
    }
    return Text(row,"Icon");
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
inline std::string CleanName(std::string value) {
    const auto first=value.find_first_not_of(" \t\r\n");if(first==value.npos)return {};
    value=value.substr(first,value.find_last_not_of(" \t\r\n")-first+1);
    auto folded=value;for(auto& c:folded)if(c>='A'&&c<='Z')c=char(c+('a'-'A'));
    if(folded=="none"||folded.find("missing string")!=folded.npos||folded.find("string table entry")!=folded.npos)return {};
    return value;
}
inline std::string OriginalName(const Json& row) {
    for(const auto* field:{"Name","DisplayName"})if(auto name=CleanName(Text(row,field));!name.empty())return name;
    // Blank/missing cooked localization must not make a row invisible. This
    // is a presentation fallback only; identity remains the canonical Path.
    auto leaf=Text(row,"Path");if(const auto split=leaf.find_last_of("/.");split!=leaf.npos)leaf.erase(0,split+1);
    if(leaf.empty())leaf=CleanName(Text(row,"InternalName"));
    for(const auto* prefix:{"ITEM_","BP_"})if(leaf.starts_with(prefix)){leaf.erase(0,std::char_traits<char>::length(prefix));break;}
    if(leaf.ends_with("_C"))leaf.resize(leaf.size()-2);
    return leaf.empty()?"Unnamed item":Label(leaf);
}
inline std::string Name(const Json& row){const auto nickname=HelpyPersonalization::Nickname(HelpyPersonalization::ItemKey(row));return nickname.empty()?OriginalName(row):nickname;}
inline bool Placeholder(const Json& row){auto name=OriginalName(row);return name.size()>=4&&name[0]=='['&&(name[1]=='P'||name[1]=='p')&&(name[2]=='H'||name[2]=='h')&&name[3]==']';}
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
