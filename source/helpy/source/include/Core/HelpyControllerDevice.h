#pragma once
#include <windows.h>
#include <xinput.h>
#include <array>
#include <string>

namespace PS::HelpyControllerDevice {
enum class Button {A,B,X,Y,Cross,Circle,Square,Triangle,South,East,West,North};
using Labels=std::array<Button,4>;
inline constexpr Labels Xbox{Button::A,Button::B,Button::X,Button::Y},PlayStation{Button::Cross,Button::Circle,Button::Square,Button::Triangle},Nintendo{Button::B,Button::A,Button::Y,Button::X},Generic{Button::South,Button::East,Button::West,Button::North};
struct Reading {bool Connected=false;XINPUT_GAMEPAD Pad{};Labels Glyphs=Generic;bool Guide=false;};
inline bool Active(const XINPUT_GAMEPAD& pad){return pad.wButtons||pad.bLeftTrigger>30||pad.bRightTrigger>30||pad.sThumbLX>7849||pad.sThumbLX<-7849||pad.sThumbLY>7849||pad.sThumbLY<-7849||pad.sThumbRX>8689||pad.sThumbRX<-8689||pad.sThumbRY>8689||pad.sThumbRY<-8689;}
inline Labels Resolve(int style,const Labels& reported){return style==1?Xbox:style==2?PlayStation:style==3?Generic:style==4?Nintendo:reported;}
Reading Read(bool refresh=false);
void Shutdown();
inline std::string StatusText="Controller discovery has not run";
inline const std::string& Status(){return StatusText;}
}
