#include "Loader/HumanPose.h"
#include "Loader/HumanNpc.h"
#include <cassert>
#include <limits>
#include <set>
using namespace DragonWilds;
using nlohmann::json;
template<class F> bool Rejects(F call) {try{call();}catch(const std::exception&){return true;}return false;}
int main() {
    std::set<std::string> names;
    for(const auto& preset:HumanPose::Presets) {
        assert(names.insert(preset.Name).second);
        const auto parsed=HumanPose::Parse({{"Preset",preset.Name}});
        assert(parsed.Mode==preset.Mode && parsed.Time==0);
        assert(parsed.Path.empty()==(parsed.Name=="Equipment"));
        if(preset.Mode!=HumanPose::Playback::Hold)
            assert(Rejects([&]{HumanPose::Parse({{"Preset",preset.Name},{"Time",0}});}));
    }
    assert(names.size()==23);
    const auto custom=HumanPose::Parse({{"Asset","/Game/Test.Idle"}});
    assert(custom.Mode==HumanPose::Playback::Loop && HumanPose::HideWeapon(custom));
    assert(!HumanPose::HideWeapon(HumanPose::Parse({{"Asset","/Game/Test.Idle"},{"WeaponVisibility","Show"}})));
    assert(HumanPose::HideWeapon(HumanPose::Parse({{"Preset","Relaxed"}})));
    assert(!HumanPose::HideWeapon(HumanPose::Parse({{"Preset","Equipment"}})));
    assert(HumanPose::HideWeapon(HumanPose::Parse({{"Preset","Staff"},{"WeaponVisibility","Hide"}})));
    assert(HumanPose::Parse({{"Asset","/Game/Test.Idle"},{"Playback","Hold"},{"Time",0.1}}).Time==0.1f);
    for(const auto& bad:json::array({{{"Asset","relative"}},{{"Asset","/Game/Test.Idle"},{"Preset","Staff"}},{{"Asset","/Game/Test.Idle"},{"Playback","Bad"}},{{"Asset","/Game/Test.Idle"},{"Time",1}},{{"Preset","Staff"},{"WeaponVisibility","Bad"}}}))
        assert(Rejects([&]{HumanPose::Parse(bad);}));
    assert(HumanPose::Parse({{"Preset","Staff"}}).Path=="/Game/Art/Animation/PlayerM/Staff/A_PlayerM_Idle_Staff_Poses.A_PlayerM_Idle_Staff_Poses");
    assert(HumanPose::Parse({{"Preset","Staff"},{"Time",0.1}}).Time==0.1f);
    for(const auto& bad:json::array({nullptr,"Staff",json::object(),{{"Preset","Breathing"}},{{"Preset","Staff"},{"Loop",true}},{{"Preset","Staff"},{"Time",-1}},{{"Preset","Staff"},{"Time",61}},{{"Preset","Staff"},{"Time","0"}}}))
        assert(Rejects([&]{HumanPose::Parse(bad);}));
    assert(Rejects([]{HumanPose::Parse({{"Preset","Staff"},{"Time",std::numeric_limits<double>::infinity()}});}));
    HumanPose::ValidateTime(0,0.1666667f);
    assert(Rejects([]{HumanPose::ValidateTime(0.2f,0.1666667f);}));
    assert(Rejects([]{HumanPose::ValidateTime(0,0);}));
    assert(Rejects([]{HumanNpc::Validate({{"Type","AI"},{"Pose",{{"Preset","Staff"}}}});}));
    assert(Rejects([]{HumanNpc::Validate({{"Type","Human"},{"Pose",{{"Preset","Staff"}}},{"IdleAnimation","/Game/Clip"}});}));
    json human={{"Type","Human"},{"Appearance",json::object()},{"HideWeapon",true},{"Ghost",{{"Character",true},{"Equipment",false},{"Weapons",true}}}};
    for(const auto* key:HumanNpc::AppearanceKeys)human["Appearance"][key]="test";
    HumanNpc::Validate(human);
    human["Ghost"]["Weapons"]="true";
    assert(Rejects([&]{HumanNpc::Validate(human);}));
    human["Ghost"]={{"Unknown",true}};
    assert(Rejects([&]{HumanNpc::Validate(human);}));
    assert(Rejects([]{HumanNpc::Validate({{"Type","AI"},{"Ghost",{{"Character",true}}}});}));
    assert(Rejects([]{HumanNpc::Validate({{"Type","AI"},{"HideWeapon",true}});}));
    assert(Rejects([]{HumanNpc::Validate({{"Type","Human"},{"HideWeapon","yes"}});}));
}
