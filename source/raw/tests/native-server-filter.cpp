#include "Core/NativeServerFilterContract.h"
#include <cassert>
#include <iostream>
using namespace PS::NativeServerFilterContract;
template<class F> void Refuses(F function) {bool refused=false;try{function();}catch(const std::exception&){refused=true;}assert(refused);}
int main() {
    assert(ResolveApplyAddress(0x30e25127,0x0c7fb000)==0x05958ff0);
    assert(ResolveApplyAddress(0x194c6c05,0x0c819000)==0x05961180);
    assert(ResolveApplyAddress(0xeee4a2f3,0x0c823000)==0x05962540);
    Refuses([]{ResolveApplyAddress(0x30e25127,0x0c819000);});
    Refuses([]{ResolveApplyAddress(0x194c6c05,0x0c7fb000);});
    Refuses([]{ResolveApplyAddress(0xeee4a2f3,0x0c819000);});
    Refuses([]{ResolveApplyAddress(0x194c6c05,0x0c823000);});
    Refuses([]{ResolveApplyAddress(0x00a58497,0x0de46000);});
    Verify(ApplyCode);
    Refuses([]{Verify(std::span(ApplyCode).first(31));});
    auto changed=ApplyCode;changed[4]^=1;Refuses([&]{Verify(changed);});
    std::cout<<"Native server filter build/signature guards passed\n";
}
