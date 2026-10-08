#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <cmath>
namespace DragonWilds {
// Archetypes are author-assigned labels and base icons, never inferred classes.
inline nlohmann::json NormalizePlayerArchetype(nlohmann::json value) {
    if (!value.is_object() || !value.contains("Archetype")) return value;
    const auto& archetype=value.at("Archetype");
    if (!archetype.is_object()) throw std::runtime_error("Archetype must be an object.");
    for (const auto& [key, ignored] : archetype.items())
        if (key!="Name" && key!="Icon" && key!="Scale")
            throw std::runtime_error("Unsupported Archetype field: "+key);
    for (const auto* field : {"Name","Icon"})
        if (!archetype.contains(field) || !archetype[field].is_string() || archetype[field].get_ref<const std::string&>().empty())
            throw std::runtime_error(std::string("Archetype requires ")+field);
    const auto& name=archetype["Name"].get_ref<const std::string&>();
    const auto& icon=archetype["Icon"].get_ref<const std::string&>();
    if (name.size()>64) throw std::runtime_error("Archetype.Name must be at most 64 UTF-8 bytes.");
    for (unsigned char c : name) if (c<32 || c==127) throw std::runtime_error("Control characters in Archetype.Name.");
    if (icon.size()>2048 || icon.front()!='/' || icon.find('.')==std::string::npos
        || icon.find('\\')!=std::string::npos || icon.find("..")!=std::string::npos)
        throw std::runtime_error("Archetype.Icon requires a canonical cooked texture object path.");
    for (unsigned char c : icon) if (c<=32 || c==127) throw std::runtime_error("Whitespace/control characters in Archetype.Icon.");
    if (archetype.contains("Scale") && (!archetype["Scale"].is_number()
        || !std::isfinite(archetype["Scale"].get<double>()) || archetype["Scale"]<0.1 || archetype["Scale"]>4.0))
        throw std::runtime_error("Archetype.Scale must be between 0.1 and 4.");
    if (!value.contains("Nameplate")) value["Nameplate"]=nlohmann::json::object();
    auto& plate=value["Nameplate"];
    if (!plate.is_object()) throw std::runtime_error("Nameplate must be an object.");
    if ((plate.contains("Mode") && plate["Mode"]!="Icon") || (plate.contains("Icon") && plate["Icon"]!=icon))
        throw std::runtime_error("Archetype conflicts with the base Nameplate Mode/Icon; use States for temporary icons.");
    plate["Mode"]="Icon";plate["Icon"]=icon;
    if (archetype.contains("Scale")) {
        if (plate.contains("Scale") && plate["Scale"]!=archetype["Scale"])
            throw std::runtime_error("Archetype.Scale conflicts with Nameplate.Scale.");
        plate["Scale"]=archetype["Scale"];
    }
    return value;
}
}
