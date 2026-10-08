#pragma once
#include "Loader/VirtualDefinitionId.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <vector>

namespace DragonWilds::GameplayEffectDocument {
struct Definition {
    std::string Key;
    std::string ClassPath;
};
inline std::vector<Definition> Parse(const nlohmann::json& data,std::string_view owner) {
    if(!data.is_object())throw std::runtime_error("Effect document must be an object map");
    std::vector<Definition> result;
    for(const auto& [key,value]:data.items()) {
        if(key=="$Comment")continue;
        if(key.starts_with('$'))throw std::runtime_error("Unsupported effect directive: "+key);
        const auto id=VirtualDefinitionId::Qualify(owner,key);
        if(!VirtualDefinitionId::IsCanonical(id))throw std::runtime_error("Invalid effect definition ID: "+id);
        if(id.substr(0,id.find(':'))!=owner)throw std::runtime_error("Effect definition must belong to its mod: "+id);
        if(!value.is_object())throw std::runtime_error("Effect definition requires an object body: "+id);
        for(const auto& [field,unused]:value.items())
            if(field!="Class" && field!="$Comment")throw std::runtime_error("Unsupported effect field: "+field);
        if(!value.contains("Class") || !value.at("Class").is_string())
            throw std::runtime_error("Effect definition requires a cooked Class path: "+id);
        const auto path=value.at("Class").get<std::string>();
        if(!path.starts_with('/') || path.find('.')==std::string::npos || !path.ends_with("_C"))
            throw std::runtime_error("Effect Class must be a cooked Blueprint class path ending in _C: "+id);
        result.push_back({id,path});
    }
    return result;
}
}
