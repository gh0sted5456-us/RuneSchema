#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <cmath>
#include <vector>
#include "Core/BackgroundPolicy.h"
#include "Core/ContentSource.h"
namespace PS::PlayerPortraits {
struct Crop {float X=.5f,Y=.5f,Zoom=1;};
struct Bounds {float Left,Top,Right,Bottom;};
inline float Srgb(float linear){return linear<=.0031308f?linear*12.92f:1.055f*std::pow(linear,1.f/2.4f)-.055f;}
inline Bounds Frame(unsigned width,unsigned height,Crop crop){
    if(!width||!height)return {};
    const float zoom=std::isfinite(crop.Zoom)?std::clamp(crop.Zoom,1.f,8.f):1.f;
    const float side=std::min(width,height)/zoom,dx=side/width,dy=side/height;
    const float x=std::isfinite(crop.X)?std::clamp(crop.X,dx*.5f,1-dx*.5f):.5f;
    const float y=std::isfinite(crop.Y)?std::clamp(crop.Y,dy*.5f,1-dy*.5f):.5f;
    return {x-dx*.5f,y-dy*.5f,x+dx*.5f,y+dy*.5f};
}
inline std::vector<unsigned char> Cropped(const std::vector<unsigned char>& pixels,unsigned width,unsigned height,Crop crop){
    if(!width||!height||width>4096||height>4096||pixels.size()!=size_t(width)*height*4)return {};
    const auto frame=Frame(width,height,crop);std::vector<unsigned char> result(256*256*4);
    for(unsigned y=0;y<256;++y)for(unsigned x=0;x<256;++x){
        const auto sx=std::min(width-1,unsigned((frame.Left+(x+.5f)/256*(frame.Right-frame.Left))*width));
        const auto sy=std::min(height-1,unsigned((frame.Top+(y+.5f)/256*(frame.Bottom-frame.Top))*height));
        std::copy_n(pixels.data()+(size_t(sy)*width+sx)*4,4,result.data()+(y*256+x)*4);
    }return result;
}
inline std::string Identity(std::string guid){
    if(guid.size()!=32||guid.find_first_not_of("0123456789abcdefABCDEF")!=guid.npos||guid.find_first_not_of('0')==guid.npos)return {};
    std::transform(guid.begin(),guid.end(),guid.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return guid;
}
inline bool Icon(const std::string& path){return path.size()>4&&path.size()<=2048&&path[0]=='/'&&path.find('.')!=path.npos&&path.find("..") == path.npos
    &&path.find_first_of("\\:\r\n\t")==path.npos&&std::none_of(path.begin(),path.end(),[](unsigned char c){return c<32||c==127;});}
struct Choices {
    std::map<std::string,std::string> Icons;
    std::map<std::string,std::string> Backgrounds;
    static bool Background(const std::string& value){return value.empty()||(value.starts_with("source:")&&ContentSource::Valid(value.substr(7)))||(value.starts_with("background:")&&BackgroundPolicy::Filename(value.substr(11)))||value=="ui:home-banner.png";}
    bool SetBackground(const std::string& guid,const std::string& value){const auto id=Identity(guid);if(id.empty()||!Background(value))return false;if(value.empty()){Backgrounds.erase(id);return true;}if(Backgrounds.size()>=256&&!Backgrounds.contains(id))return false;Backgrounds[id]=value;return true;}
    std::string GetBackground(const std::string& guid)const{const auto at=Backgrounds.find(Identity(guid));return at==Backgrounds.end()?std::string{}:at->second;}
    bool Meters=true,Equipment=true,Skills=true,Power=true,Roles=true,HoverVitals=false,Technical=false,CompactParty=true;
    bool Set(const std::string& guid,const std::string& icon){const auto id=Identity(guid);if(id.empty()||(!icon.empty()&&!Icon(icon)))return false;
        if(icon.empty()){Icons.erase(id);return true;}if(Icons.size()>=256&&!Icons.contains(id))return false;Icons[id]=icon;return true;}
    std::string Get(const std::string& guid)const{const auto it=Icons.find(Identity(guid));return it==Icons.end()?std::string{}:it->second;}
    nlohmann::json Json()const{return {{"Version",1},{"Players",Icons},{"Backgrounds",Backgrounds},{"Display",{{"Meters",Meters},{"Equipment",Equipment},{"Skills",Skills},{"Power",Power},{"Roles",Roles},{"HoverVitals",HoverVitals},{"Technical",Technical},{"CompactParty",CompactParty}}}};}
    static Choices Parse(const nlohmann::json& document){Choices out;
        if(!document.is_object()||document.value("Version",0)!=1||!document.contains("Players")||!document["Players"].is_object()||document["Players"].size()>256)return out;
        for(const auto& [guid,icon]:document["Players"].items())if(icon.is_string())out.Set(guid,icon.get<std::string>());
        if(document.contains("Backgrounds")&&document["Backgrounds"].is_object()&&document["Backgrounds"].size()<=256)for(const auto& [guid,value]:document["Backgrounds"].items())if(value.is_string())out.SetBackground(guid,value.get<std::string>());
        if(document.contains("Display")&&document["Display"].is_object()){const auto& d=document["Display"];const auto read=[&](const char* key,bool& value){if(d.contains(key)&&d[key].is_boolean())value=d[key];};read("Meters",out.Meters);read("Equipment",out.Equipment);read("Skills",out.Skills);read("Power",out.Power);read("Roles",out.Roles);read("HoverVitals",out.HoverVitals);read("Technical",out.Technical);read("CompactParty",out.CompactParty);}return out;}
};
}
