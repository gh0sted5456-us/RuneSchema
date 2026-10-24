#include "Core/SlowWork.h"
#include <cassert>
int main(){
    PS::SlowWork::Meter firstStall;
    assert(firstStall.Observe(1000,0));assert(!firstStall.Observe(1000,1));
    assert(firstStall.Observe(1000,10000));
    PS::SlowWork::Meter meter;
    assert(!meter.ObserveUs(250,0));
    for(unsigned i=1;i<10000;++i)assert(!meter.ObserveUs(1000,i));
    const auto recurring=meter.ObserveUs(3000,10000);
    assert(recurring&&recurring->Calls==10001&&recurring->PeakUs==3000&&recurring->SlowCalls==1);
    assert(recurring->TotalUs==10002250);
    assert(!meter.Observe(3400,10001));assert(!meter.Observe(3000,19999));
    const auto stalled=meter.Observe(100,20000);
    assert(stalled&&stalled->PeakMs==3400&&stalled->SlowCalls==3&&stalled->Calls==3);
    for(unsigned i=20001;i<=30000;++i)assert(!meter.ObserveUs(10,i));
    assert(!meter.ObserveUs(100,30001));
    const auto resumed=meter.ObserveUs(2500,40000);assert(resumed&&resumed->PeakUs==2500&&resumed->Calls==2);
}
