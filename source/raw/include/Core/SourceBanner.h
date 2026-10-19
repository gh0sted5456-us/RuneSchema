#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <stdexcept>
#include "Core/ItemSource.h"
// Small, inert pixels inside an owner-scoped art record. No file paths, URLs,
// commands or runtime registrations are accepted from a banner.
namespace PS::SourceBanner {
using Json=nlohmann::json;
inline constexpr unsigned Width=256,Height=144;
inline Json Encode(const std::vector<unsigned char>& pixels){
    if(pixels.size()!=Width*Height*4)throw std::runtime_error("Invalid source banner dimensions");
    constexpr char digits[]="0123456789abcdef";std::string hex;hex.reserve(pixels.size()*2);
    for(auto c:pixels){hex+=digits[c>>4];hex+=digits[c&15];}
    return {{"Width",Width},{"Height",Height},{"RGBAHex",hex}};
}
inline std::vector<unsigned char> Decode(const Json& image){
    if(!image.is_object()||image.size()!=3||image.value("Width",0)!=Width||image.value("Height",0)!=Height||!image.contains("RGBAHex")||!image["RGBAHex"].is_string())throw std::runtime_error("[RS-CACHE-002] Invalid source banner dimensions.");
    const auto& hex=image["RGBAHex"].get_ref<const std::string&>();if(hex.size()!=Width*Height*8)throw std::runtime_error("[RS-CACHE-002] Invalid source banner pixel extent.");
    const auto digit=[](char c)->unsigned{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;throw std::runtime_error("[RS-CACHE-002] Invalid source banner pixels.");};
    std::vector<unsigned char> out;out.reserve(hex.size()/2);for(size_t i=0;i<hex.size();i+=2)out.push_back(static_cast<unsigned char>(digit(hex[i])*16+digit(hex[i+1])));return out;
}
inline std::string Key(const std::string& text){std::string key;for(unsigned char c:text)if(c>='A'&&c<='Z')key+=char(c+32);else if(c>='a'&&c<='z'||c>='0'&&c<='9')key+=char(c);return key;}
inline const char* Builtin(const std::string& owner){
    if(owner=="core")return "source-base-game.png";
    const auto key=Key(owner.substr(owner.find('/')+1));
    if(owner.starts_with("plugins/")){
        if(key=="umbralsands")return "source-umbral-sands.png";
        if(key=="scornedwilderness"||key=="scorchedwilderness")return "source-scorched-wilderness.png";
        if(key=="bastion"||key=="thebastion"||key=="dowdunreach")return "source-bastion.png";
        return "";
    }
    if(!owner.starts_with("mods/"))return "";
    if(key.starts_with("dye")||key=="jobboarddyequests"||key.starts_with("colorfuldye")||key.starts_with("runeschemadye"))return "source-colorfuldye.png";
    if(key=="shadowmancer"||key=="shadowmancerarmor"||key=="runeschemashadowmancer")return "source-shadowmancer.png";
    if(key=="gunslinger"||key=="flintlocks"||key=="flintlockpistols"||key=="runeschemaflintlockpistols")return "source-gunslinger.png";
    if(key=="rsdw"||key=="rsdwspears"||key=="spearsscythes"||key=="spearsandscythes"||key=="runeschemaspearsscythes")return "source-spears-scythes.png";
    return "";
}
inline std::string OwnerForItem(const Json& row){
    const auto info=ItemSource::Row(row);if(info.Group==ItemSource::Kind::Vanilla)return "core";
    if(info.Group==ItemSource::Kind::Expansion){const auto source=HelpyCacheSources::Source(row.value("Path",std::string{}));return source.empty()?std::string{}:"plugins/"+source;}
    if(info.Group==ItemSource::Kind::Modded&&!info.Name.empty())return ContentSource::Mod(info.Name);return {};
}
}
