#pragma once
#include <windows.h>
#include <xinput.h>
#include <cstdint>

namespace PS::HelpyControllerShortcut {
inline constexpr UINT GuideMessage=WM_APP+0x524;
inline constexpr int Disabled=0,Guide=1,MenuView=2;
struct Gesture {
    int Mode=-1;bool Down=false,Allowed=false,Fired=false;uint64_t Last=0;
    bool Fire(uint64_t now){
        // SDL and the game bridge can report the same physical Guide press.
        if(Fired&&(now<Last||now-Last<250))return false;
        Last=now;Fired=true;return true;
    }
    bool Poll(int mode,bool guide,WORD buttons,bool allowed,uint64_t now){
        const bool down=mode==Guide?guide:mode==MenuView&&(buttons&(XINPUT_GAMEPAD_START|XINPUT_GAMEPAD_BACK))==(XINPUT_GAMEPAD_START|XINPUT_GAMEPAD_BACK);
        const bool changed=mode!=Mode||allowed!=Allowed;Mode=mode;Allowed=allowed;
        const bool pressed=down&&!Down;Down=down;
        return !changed&&allowed&&pressed&&Fire(now);
    }
    bool Native(int mode,bool allowed,uint64_t now){return mode==Guide&&allowed&&Fire(now);}
};
}
