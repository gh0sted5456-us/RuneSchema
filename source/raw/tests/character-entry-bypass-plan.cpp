#include "Core/CharacterEntryBypassPlan.h"
#include <array>
#include <cassert>
using namespace PS::CharacterEntryBypassPlan;
int main(){
    // Native work/result -> common restore tail -> RET; later blocks can jump
    // backwards into the same epilogue (the current Steam loader does this).
    const std::array<Instruction,7> valid={{{0,5,Kind::Other},{5,2,Kind::Other},
        {7,5,Kind::Restore},{12,4,Kind::StackRestore},{16,1,Kind::Return},
        {17,2,Kind::Other},{19,2,Kind::Branch,7}}};
    assert(FindReturn(valid,21)==7);
    auto code=valid;code[6].Target=12;assert(!FindReturn(code,21)); // skip acceptance
    code=valid;code[6].Target=8;assert(!FindReturn(code,21)); // instruction interior
    code=valid;code[6].Target=21;assert(!FindReturn(code,21)); // unknown tail exit
    code=valid;code[6].Target=-1;assert(!FindReturn(code,21));
    code=valid;code[5].Type=Kind::Return;assert(!FindReturn(code,21));
    code=valid;code[4].Type=Kind::Other;assert(!FindReturn(code,21));
    code=valid;code[3].Type=Kind::Other;assert(!FindReturn(code,21)); // no stack restore
    code=valid;code[2].Type=Kind::Other;assert(!FindReturn(code,21)); // short patch tail
    code=valid;code[0].Type=Kind::Invalid;assert(!FindReturn(code,21));
    code=valid;code[2].Offset=8;assert(!FindReturn(code,21));
    code=valid;code[2].Size=16;assert(!FindReturn(code,21));
    code=valid;code[2].Size=0;assert(!FindReturn(code,21));
    assert(!FindReturn(valid,20)&&!FindReturn(valid,22)&&!FindReturn({},0));
    assert(PlayerStatePatterns.size()==2);
    assert(AcceptRejected(0)==1&&AcceptRejected(1)==1&&AcceptRejected(2)==2);
    assert(AcceptRejected(0x12345600)==0x12345601&&AcceptRejected(0x123456ff)==0x123456ff);
}
