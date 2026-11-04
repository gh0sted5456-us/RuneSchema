#pragma once
#include "Core/ConfigFiles.h"
#include "Core/CacheBundleIO.h"
#include <nlohmann/json.hpp>
#include <cmath>

namespace PS::HelpyHudLayout {
struct Point {float X=0,Y=0;};
struct Rect {float X=0,Y=0,W=0,H=0;};
struct Settings {bool Minimap=false,CoordinatesBelowMap=true,Custom=false,FollowPlayer=true,MapPositionLocked=false;float MapSize=280,MapZoom=6;Point Map{1,0},MapFocus{.5f,.5f},Coordinates{1,1},Notifications{0,0},Loading{.5f,.5f};};
inline std::filesystem::path File(const std::filesystem::path& root){return root/"settings/helpy-hud.json";}
inline Settings Read(const std::filesystem::path& root){
    const auto file=File(root);CacheBundleIO::Safe(file);if(!std::filesystem::exists(file))return {};const auto doc=nlohmann::json::parse(ConfigFiles::Read(file,16384));Settings s;s.Custom=true;s.Minimap=doc.value("Minimap",false);s.CoordinatesBelowMap=doc.value("CoordinatesBelowMap",true);
    const auto number=[&](const nlohmann::json& value,const char* key,float fallback,float lo,float hi){const float v=value.value(key,fallback);if(!std::isfinite(v)||v<lo||v>hi)throw std::runtime_error("Invalid HUD layout.");return v;};
    s.MapSize=number(doc,"MapSize",280,160,600);s.MapZoom=number(doc,"MapZoom",6,1,32);s.FollowPlayer=doc.value("FollowPlayer",true);s.MapPositionLocked=doc.value("MapPositionLocked",false);const auto point=[&](const char* name,Point fallback){const auto obj=doc.value(name,nlohmann::json::object());return Point{number(obj,"X",fallback.X,0,1),number(obj,"Y",fallback.Y,0,1)};};s.Map=point("Map",s.Map);s.MapFocus=point("MapFocus",s.MapFocus);s.Coordinates=point("Coordinates",s.Coordinates);s.Notifications=point("Notifications",s.Notifications);s.Loading=point("Loading",s.Loading);return s;
}
inline void Save(const std::filesystem::path& root,const Settings& s){const auto point=[](Point p){return nlohmann::json{{"X",p.X},{"Y",p.Y}};};const nlohmann::json doc={{"Minimap",s.Minimap},{"CoordinatesBelowMap",s.CoordinatesBelowMap},{"MapSize",s.MapSize},{"MapZoom",s.MapZoom},{"FollowPlayer",s.FollowPlayer},{"MapPositionLocked",s.MapPositionLocked},{"Map",point(s.Map)},{"MapFocus",point(s.MapFocus)},{"Coordinates",point(s.Coordinates)},{"Notifications",point(s.Notifications)},{"Loading",point(s.Loading)}};const auto file=File(root);CacheBundleIO::Safe(file);ConfigFiles::Write(file,doc.dump(2));}
inline Rect Place(float width,float height,float w,float h,Point p){if(width<1||height<1)return {};w=std::clamp(w,1.f,width);h=std::clamp(h,1.f,height);return {std::clamp(16.f+(width-w-32.f)*p.X,0.f,width-w),std::clamp(16.f+(height-h-32.f)*p.Y,0.f,height-h),w,h};}
inline Rect Map(float width,float height,const Settings& s){const float footer=s.Minimap&&s.CoordinatesBelowMap?72.f:0.f;const float size=std::min(s.MapSize,std::max(1.f,height-footer));auto box=Place(width,height,size,size,s.Map);box.Y=std::min(box.Y,std::max(0.f,height-box.H-footer));return box;}
inline Rect Coordinates(float width,float height,float w,float h,const Settings& s){if(s.Minimap&&s.CoordinatesBelowMap){const auto map=Map(width,height,s);return {map.X,std::clamp(map.Y+map.H+8,0.f,std::max(0.f,height-h)),map.W,std::min(h,height)};}return Place(width,height,w,h,s.Coordinates);}
inline std::string CoordinateText(std::string text,const Settings& s){if(s.Minimap&&s.CoordinatesBelowMap){const auto separator=text.find("   |   ");if(separator!=text.npos)text.replace(separator,7,"\n");}return text;}
inline Point Drag(float width,float height,float w,float h,float x,float y){return {std::clamp((x-16)/std::max(1.f,width-w-32),0.f,1.f),std::clamp((y-16)/std::max(1.f,height-h-32),0.f,1.f)};}
}
