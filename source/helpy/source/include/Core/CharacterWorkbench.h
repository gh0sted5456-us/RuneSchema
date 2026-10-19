#pragma once
#include <nlohmann/json.hpp>
#include <array>
#include <string>
#include <stdexcept>
#include <cctype>
namespace PS::CharacterWorkbench {
using Json=nlohmann::json;
struct Field {const char* Key;const char* Label;const char* Table;const char* Native;};
inline constexpr std::array Fields{
 Field{"BodyType","Body type","BodyType","BodyTypeDataHandle"},
 Field{"Head","Face","FaceType","FaceDataHandle"},
 Field{"HairPreset","Hair style","HairPresets","HairPresetDataHandle"},
 Field{"FacialHairPreset","Beard style","FacialHairPresets","FacialHairPresetDataHandle"},
 Field{"SkinTone","Skin tone","SkinTone","SkinTonePrimitiveDataHandle"},
 Field{"EyeColor","Eye color","EyeColor","EyeColorPrimitiveDataHandle"},
 Field{"HairColor","Hair color","HairColor","HairColorPrimitiveDataHandle"},
 Field{"EyebrowColor","Eyebrow color","EyebrowColor","EyebrowColorPrimitiveDataHandle"}};
inline std::string Table(const Field& field){const auto name=std::string("DT_Customization_")+field.Table;return "/Game/Gameplay/Character/Player/Customization/"+name+"."+name;}
inline const Field& Find(const std::string& key){for(const auto& field:Fields)if(key==field.Key)return field;throw std::runtime_error("Unsupported native appearance field");}
inline bool Identifier(const std::string& value){if(value.empty()||value.size()>128)return false;for(unsigned char c:value)if(!std::isalnum(c)&&c!='_'&&c!='-')return false;return true;}
inline const Json& Appearance(const Json& save){const auto& value=save.at("Customization").at("CustomizationData");if(!value.is_object())throw std::runtime_error("Native appearance layout unavailable");return value;}
inline std::string Row(const Json& appearance,const Field& field){const auto& handle=appearance.at(field.Key);const auto table=handle.at("dataTable").get<std::string>();const auto path=Table(field);if(table!=path&&table!="/Script/Engine.DataTable'"+path+"'")throw std::runtime_error("Appearance table differs from the verified native table");const auto row=handle.at("rowName").get<std::string>();if(!Identifier(row))throw std::runtime_error("Appearance row is invalid");return row;}
inline std::string NativeText(const Json& appearance){std::string text="(";for(const auto& field:Fields){if(text.size()>1)text+=",";text+=std::string(field.Native)+"=(DataTable=DataTable'"+Table(field)+"',RowName="+Row(appearance,field)+")";}return text+")";}
inline std::string InventoryGroup(const Json& row){if(row.value("Container",std::string{})=="PersonalInventory")return "Personal storage";auto tag=row.value("NativeCategory",row.value("Category",std::string{}));for(auto& c:tag)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));if(tag.find("quest")!=tag.npos)return "Quest items";if(tag.find("ammo.rune")!=tag.npos||tag=="runes"||tag=="item.rune")return "Runes";if(tag.find("ammo")!=tag.npos||tag.find("arrow")!=tag.npos||tag.find("bolt")!=tag.npos)return "Ammo";return "Inventory";}
inline constexpr std::array Groups{"Inventory","Quick slots","Ammo","Runes","Quest items","Personal storage","Equipped"};
}
