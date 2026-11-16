#include "Core/HelpyControllerBindings.h"
#include <cassert>
int main(){using namespace PS::HelpyControllerBindings;using Json=nlohmann::json;
 const auto defaults=Defaults();auto playstation=defaults;Rebind(playstation,0,XINPUT_GAMEPAD_B);assert(playstation[0]==XINPUT_GAMEPAD_B&&playstation[1]==XINPUT_GAMEPAD_A);
 Json saved={{"playstation",Write(playstation)}};assert(Read(Validate(saved),"playstation")==playstation&&Read(saved,"xbox")==defaults);
 XINPUT_GAMEPAD physical{};physical.wButtons=XINPUT_GAMEPAD_B;physical.sThumbLX=12345;const auto mapped=Apply(physical,playstation);assert((mapped.wButtons&XINPUT_GAMEPAD_A)&&!(mapped.wButtons&XINPUT_GAMEPAD_B)&&mapped.sThumbLX==12345);
 assert(PS::HelpyControllerDevice::Resolve(0,PS::HelpyControllerDevice::PlayStation)==PS::HelpyControllerDevice::PlayStation);
 Rebind(playstation,0,PS::InputBindings::LeftTrigger);saved["playstation"]=Write(playstation);assert(Read(saved,"playstation")==playstation);physical={};physical.bLeftTrigger=220;assert(Apply(physical,playstation).wButtons&XINPUT_GAMEPAD_A);
 auto invalid=saved;invalid["playstation"]["cancel"]="LT";bool rejected=false;try{Validate(invalid);}catch(...){rejected=true;}assert(rejected);
 rejected=false;try{Rebind(playstation,0,PS::InputBindings::Guide);}catch(...){rejected=true;}assert(rejected);
 const auto roundtrip=Json::parse(saved.dump());assert(Read(roundtrip,"playstation")==playstation&&Read(roundtrip,"nintendo")==defaults);
}
