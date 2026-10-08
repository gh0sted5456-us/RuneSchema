#pragma once
#include "Generator/AuthoringPolicy.h"
#include <optional>
#include <string>
#include <string_view>
namespace PS::HelpyStatKey {
struct Key {std::string handle,field;};
inline std::string Make(std::string_view handle,std::string_view field) {
    if(!Authoring::SafeFieldName(handle)||!Authoring::SafeFieldName(field))throw std::runtime_error("Invalid stat row field name");
    return "@row/"+std::string(handle)+"/"+std::string(field);
}
inline std::optional<Key> Parse(std::string_view name) {
    if(!name.starts_with("@row/"))return {};
    const auto at=name.find('/',5);
    if(at==name.npos)throw std::runtime_error("Malformed stat row field");
    Key key{std::string(name.substr(5,at-5)),std::string(name.substr(at+1))};
    (void)Make(key.handle,key.field);return key;
}
inline bool Required(std::string_view name) {
    return name=="WearableEquipmentDataTableRowHandle"||name=="BaseHeldEquipmentPowerLevelRowHandle"
        ||name=="BaseHeldEquipmentDamageNegationRowHandle";
}
}
