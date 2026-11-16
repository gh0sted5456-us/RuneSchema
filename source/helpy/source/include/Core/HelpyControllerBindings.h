#pragma once
#include "Core/HelpyControllerDevice.h"
#include "InputBindings.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <stdexcept>

namespace PS::HelpyControllerBindings {
struct Action {const char* Id;const char* Label;uint32_t Button;};
inline constexpr std::array<Action,12> Actions{{
 {"confirm","Select / toggle",XINPUT_GAMEPAD_A},{"cancel","Back / cancel",XINPUT_GAMEPAD_B},{"actions","Item actions",XINPUT_GAMEPAD_X},{"search","Item Bay search",XINPUT_GAMEPAD_Y},
 {"previous-tab","Previous panel",XINPUT_GAMEPAD_LEFT_SHOULDER},{"next-tab","Next panel",XINPUT_GAMEPAD_RIGHT_SHOULDER},{"previous-page","Previous workspace",InputBindings::LeftTrigger},{"next-page","Next workspace",InputBindings::RightTrigger},
 {"left","Navigate left",XINPUT_GAMEPAD_DPAD_LEFT},{"right","Navigate right",XINPUT_GAMEPAD_DPAD_RIGHT},{"up","Navigate up",XINPUT_GAMEPAD_DPAD_UP},{"down","Navigate down",XINPUT_GAMEPAD_DPAD_DOWN}
}};
using Profile=std::array<uint32_t,Actions.size()>;
inline Profile Defaults(){Profile p{};for(size_t i=0;i<p.size();++i)p[i]=Actions[i].Button;return p;}
inline const char* Family(const HelpyControllerDevice::Labels& labels){return labels==HelpyControllerDevice::PlayStation?"playstation":labels==HelpyControllerDevice::Nintendo?"nintendo":labels==HelpyControllerDevice::Xbox?"xbox":"generic";}
inline bool Supported(uint32_t button){return std::any_of(Actions.begin(),Actions.end(),[&](const auto& action){return action.Button==button;});}
inline Profile Read(const nlohmann::json& profiles,const char* family){
 auto result=Defaults();if(!profiles.contains(family))return result;const auto& value=profiles.at(family);if(!value.is_object()||value.size()>Actions.size())throw std::runtime_error("Controller profile must contain known actions");
 for(const auto& [id,binding]:value.items()){const auto action=std::find_if(Actions.begin(),Actions.end(),[&](const auto& item){return id==item.Id;});if(action==Actions.end()||!binding.is_string())throw std::runtime_error("Unknown controller action or invalid button");const auto button=InputBindings::ParsePad(binding.get<std::string>());if(!Supported(button))throw std::runtime_error("Choose one face, shoulder, trigger or direction button");result[size_t(action-Actions.begin())]=button;}
 auto sorted=result;std::sort(sorted.begin(),sorted.end());if(std::adjacent_find(sorted.begin(),sorted.end())!=sorted.end())throw std::runtime_error("Controller profile assigns one button to multiple actions");return result;
}
inline nlohmann::json Write(const Profile& profile){nlohmann::json value=nlohmann::json::object();for(size_t i=0;i<profile.size();++i)value[Actions[i].Id]=InputBindings::PadText(profile[i]);return value;}
inline nlohmann::json Validate(const nlohmann::json& profiles){
 if(!profiles.is_object()||profiles.size()>4)throw std::runtime_error("Invalid controller profiles");for(const auto& [family,_]:profiles.items()){if(family!="playstation"&&family!="xbox"&&family!="nintendo"&&family!="generic")throw std::runtime_error("Unknown controller profile");(void)Read(profiles,family.c_str());}return profiles;
}
inline void Rebind(Profile& profile,size_t action,uint32_t button){
 if(action>=profile.size()||!Supported(button))throw std::runtime_error("Unsupported controller action binding");const auto old=profile[action];for(size_t i=0;i<profile.size();++i)if(i!=action&&profile[i]==button)profile[i]=old;profile[action]=button;
}
inline XINPUT_GAMEPAD Apply(const XINPUT_GAMEPAD& pad,const Profile& profile){
 auto result=pad;uint32_t mask=0;for(const auto& action:Actions)mask|=action.Button;result.wButtons&=WORD(~mask);result.bLeftTrigger=result.bRightTrigger=0;
 const auto value=[&](uint32_t button){return button==InputBindings::LeftTrigger?pad.bLeftTrigger:button==InputBindings::RightTrigger?pad.bRightTrigger:BYTE((pad.wButtons&button)?255:0);};
 for(size_t i=0;i<profile.size();++i){const auto target=Actions[i].Button;const auto amount=value(profile[i]);if(target==InputBindings::LeftTrigger)result.bLeftTrigger=amount;else if(target==InputBindings::RightTrigger)result.bRightTrigger=amount;else if(amount>100)result.wButtons|=WORD(target);}
 return result;
}
}
