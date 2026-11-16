#pragma once
#include "Core/HelpyControllerDevice.h"
#include <cmath>
namespace PS {
struct HelpyTouchpadTap {
    uint32_t Device=0;bool Armed=false,Down=false,Candidate=false;double Started=0;float X=0,Y=0;
    bool Poll(bool enabled,uint32_t device,const HelpyControllerDevice::Touchpad& touch,double now){
        if(!enabled||Device!=device){*this={};Device=device;if(!enabled)return false;}
        if(!touch.Down){const bool click=Down&&Candidate&&!touch.Clicked&&now>=Started&&now-Started<=.25;Down=false;Candidate=false;Armed=!touch.Clicked;return click;}
        if(!Down){Down=true;Started=now;X=touch.X;Y=touch.Y;Candidate=Armed&&touch.Fingers==1&&!touch.Clicked;Armed=false;}
        const float dx=touch.X-X,dy=touch.Y-Y;
        if(touch.Fingers!=1||touch.Clicked||!std::isfinite(dx)||!std::isfinite(dy)||dx*dx+dy*dy>.015f*.015f||now<Started)Candidate=false;
        return false;
    }
};
}
