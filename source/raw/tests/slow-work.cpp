#include "Core/SlowWork.h"
#include <cassert>
int main(){
    PS::SlowWork::Meter meter;
    assert(!meter.Observe(99,0));
    const auto first=meter.Observe(3300,0);assert(first&&first->PeakMs==3300&&first->SlowCalls==1);
    assert(!meter.Observe(3400,100));assert(!meter.Observe(3000,9999));
    const auto next=meter.Observe(100,10000);assert(next&&next->PeakMs==3400&&next->SlowCalls==3);
    for(unsigned i=0;i<100000;++i)assert(!meter.Observe(1,10001+i));
    const auto resumed=meter.Observe(120,120000);assert(resumed&&resumed->SlowCalls==1&&resumed->PeakMs==120);
}
