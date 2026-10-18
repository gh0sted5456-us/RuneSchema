#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <map>
#include <string>
namespace PS::PlayerPortraits {
inline std::string Identity(std::string guid){
    if(guid.size()!=32||guid.find_first_not_of("0123456789abcdefABCDEF")!=guid.npos||guid.find_first_not_of('0')==guid.npos)return {};
    std::transform(guid.begin(),guid.end(),guid.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return guid;
}
inline bool Icon(const std::string& path){return path.size()>4&&path.size()<=2048&&path[0]=='/'&&path.find('.')!=path.npos&&path.find("..") == path.npos
    &&path.find_first_of("\\:\r\n\t")==path.npos&&std::none_of(path.begin(),path.end(),[](unsigned char c){return c<32||c==127;});}
struct Choices {
    std::map<std::string,std::string> Icons;
    bool Meters=true,Equipment=true,Skills=true,Power=true,Roles=true,HoverVitals=false,Technical=false;
    bool Set(const std::string& guid,const std::string& icon){const auto id=Identity(guid);if(id.empty()||(!icon.empty()&&!Icon(icon)))return false;
        if(icon.empty()){Icons.erase(id);return true;}if(Icons.size()>=256&&!Icons.contains(id))return false;Icons[id]=icon;return true;}
    std::string Get(const std::string& guid)const{const auto it=Icons.find(Identity(guid));return it==Icons.end()?std::string{}:it->second;}
    nlohmann::json Json()const{return {{"Version",1},{"Players",Icons},{"Display",{{"Meters",Meters},{"Equipment",Equipment},{"Skills",Skills},{"Power",Power},{"Roles",Roles},{"HoverVitals",HoverVitals},{"Technical",Technical}}}};}
    static Choices Parse(const nlohmann::json& document){Choices out;
        if(!document.is_object()||document.value("Version",0)!=1||!document.contains("Players")||!document["Players"].is_object()||document["Players"].size()>256)return out;
        for(const auto& [guid,icon]:document["Players"].items())if(icon.is_string())out.Set(guid,icon.get<std::string>());
        if(document.contains("Display")&&document["Display"].is_object()){const auto& d=document["Display"];const auto read=[&](const char* key,bool& value){if(d.contains(key)&&d[key].is_boolean())value=d[key];};read("Meters",out.Meters);read("Equipment",out.Equipment);read("Skills",out.Skills);read("Power",out.Power);read("Roles",out.Roles);read("HoverVitals",out.HoverVitals);read("Technical",out.Technical);}return out;}
};
}
