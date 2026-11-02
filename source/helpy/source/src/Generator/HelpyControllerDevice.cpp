#include "Core/HelpyControllerDevice.h"
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#include <memory>
#include <vector>
#include <cstring>
#include <algorithm>

namespace PS::HelpyControllerDevice {
namespace {
using Handle=std::unique_ptr<SDL_Gamepad,decltype(&SDL_CloseGamepad)>;
struct Device{SDL_JoystickID Id=0;Handle Pad{nullptr,SDL_CloseGamepad};Labels Glyphs=Generic;XINPUT_GAMEPAD Previous{};bool PreviousGuide=false;};
bool Initialized=false,Refresh=true;std::vector<Device> Devices;SDL_JoystickID Selected=0;
auto& Message=StatusText;
Button Label(SDL_GamepadButtonLabel value,Button fallback){
    switch(value){case SDL_GAMEPAD_BUTTON_LABEL_A:return Button::A;case SDL_GAMEPAD_BUTTON_LABEL_B:return Button::B;
    case SDL_GAMEPAD_BUTTON_LABEL_X:return Button::X;case SDL_GAMEPAD_BUTTON_LABEL_Y:return Button::Y;
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS:return Button::Cross;case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE:return Button::Circle;
    case SDL_GAMEPAD_BUTTON_LABEL_SQUARE:return Button::Square;case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE:return Button::Triangle;default:return fallback;}
}
XINPUT_GAMEPAD Sample(SDL_Gamepad* device){
    XINPUT_GAMEPAD pad{};
    for(const auto& [key,button]:std::array<std::pair<SDL_GamepadButton,WORD>,14>{{
        {SDL_GAMEPAD_BUTTON_SOUTH,XINPUT_GAMEPAD_A},{SDL_GAMEPAD_BUTTON_EAST,XINPUT_GAMEPAD_B},
        {SDL_GAMEPAD_BUTTON_WEST,XINPUT_GAMEPAD_X},{SDL_GAMEPAD_BUTTON_NORTH,XINPUT_GAMEPAD_Y},
        {SDL_GAMEPAD_BUTTON_DPAD_UP,XINPUT_GAMEPAD_DPAD_UP},{SDL_GAMEPAD_BUTTON_DPAD_DOWN,XINPUT_GAMEPAD_DPAD_DOWN},
        {SDL_GAMEPAD_BUTTON_DPAD_LEFT,XINPUT_GAMEPAD_DPAD_LEFT},{SDL_GAMEPAD_BUTTON_DPAD_RIGHT,XINPUT_GAMEPAD_DPAD_RIGHT},
        {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,XINPUT_GAMEPAD_LEFT_SHOULDER},{SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,XINPUT_GAMEPAD_RIGHT_SHOULDER},
        {SDL_GAMEPAD_BUTTON_LEFT_STICK,XINPUT_GAMEPAD_LEFT_THUMB},{SDL_GAMEPAD_BUTTON_RIGHT_STICK,XINPUT_GAMEPAD_RIGHT_THUMB},
        {SDL_GAMEPAD_BUTTON_START,XINPUT_GAMEPAD_START},{SDL_GAMEPAD_BUTTON_BACK,XINPUT_GAMEPAD_BACK}}})if(SDL_GetGamepadButton(device,key))pad.wButtons|=button;
    pad.sThumbLX=SDL_GetGamepadAxis(device,SDL_GAMEPAD_AXIS_LEFTX);pad.sThumbRX=SDL_GetGamepadAxis(device,SDL_GAMEPAD_AXIS_RIGHTX);
    const auto invert=[](Sint16 value){return value==-32768?SHORT(32767):SHORT(-value);};
    pad.sThumbLY=invert(SDL_GetGamepadAxis(device,SDL_GAMEPAD_AXIS_LEFTY));pad.sThumbRY=invert(SDL_GetGamepadAxis(device,SDL_GAMEPAD_AXIS_RIGHTY));
    pad.bLeftTrigger=BYTE(std::max(0,int(SDL_GetGamepadAxis(device,SDL_GAMEPAD_AXIS_LEFT_TRIGGER)))*255/32767);
    pad.bRightTrigger=BYTE(std::max(0,int(SDL_GetGamepadAxis(device,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)))*255/32767);return pad;
}
}
Reading Read(bool refresh){
    try{
        if(!Initialized){SDL_SetMainReady();SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
            if(!SDL_InitSubSystem(SDL_INIT_GAMEPAD|SDL_INIT_EVENTS)){Message="Controller service unavailable; XInput fallback remains available";return {};}
            Initialized=true;Refresh=true;}
        SDL_PumpEvents();std::array<SDL_Event,64> events{};if(SDL_PeepEvents(events.data(),int(events.size()),SDL_GETEVENT,SDL_EVENT_GAMEPAD_ADDED,SDL_EVENT_GAMEPAD_REMAPPED)>0)Refresh=true;
        // Helpy owns no SDL windows and polls state; retaining axis events would grow an unused queue.
        SDL_FlushEvents(SDL_EVENT_FIRST,SDL_EVENT_LAST);
        if(refresh||Refresh){
            Refresh=false;int count=0;auto* ids=SDL_GetGamepads(&count);const auto release=[](SDL_JoystickID* value){SDL_free(value);};std::unique_ptr<SDL_JoystickID,decltype(release)> owned(ids,release);
            Devices.clear();for(int i=0;ids&&i<std::min(count,16);++i){Handle pad(SDL_OpenGamepad(ids[i]),SDL_CloseGamepad);if(!pad)continue;
                Labels labels{};for(size_t j=0;j<4;++j)labels[j]=Label(SDL_GetGamepadButtonLabel(pad.get(),SDL_GamepadButton(SDL_GAMEPAD_BUTTON_SOUTH+j)),Generic[j]);
                Devices.push_back({ids[i],std::move(pad),labels});}
            Selected=Devices.empty()?0:Devices.front().Id;
            const auto* name=Devices.empty()?nullptr:SDL_GetGamepadName(Devices.front().Pad.get());
            Message=Devices.empty()?"No mapped controller found; XInput fallback available":std::to_string(Devices.size())+" mapped controller(s); "+(name?name:"Controller");
        }
        SDL_UpdateGamepads();Reading chosen;
        for(auto& device:Devices){if(!SDL_GamepadConnected(device.Pad.get())){Refresh=true;continue;}const auto pad=Sample(device.Pad.get());const bool guide=SDL_GetGamepadButton(device.Pad.get(),SDL_GAMEPAD_BUTTON_GUIDE);
            if(device.Id==Selected)chosen={true,pad,device.Glyphs,guide};
            if((guide&&!device.PreviousGuide)||(Active(pad)&&std::memcmp(&pad,&device.Previous,sizeof(pad))!=0)){Selected=device.Id;chosen={true,pad,device.Glyphs,guide};}
            device.Previous=pad;device.PreviousGuide=guide;
        }return chosen;
    }catch(...){Message="Controller reading unavailable; XInput fallback remains available";return {};}
}
void Shutdown(){Devices.clear();if(Initialized)SDL_QuitSubSystem(SDL_INIT_GAMEPAD|SDL_INIT_EVENTS);Initialized=false;Refresh=true;Selected=0;}
}
