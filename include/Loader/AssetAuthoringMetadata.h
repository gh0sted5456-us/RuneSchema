#pragma once
// Author declarations are not native object properties or proof of package origin.
#include <nlohmann/json.hpp>
#include "Loader/AssetMetadataRegistry.h"
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
namespace RC::Unreal {class UObject;}
namespace PS::AssetMetadata {
inline bool Flag(const nlohmann::json& value,std::string_view field) {
    if(value.is_boolean())return value.get<bool>();
    if(value.is_string()) {
        auto text=value.get<std::string>();for(auto& c:text)if(c>='A'&&c<='Z')c=static_cast<char>(c+32);
        if(text=="yes"||text=="true")return true;
        if(text=="no"||text=="false")return false;
    }
    throw std::runtime_error("Asset metadata "+std::string(field)+" must be a JSON Boolean (or Yes/No string)");
}
inline bool IsKey(std::string_view key){return key=="Modded"||key=="RuneSchema";}
inline Declaration Read(const nlohmann::json& object) {
    Declaration result;if(!object.is_object())throw std::runtime_error("Asset metadata container must be an object");
    if(object.contains("RuneSchema"))result.runeSchema=Flag(object.at("RuneSchema"),"RuneSchema");
    if(!object.contains("Modded"))return result;
    const auto& block=object.at("Modded");
    if(!block.is_object()){result.modded=Flag(block,"Modded");return result;}
    result.modded=true;
    for(const auto& [key,value]:block.items()) {
        if(key=="RuneSchema") {
            const auto flag=Flag(value,key);
            if(result.runeSchema.has_value()&&*result.runeSchema!=flag)throw std::runtime_error("Conflicting RuneSchema metadata flags");
            result.runeSchema=flag;
        }else if(key=="Cooked")result.cooked=Flag(value,key);
        else if(key=="SafeToClone")result.safeToClone=Flag(value,key);
        else throw std::runtime_error("Unknown Modded metadata field: "+key+" (use RuneSchema, Cooked, SafeToClone)");
    }
    return result;
}
inline nlohmann::json Encode(const Declaration& value) {
    auto out=nlohmann::json::object();
    if(value.modded.has_value())out["Modded"]=*value.modded;
    if(value.runeSchema.has_value())out["RuneSchema"]=*value.runeSchema;
    if(value.cooked.has_value())out["Cooked"]=*value.cooked;
    if(value.safeToClone.has_value())out["SafeToClone"]=*value.safeToClone;
    return out;
}
}
