#include "Core/IconCaptureSchedule.h"
#include <stdexcept>
static void Require(bool value){if(!value)throw std::runtime_error("Icon capture scheduling regression");}
int main(){
    PS::IconCapture::Schedule timing;timing.Begin(1000);
    Require(!timing.CanDraw(1000)&&!timing.CanRead(1000)&&!timing.Opaque());
    Require(!timing.CanDraw(1015)&&timing.CanDraw(1016));
    timing.Submitted(1016);Require(!timing.CanDraw(1016)&&!timing.CanRead(1016));
    Require(!timing.CanRead(1047)&&timing.CanRead(1048));
    Require(timing.Blank(1048)&&!timing.CanRead(1048)&&!timing.CanDraw(1048));
    Require(timing.Opaque());
    Require(timing.CanDraw(1128));
    for(unsigned i=0;i<4;++i)Require(timing.Blank(1300+i));
    Require(!timing.Blank(1400));
    timing.Begin(2000);Require(!timing.Opaque());Require(timing.Expired(7000)&&!timing.CanDraw(7000)&&!timing.CanRead(7000));
    Require(PS::IconCapture::Retryable("Item icon capture texture expired")&&PS::IconCapture::Retryable("ReadRenderTarget produced a blank item icon after bounded warm-up"));
    Require(!PS::IconCapture::Retryable("Icon source is not registered")&&!PS::IconCapture::Retryable("stage=root-load: missing asset"));
}
