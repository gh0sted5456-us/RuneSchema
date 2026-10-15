#include "Core/CharacterEntryBypassPlan.h"
#include <array>
#include <cassert>
using namespace PS::CharacterEntryBypassPlan;
int main(){
    std::array<std::uint8_t,96> code{};code.fill(0x90);
    const auto set=[&](std::size_t at,std::uint32_t target){
        code[at]=0xe8;const std::int32_t relative=static_cast<std::int32_t>(target)-static_cast<std::int32_t>(0x3000+at+5);
        std::memcpy(code.data()+at+1,&relative,4);code[at+5]=0x84;code[at+6]=0xc0;code[at+7]=0x0f;code[at+8]=0x85;
        const std::int32_t jump=12;std::memcpy(code.data()+at+9,&jump,4);
    };
    set(8,0x1000);const auto gate=Find(code,0x3000,0x1000);
    assert(gate&&gate->Call==0x3008&&gate->Test==0x300d&&gate->Success==0x3021);
    assert(!Find(code,0x3000,0x1001));
    assert(!Find({code.data(),20},0x3000,0x1000));
    code[15]=0x75;code[16]=12;assert(Find(code,0x3000,0x1000));
    code[16]=0x7f;assert(!Find(code,0x3000,0x1000));
    set(8,0x1000);code[16]=0x84;assert(!Find(code,0x3000,0x1000));
    set(8,0x1000);set(48,0x1000);assert(!Find(code,0x3000,0x1000));
    assert(AcceptRejected(0)==1&&AcceptRejected(1)==1&&AcceptRejected(2)==2);
    assert(AcceptRejected(0x12345600)==0x12345601&&AcceptRejected(0x123456ff)==0x123456ff);
}
