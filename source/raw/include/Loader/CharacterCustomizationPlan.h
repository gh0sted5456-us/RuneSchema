#pragma once

#include "Loader/RegistryPatchPlan.h"
#include <array>

namespace DragonWilds::CharacterCustomization {
    inline constexpr std::string_view Schema = "runeschema.character-customization/v1";
    inline constexpr std::string_view HairZones = "/Game/Gameplay/Character/Player/Customization/DT_Customization_HairZones.DT_Customization_HairZones";
    inline constexpr std::string_view HairPresets = "/Game/Gameplay/Character/Player/Customization/DT_Customization_HairPresets.DT_Customization_HairPresets";
    inline constexpr std::string_view CharacterOptions = "/Game/UI/MainMenu/CharacterCreate/Data/DA_CharacterOptionData.DA_CharacterOptionData_C";

    inline void Keys(const nlohmann::json& value, std::initializer_list<std::string_view> allowed, std::string_view where) {
        RegistryPatch::RequireObject(value, where); RegistryPatch::RequireKeys(value, allowed, where);
    }

    inline std::vector<RegistryPatch::Patch> Expand(const nlohmann::json& value, std::string actualOwner, std::string source) {
        Keys(value,{"$schema","schema","modId","priority","entries"},"character customization document");
        if(value.value("schema","")!=Schema)throw std::runtime_error("unsupported character customization schema");
        const auto declared=value.value("modId","");
        if(!RegistryPatch::IsId(declared)||RegistryPatch::NormalizeId(declared)!=RegistryPatch::NormalizeId(actualOwner))
            throw std::runtime_error("character customization modId does not match the containing mod");
        const auto owner=RegistryPatch::NormalizeId(actualOwner);const auto priority=value.value("priority",0);
        if(priority < -100000 || priority > 100000)throw std::runtime_error("character customization priority is out of range");
        if(!value.contains("entries")||!value.at("entries").is_array()||value.at("entries").size()>1024)
            throw std::runtime_error("character customization entries must be an array with at most 1024 entries");
        std::vector<RegistryPatch::Patch> out;std::set<std::string> ids;
        for(const auto& entry:value.at("entries")) {
            Keys(entry,{"id","type","enabled","visible","display","compatibility","color","zones"},"character customization entry");
            const auto id=entry.value("id","");if(!RegistryPatch::IsId(id)||!ids.insert(RegistryPatch::NormalizeId(id)).second)
                throw std::runtime_error("character customization id is invalid or duplicated");
            if(entry.value("type","")!="hair")throw std::runtime_error("character customization v1 currently supports type 'hair'");
            if(!entry.value("enabled",true))continue;
            const auto canonical=owner+":"+RegistryPatch::NormalizeId(id);const auto transaction="customization-"+RegistryPatch::NormalizeId(id);
            if(!entry.contains("zones")||!entry.at("zones").is_object())throw std::runtime_error("hair entry requires zones");
            const auto& zones=entry.at("zones");Keys(zones,{"zone1","zone2","zone3","zone4","zone5"},"hair zones");
            std::array<std::string,5> rowNames{};bool any=false;
            for(std::size_t index=0;index<5;++index) {
                const auto key="zone"+std::to_string(index+1);const auto zone=zones.contains(key)?zones.at(key):nlohmann::json{};
                if(zone.is_null())continue;Keys(zone,{"mesh","animClass"},key);
                const auto mesh=zone.value("mesh","");const auto animation=zone.value("animClass","");
                if(!RegistryPatch::IsObjectPath(mesh)||!RegistryPatch::IsObjectPath(animation))throw std::runtime_error(key+" requires cooked mesh and animClass object paths");
                any=true;rowNames[index]=RegistryPatch::StableRowName("RS_HZ",canonical,key);
                RegistryPatch::Patch patch;patch.Owner=owner;patch.Source=source;patch.Id=id+"-"+key;patch.CanonicalId=canonical+":"+key;
                patch.Transaction=transaction;patch.Profile="dragonwilds.characterCustomization.v1";patch.Priority=priority;
                patch.TargetSpec.Kind=RegistryPatch::TargetKind::DataTable;patch.TargetSpec.ObjectPath=std::string(HairZones);
                patch.TargetSpec.ExpectedRowStruct="/Script/Dominion.PlayerHairZoneCustomizationMeshData";patch.Op=RegistryPatch::Operation::AddRow;
                patch.Row={{"name",rowNames[index]},{"template",{{"rowName","Zone1_None"}}},{"values",{{"HairZone","Zone"+std::to_string(index+1)},
                    {"ZoneStyleIndex","auto"},{"SoftSkeletalMesh",mesh},{"SoftAnimBlueprintClass",animation}}}};
                patch.Preconditions={{"requiredProperties",{{"HairZone","EnumProperty"},{"ZoneStyleIndex","IntProperty"},{"SoftSkeletalMesh","SoftObjectProperty"},{"SoftAnimBlueprintClass","SoftClassProperty"}}}};
                patch.Digest=RegistryPatch::StableDigest(entry.dump()+key);out.push_back(std::move(patch));
            }
            if(!any)throw std::runtime_error("hair entry must define at least one zone");
            const auto preset=RegistryPatch::StableRowName("RS_HP",canonical);
            nlohmann::json values=nlohmann::json::object();
            for(std::size_t index=0;index<5;++index)values["HairZone"+std::to_string(index+1)+"DataHandle"]={{"DataTable",HairZones},{"RowName",rowNames[index].empty()?"Zone"+std::to_string(index+1)+"_None":rowNames[index]}};
            RegistryPatch::Patch presetPatch;presetPatch.Owner=owner;presetPatch.Source=source;presetPatch.Id=id+"-preset";presetPatch.CanonicalId=canonical+":preset";
            presetPatch.Transaction=transaction;presetPatch.Profile="dragonwilds.characterCustomization.v1";presetPatch.Priority=priority;
            presetPatch.TargetSpec.Kind=RegistryPatch::TargetKind::DataTable;presetPatch.TargetSpec.ObjectPath=std::string(HairPresets);
            presetPatch.TargetSpec.ExpectedRowStruct="/Script/Dominion.PlayerHairPresetCustomizationMeshData";presetPatch.Op=RegistryPatch::Operation::AddRow;
            presetPatch.Row={{"name",preset},{"template",{{"rowName","Zone1_None"}}},{"values",values}};presetPatch.Digest=RegistryPatch::StableDigest(entry.dump()+"preset");
            for(std::size_t index=0;index<5;++index)if(!rowNames[index].empty())presetPatch.DependsOn.push_back(id+"-zone"+std::to_string(index+1));out.push_back(std::move(presetPatch));
            if(entry.value("visible",true)) {
                const auto display=entry.value("display",nlohmann::json::object());if(!display.is_object())throw std::runtime_error("display must be an object");
                Keys(display,{"name","description","image","sortGroup","sortOrder"},"display");
                const auto compatibility=entry.value("compatibility",nlohmann::json::object());if(!compatibility.is_object())throw std::runtime_error("compatibility must be an object");
                Keys(compatibility,{"bodyType","faceTypes","eyeTypes"},"compatibility");
                RegistryPatch::Patch menu;menu.Owner=owner;menu.Source=source;menu.Id=id+"-menu";menu.CanonicalId=canonical+":menu";menu.Transaction=transaction;
                menu.Profile="dragonwilds.characterCustomization.v1";menu.Priority=priority;menu.TargetSpec.Kind=RegistryPatch::TargetKind::ClassDefaultObject;
                menu.TargetSpec.ObjectPath=std::string(CharacterOptions);menu.Op=RegistryPatch::Operation::AppendUnique;menu.Property="CharacterOptions[HairPreset].OptionData";
                menu.Identity={{"property","DataHandle.RowName"},{"value",preset}};menu.Template={{"fromIndex",0}};
                menu.Value={{"Name",display.value("name",id)},{"OptionalText",display.value("description","")},{"Image",display.value("image",nlohmann::json{})},
                    {"BodyTypeCompatability",compatibility.value("bodyType","both")},{"FaceTypeCompatibility",compatibility.value("faceTypes",nlohmann::json("all"))},
                    {"EyeTypeCompatibility",compatibility.value("eyeTypes",nlohmann::json("all"))},{"DataHandle",{{"DataTable",HairPresets},{"RowName",preset}}},{"bIsDisabled",false}};
                menu.DependsOn={id+"-preset"};menu.Digest=RegistryPatch::StableDigest(entry.dump()+"menu");out.push_back(std::move(menu));
            }
        }
        return out;
    }
}
