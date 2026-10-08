#pragma once
#include <array>
#include <cmath>
#include <string>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace DragonWilds::HumanPose {
enum class Playback { Hold, Loop, Once };
struct Preset { const char* Name; const char* Clip; Playback Mode; };
inline constexpr std::array Presets{
    Preset{"Equipment", "", Playback::Hold},
    Preset{"Relaxed", "Idle/A_PlayerM_CharacterSelect_Fresh", Playback::Loop},
    Preset{"CharacterSelect", "Idle/A_PlayerM_CharacterSelection_Combined", Playback::Loop},
    Preset{"Chair", "Interact/A_PlayerM_Interact_ChairIdle", Playback::Loop},
    Preset{"Armchair", "Interact/A_PlayerM_Interact_ArmChairIdle", Playback::Loop},
    Preset{"Unarmed", "Unarmed/A_PlayerM_Idle_Unarmed_Poses", Playback::Hold},
    Preset{"Crouched", "Unarmed/A_PlayerM_Idle_Crouch_Pose", Playback::Hold},
    Preset{"OneHanded", "Idle/A_PlayerM_Idle_1H_Poses", Playback::Hold},
    Preset{"Dagger", "Dagger/A_PlayerM_Idle_Dagger_Poses", Playback::Hold},
    Preset{"Staff", "Staff/A_PlayerM_Idle_Staff_Poses", Playback::Hold},
    Preset{"Sword", "Sword/A_PlayerM_Idle_Sword_Poses", Playback::Hold},
    Preset{"Pickaxe", "Pickaxe/A_PlayerM_Idle_Pickaxe_Poses", Playback::Hold},
    Preset{"Greatsword", "GreatSword/A_PlayerM_Idle_Greatsword_Poses", Playback::Hold},
    Preset{"Greataxe", "GreatAxe/A_PlayerM_Idle_GreatAxe_Poses", Playback::Hold},
    Preset{"Shield", "Shield/A_PlayerM_Idle_Kite_Poses", Playback::Hold},
    Preset{"Torch", "Torch/A_PlayerM_Idle_LH_Torch_Poses", Playback::Hold},
    Preset{"Bow", "Bow/A_PlayerM_Idle_Bow_Poses", Playback::Hold},
    Preset{"Yes", "Emote/A_PlayerM_Emote_Yes_01", Playback::Once},
    Preset{"No", "Emote/A_PlayerM_Emote_No_01", Playback::Once},
    Preset{"Wave", "Emote/A_PlayerM_Emote_Wave_01", Playback::Once},
    Preset{"Point", "Emote/A_PlayerM_Emote_Point_01", Playback::Once},
    Preset{"NoWay", "Emote/A_PlayerM_Emote_NoWay_01", Playback::Once},
    Preset{"Celebrate", "Emote/A_PlayerM_Emote_Celebrate_01", Playback::Once}
};
struct Selection {
    std::string Name="Equipment";
    std::string Path;
    Playback Mode=Playback::Hold;
    float Time=0;
    std::string WeaponVisibility="Auto";
};
inline bool HideWeapon(const Selection& pose) {
    if(pose.WeaponVisibility=="Hide")return true;
    if(pose.WeaponVisibility=="Show")return false;
    return !pose.Name.empty() ? pose.Mode!=Playback::Hold || pose.Name=="Unarmed" || pose.Name=="Crouched" : true;
}
inline Selection Parse(const nlohmann::json& value) {
    if(!value.is_object() || value.contains("Preset")==value.contains("Asset"))throw std::runtime_error("Pose requires exactly one of Preset or Asset");
    for(const auto& [key,item]:value.items())
        if(key!="Preset" && key!="Asset" && key!="Playback" && key!="WeaponVisibility" && key!="Time")throw std::runtime_error("Unsupported Pose field: "+key);
    const auto visibility=value.value("WeaponVisibility",std::string("Auto"));
    if(visibility!="Auto" && visibility!="Show" && visibility!="Hide")throw std::runtime_error("WeaponVisibility must be Auto, Show or Hide");
    if(value.contains("Asset")) {
        Selection result;
        result.Name.clear();result.Path=value.at("Asset").get<std::string>();result.WeaponVisibility=visibility;
        if(result.Path.size()<2 || result.Path.size()>1024 || result.Path.front()!='/' || result.Path.find_first_of("\r\n\t")!=result.Path.npos || result.Path.find('\0')!=result.Path.npos)
            throw std::runtime_error("Pose Asset requires an Unreal animation asset path");
        const auto playback=value.value("Playback",std::string("Loop"));
        if(playback=="Loop")result.Mode=Playback::Loop;
        else if(playback=="Once")result.Mode=Playback::Once;
        else if(playback=="Hold")result.Mode=Playback::Hold;
        else throw std::runtime_error("Pose Playback must be Loop, Once or Hold");
        if(value.contains("Time")) {
            if(result.Mode!=Playback::Hold || !value["Time"].is_number())throw std::runtime_error("Pose Time requires Hold playback");
            const double time=value["Time"].get<double>();
            if(!std::isfinite(time) || time<0 || time>60)throw std::runtime_error("Invalid Pose Time");
            result.Time=static_cast<float>(time);
        }
        return result;
    }
    if(value.contains("Playback"))throw std::runtime_error("Named presets define their own Playback");
    const auto name=value.at("Preset").get<std::string>();
    for(const auto& preset:Presets)if(name==preset.Name) {
        Selection result{name,{},preset.Mode,0};
        result.WeaponVisibility=visibility;
        if(*preset.Clip) {
            const std::string clip=preset.Clip;
            result.Path="/Game/Art/Animation/PlayerM/"+clip+"."+clip.substr(clip.find_last_of('/')+1);
        }
        if(value.contains("Time")) {
            if(preset.Mode!=Playback::Hold || !value["Time"].is_number())
                throw std::runtime_error("Pose Time is only supported for held poses");
            const double time=value["Time"].get<double>();
            if(!std::isfinite(time) || time<0 || time>60)throw std::runtime_error("Pose Time must be finite and between 0 and 60 seconds");
            result.Time=static_cast<float>(time);
        }
        return result;
    }
    throw std::runtime_error("Unknown human Pose preset: "+name);
}
inline void ValidateTime(float time,float duration) {
    if(!std::isfinite(duration) || duration<=0 || time>=duration)
        throw std::runtime_error("Pose Time must be less than the animation duration");
}
}
