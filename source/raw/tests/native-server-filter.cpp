#include "Core/NativeServerFilterContract.h"
#include <cassert>
#include <iostream>
using namespace PS::NativeServerFilterContract;
template<class F> void Refuses(F function) {bool refused=false;try{function();}catch(const std::exception&){refused=true;}assert(refused);}
int main() {
    assert(Supports(Stamp,ImageSize));
    Verify(Stamp,ImageSize,ApplyCode);
    Refuses([]{Verify(Stamp+1,ImageSize,ApplyCode);});
    Refuses([]{Verify(Stamp,ImageSize+1,ApplyCode);});
    Refuses([]{Verify(0x00a58497,0x0de46000,ApplyCode);});
    Refuses([]{Verify(Stamp,ImageSize,std::span(ApplyCode).first(31));});
    auto changed=ApplyCode;changed[4]^=1;Refuses([&]{Verify(Stamp,ImageSize,changed);});
    std::cout<<"Native server filter build/signature guards passed\n";
}
