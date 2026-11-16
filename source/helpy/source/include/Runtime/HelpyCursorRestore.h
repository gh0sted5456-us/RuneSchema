#pragma once
#include <windows.h>
#include <xinput.h>
#include <atomic>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <cstdint>

namespace PS {
inline bool CapturedGameCall(bool captured,uintptr_t caller,uintptr_t image,size_t imageBytes){return captured&&image&&caller>=image&&caller-image<imageBytes;}
inline UINT MaskBufferedGameInput(bool captured,uintptr_t caller,uintptr_t image,size_t imageBytes,UINT count){return count!=UINT(-1)&&CapturedGameCall(captured,caller,image,imageBytes)?0:count;}
inline bool MaskGameHover(bool captured,uintptr_t caller,uintptr_t image,size_t imageBytes,POINT& point){
    if(!CapturedGameCall(captured,caller,image,imageBytes))return false;
    point={-32000,-32000};return true;
}
template<class Held,class Send>void ReleaseHeldGameInput(Held held,Send send){
    for(const auto button:{VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2})if(held(button)){
        const auto message=button==VK_LBUTTON?WM_LBUTTONUP:button==VK_RBUTTON?WM_RBUTTONUP:button==VK_MBUTTON?WM_MBUTTONUP:WM_XBUTTONUP;
        send(message,button==VK_XBUTTON1?MAKEWPARAM(0,XBUTTON1):button==VK_XBUTTON2?MAKEWPARAM(0,XBUTTON2):0,MAKELPARAM(-32000,-32000));
    }
    for(UINT key=VK_BACK;key<256;++key)if(held(key))send(WM_KEYUP,WPARAM(key),LPARAM(0xc0000001));
}
class HelpyXInputCapture {
    std::atomic<uint32_t> Held=0;
public:
    void Apply(XINPUT_STATE& state,bool captured){
        if(!captured&&!Held.load(std::memory_order_relaxed))return;
        constexpr uint32_t leftStick=1u<<16,rightStick=1u<<17,leftTrigger=1u<<18,rightTrigger=1u<<19;
        auto& pad=state.Gamepad;
        const uint32_t active=pad.wButtons
            |((std::abs(int(pad.sThumbLX))>XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE||std::abs(int(pad.sThumbLY))>XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)?leftStick:0)
            |((std::abs(int(pad.sThumbRX))>XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE||std::abs(int(pad.sThumbRY))>XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)?rightStick:0)
            |(pad.bLeftTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD?leftTrigger:0)
            |(pad.bRightTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD?rightTrigger:0);
        if(captured){Held.store(active,std::memory_order_relaxed);pad={};}
        else{
            const auto blocked=Held.fetch_and(active,std::memory_order_relaxed)&active;
            if(!blocked)return;
            pad.wButtons&=~WORD(blocked);
            if(blocked&leftStick)pad.sThumbLX=pad.sThumbLY=0;
            if(blocked&rightStick)pad.sThumbRX=pad.sThumbRY=0;
            if(blocked&leftTrigger)pad.bLeftTrigger=0;
            if(blocked&rightTrigger)pad.bRightTrigger=0;
        }
        // Force a changed packet when entering or leaving the captured state.
        state.dwPacketNumber^=0x80000000u;
    }
    void Reset(){Held.store(0,std::memory_order_relaxed);}
};
class HelpyCursorRestore {
    std::mutex Gate;
    std::optional<RECT> Requested;
    bool Active=false;
public:
    template<class Read,class Apply> void Begin(Read read,Apply apply){
        std::lock_guard lock(Gate);if(Active)return;
        RECT clip{};Requested=read(clip)?std::optional<RECT>(clip):std::nullopt;
        Active=true;apply(nullptr);
    }
    template<class Apply> bool Request(const RECT* clip,Apply apply){
        std::lock_guard lock(Gate);
        if(Active){Requested=clip?std::optional<RECT>(*clip):std::nullopt;return true;}
        return apply(clip);
    }
    template<class Apply> bool End(bool gameForeground,Apply apply){
        std::lock_guard lock(Gate);if(!Active)return true;Active=false;
        const auto clip=Requested;Requested.reset();
        return !gameForeground||apply(clip?&*clip:nullptr);
    }
};
}
