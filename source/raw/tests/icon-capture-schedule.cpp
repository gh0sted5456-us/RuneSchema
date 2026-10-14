#include "Core/IconCaptureSchedule.h"
#include <stdexcept>
static void Require(bool value){if(!value)throw std::runtime_error("Icon capture scheduling regression");}
int main(){
    PS::IconCapture::Schedule timing;timing.Begin(1000);
    Require(!timing.CanDraw(1000)&&!timing.CanRead(1000));
    Require(!timing.CanDraw(1119)&&timing.CanDraw(1120));
    timing.Submitted(1120);Require(!timing.CanDraw(1120)&&!timing.CanRead(1120));
    Require(!timing.CanRead(1151)&&timing.CanRead(1152));
    Require(timing.Blank(1152)&&!timing.CanRead(1152)&&!timing.CanDraw(1152));
    Require(timing.CanDraw(1232));
    for(unsigned i=0;i<4;++i)Require(timing.Blank(1300+i));
    Require(!timing.Blank(1400));
    timing.Begin(2000);Require(timing.Expired(7000)&&!timing.CanDraw(7000)&&!timing.CanRead(7000));
}
