#pragma once
#include "Loader/ItemIdentity.h"
#include <array>
#include <string>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include "Loader/HumanPose.h"

namespace DragonWilds::HumanNpc {
inline constexpr std::array<const char*,8> AppearanceKeys={"BodyType","Head","HairPreset","FacialHairPreset","HairColor","SkinTone","EyeColor","EyebrowColor"};
inline constexpr std::array<const char*,8> HandleKeys={"BodyTypeDataHandle","FaceDataHandle","HairPresetDataHandle","FacialHairPresetDataHandle","HairColorPrimitiveDataHandle","SkinTonePrimitiveDataHandle","EyeColorPrimitiveDataHandle","EyebrowColorPrimitiveDataHandle"};
inline constexpr std::array<const char*,8> Tables={"BodyType","FaceType","HairPresets","FacialHairPresets","HairColor","SkinTone","EyeColor","EyebrowColor"};
inline constexpr std::array<const char*,7> EquipmentKeys={"Head","Body","Legs","Cape","Trinket","MainHand","OffHand"};
inline bool IsMainHandSlot(const std::string& slot) {
    return slot=="HeldOnlyRight" || slot=="HeldTwoHanded";
}
inline bool IsOffHandSlot(const std::string& slot) {
    return slot=="HeldOnlyLeft";
}
template<class Exists>
std::string SelectAttachment(const std::string& declared,const std::string& hand,Exists exists) {
    if(!declared.empty() && declared!="None" && exists(declared))return declared;
    // RuneSchema's sockets are authored on the corresponding player hand bones.
    // Prefer the stable bridge name, then the exact Dragonwilds player bone. Keep
    // the legacy prop bone last so older compatible meshes continue to render.
    const std::array<const char*,3>* candidates=nullptr;
    static constexpr std::array<const char*,3> right={"RS_MainHand","hand_r","prop_r"};
    static constexpr std::array<const char*,3> left={"RS_OffHand","hand_l","prop_l"};
    if(hand=="Right")candidates=&right;
    else if(hand=="Left")candidates=&left;
    if(!candidates)return {};
    for(const auto* candidate:*candidates)if(exists(candidate))return candidate;
    return {};
}
template<class Apply,class Failure>
void ApplyOptionalHeldVisual(Apply apply,Failure failure) {
    try {apply();}
    catch(const std::exception& error) {failure(error.what());}
    catch(...) {failure("unknown held visual failure");}
}
inline bool IsHuman(const nlohmann::json& data) {
    const auto type=data.value("Type",std::string("AI"));
    if(type!="AI" && type!="Human" && type!="Resource" && type!="Prop")throw std::runtime_error("NPC Type must be AI, Human, Resource or Prop");
    return type=="Human";
}
inline bool UsesStaticMesh(const nlohmann::json& data) {
    const auto type=data.value("Type",std::string("AI"));
    return type=="Resource" || type=="Prop";
}
inline void Validate(const nlohmann::json& data) {
    if(!IsHuman(data)) {
        if(data.contains("Appearance") || data.contains("Equipment") || data.contains("Pose") || data.contains("DialoguePose") || data.contains("Ghost") || data.contains("HideWeapon"))throw std::runtime_error("Appearance/Equipment/Pose/DialoguePose/Ghost/HideWeapon require Type Human");
        if(UsesStaticMesh(data)) {
            if(!data.contains("Mesh") || !data.at("Mesh").is_string() || data.at("Mesh").get_ref<const std::string&>().empty()
                || data.at("Mesh").get_ref<const std::string&>().front()!='/')
                throw std::runtime_error("Resource/Prop NPC requires a StaticMesh asset path in Mesh");
            for(const auto* key:{"VisualSource","IdleAnimation","IdleAnim","IdleAnimationAsset"})
                if(data.contains(key))throw std::runtime_error("Resource/Prop NPC uses Mesh only, without source actors or animation");
        }
        return;
    }
    if(data.contains("Ghost")) {
        const auto& ghost=data.at("Ghost");
        if(!ghost.is_object())throw std::runtime_error("Ghost must be an object of mesh-group toggles");
        for(const auto& [key,value]:ghost.items())
            if((key!="Character" && key!="Equipment" && key!="Weapons") || !value.is_boolean())
                throw std::runtime_error("Ghost accepts boolean Character, Equipment and Weapons fields");
    }
    if(data.contains("HideWeapon") && !data.at("HideWeapon").is_boolean())
        throw std::runtime_error("HideWeapon must be true or false");
    if(data.contains("Pose")) {
        if(data.contains("IdleAnimation") || data.contains("IdleAnim") || data.contains("IdleAnimationAsset"))
            throw std::runtime_error("Use Pose or IdleAnimation, not both");
        const auto pose=HumanPose::Parse(data.at("Pose"));
        if(pose.Name=="Equipment" && pose.Time>0 && (!data.contains("Equipment") || !data.at("Equipment").contains("MainHand")))
            throw std::runtime_error("Equipment pose Time requires MainHand equipment");
    }
    if(data.contains("DialoguePose") && HumanPose::Parse(data.at("DialoguePose")).Path.empty())
        throw std::runtime_error("DialoguePose requires a concrete preset or Asset; Equipment is unavailable");
    if(data.contains("Mesh") || data.contains("VisualSource") || data.contains("Materials"))
        throw std::runtime_error("Human uses Appearance and Equipment, not AI Mesh/VisualSource/Materials");
    const auto& appearance=data.at("Appearance");
    if(!appearance.is_object() || appearance.size()!=AppearanceKeys.size())throw std::runtime_error("Human Appearance requires all eight customization selectors");
    for(const auto* key:AppearanceKeys) {
        const auto& value=appearance.at(key);
        if(!value.is_string() || value.get_ref<const std::string&>().empty() || value.get_ref<const std::string&>().size()>128)
            throw std::runtime_error("Human appearance selectors must be nonempty row names of at most 128 characters");
    }
    if(!data.contains("Equipment"))return;
    const auto& equipment=data.at("Equipment");
    if(!equipment.is_object())throw std::runtime_error("Human Equipment must be a slot-to-item-path object");
    for(const auto& [key,value]:equipment.items()) {
        bool known=false;for(const auto* slot:EquipmentKeys)if(key==slot)known=true;
        if(!known)throw std::runtime_error("Unsupported human equipment slot: "+key);
        if(!value.is_string())throw std::runtime_error("Equipment requires Unreal item asset paths");
        const auto path=value.get<std::string>();
        if(!DragonWilds::IsCanonicalPersistenceId(path) && (path.empty() || path.front()!='/' || path.size()>1024 || path.find_first_of("\r\n\t")!=path.npos))
            throw std::runtime_error("Equipment requires Unreal item asset paths");
    }
}
}
