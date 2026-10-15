#include "Core/IconDrawContext.h"
#include <array>
#include <cassert>
using PS::IconCapture::TransferDrawContext;
template<class F> void Reject(F&& fn){bool rejected=false;try{fn();}catch(const std::runtime_error&){rejected=true;}assert(rejected);}
int main(){
    // Reflected object handle plus unreflected native bookkeeping must survive.
    std::array<std::uint8_t,24> source{},destination{};source[0]=17;source[16]=98;source[23]=201;
    const auto expected=source;TransferDrawContext(destination,source,true);
    assert(destination==expected);for(auto byte:source)assert(byte==0);
    Reject([&]{TransferDrawContext(destination,source,true);}); // no overwrite
    Reject([&]{TransferDrawContext(source,source,true);});
    std::array<std::uint8_t,23> shorter{};
    Reject([&]{TransferDrawContext(shorter,source,true);});
    Reject([&]{TransferDrawContext(source,destination,false);});
    std::array<std::uint8_t,257> oversized{},other{};
    Reject([&]{TransferDrawContext(oversized,other,true);});
    Reject([&]{TransferDrawContext(std::span(source).subspan(1,8),std::span(source).first(8),true);});
    Reject([&]{TransferDrawContext({},{},true);});
}
