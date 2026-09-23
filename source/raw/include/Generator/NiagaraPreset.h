#pragma once
#include <nlohmann/json.hpp>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>

namespace PS::NiagaraPreset {
using nlohmann::json;
inline json Parse(const std::string& text) {
    if(text.size()>16384)throw std::runtime_error("Niagara preset exceeds 16 KiB.");
    size_t nodes=0;
    return json::parse(text,[&](int depth,json::parse_event_t,json&) {
        if(depth>8 || ++nodes>512)throw std::runtime_error("Niagara preset structure exceeds limits.");
        return true;
    },true,true);
}
inline json Validate(json preset) {
    if(!preset.is_object())throw std::runtime_error("Niagara preset must be an object.");
    for(const auto& [key,value]:preset.items())
        if(key!="Version" && key!="Name" && key!="System" && key!="Target" && key!="Socket" && key!="LocationOffset"
            && key!="RotationOffset" && key!="Emitters" && key!="Parameters")
            throw std::runtime_error("Unknown Niagara preset field: "+key);
    if(!preset.contains("Version") || !preset["Version"].is_number_integer() || preset["Version"]!=1)
        throw std::runtime_error("Niagara preset requires Version 1.");
    auto string=[&](const char* key,size_t max) {
        if(!preset.contains(key) || !preset[key].is_string())throw std::runtime_error(std::string("Missing string: ")+key);
        const auto value=preset[key].get<std::string>();
        if(value.empty() || value.size()>max)throw std::runtime_error(std::string("Invalid length: ")+key);
        for(unsigned char c:value)if(c<32 || c==127)throw std::runtime_error("Control characters are not allowed.");
        return value;
    };
    string("Name",96);
    const auto system=string("System",512);
    const auto dot=system.find('.'),slash=system.rfind('/');
    if(system.front()!='/' || slash==0 || dot==std::string::npos || dot<=slash+1 || dot+1==system.size()
        || system.find("..")!=std::string::npos || system.find('.',dot+1)!=std::string::npos
        || system.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_/.")!=std::string::npos)
        throw std::runtime_error("System requires a cooked object path: /Game/Folder/Asset.Asset.");
    if(!preset.contains("Target"))preset["Target"]="PlayerRoot";
    const auto target=string("Target",32);
    if(target!="PlayerRoot" && target!="PlayerMesh")throw std::runtime_error("Target must be PlayerRoot or PlayerMesh (local player only).");
    if(!preset.contains("Socket"))preset["Socket"]="None";
    string("Socket",96);
    if(target=="PlayerRoot" && preset["Socket"]!="None")throw std::runtime_error("Named sockets require PlayerMesh.");
    const auto triple=[&](const char* field,const std::array<const char*,3>& keys,double limit) {
        if(!preset.contains(field)) {
            preset[field]={{keys[0],0.0},{keys[1],0.0},{keys[2],0.0}};
            return;
        }
        const auto& value=preset[field];
        if(!value.is_object() || value.size()!=3)
            throw std::runtime_error(std::string(field)+" must contain exactly three numeric fields.");
        for(const auto* key:keys) {
            if(!value.contains(key) || !value[key].is_number())
                throw std::runtime_error(std::string(field)+"."+key+" must be numeric.");
            const auto number=value[key].get<double>();
            if(!std::isfinite(number) || std::abs(number)>limit)
                throw std::runtime_error(std::string(field)+"."+key+" is outside the supported range.");
        }
    };
    triple("LocationOffset",{"X","Y","Z"},100000.0);
    triple("RotationOffset",{"Pitch","Yaw","Roll"},360000.0);
    if(!preset.contains("Emitters"))preset["Emitters"]=json::object();
    const auto& emitters=preset["Emitters"];
    if(!emitters.is_object() || emitters.size()>16)throw std::runtime_error("Emitters must be an object with at most 16 boolean entries.");
    for(const auto& [name,enabled]:emitters.items()) {
        if(name.empty() || name.size()>96 || !enabled.is_boolean())throw std::runtime_error("Emitter names require boolean enable values.");
        for(unsigned char c:name)if(c<32 || c==127)throw std::runtime_error("Invalid emitter name.");
    }
    if(!preset.contains("Parameters"))preset["Parameters"]=json::object();
    const auto& parameters=preset["Parameters"];
    if(!parameters.is_object() || parameters.size()>32)throw std::runtime_error("Parameters must contain at most 32 values.");
    for(const auto& [name,value]:parameters.items()) {
        if(name.rfind("User.",0)!=0 || name.size()>128 || value.is_array() || value.is_null())
            throw std::runtime_error("Parameter names require User. and values must be scalar or object.");
        const bool color=value.is_object() && value.contains("R") && value.contains("G") && value.contains("B") && value.contains("A");
        const bool vector=value.is_object() && value.contains("X") && value.contains("Y") && value.contains("Z");
        if(!value.is_boolean() && !value.is_number() && !color && !vector)
            throw std::runtime_error("Parameters support booleans, numbers, XYZ vectors, and RGBA colors.");
    }
    return preset;
}
inline json Builtin(size_t index) {
    constexpr std::array names{"Full fire","Fire_Ribbon only","Smoke only","Flames only"};
    if(index>=names.size())throw std::runtime_error("Unknown Niagara test preset.");
    json result={{"Version",1},{"Name",names[index]},
        {"System","/Game/Art/VFX/Library/Survival/Burning/NS_Fire_Small_NPC.NS_Fire_Small_NPC"},
        {"Target","PlayerRoot"},{"Socket","None"},{"Emitters",json::object()},{"Parameters",json::object()}};
    constexpr std::array selected{"","Fire_Ribbon","Smoke","Flames"};
    if(index)for(const auto* name:{"Fire_Ribbon","Smoke","Flames","Embers"})result["Emitters"][name]=std::string(name)==selected[index];
    return Validate(result);
}
}
