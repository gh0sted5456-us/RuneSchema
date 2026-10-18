#include "Loader/QuestScrollIdentity.h"
#include <cassert>
#include <array>
using namespace DragonWilds::QuestScrolls;
int main() {
    const std::string expected="EL786YJnJ_O1OUT1voR1Ww";
    const std::wstring canonical(expected.begin(),expected.end());
    const auto asset=ReadIdentity(canonical.c_str(),23,24);
    assert(asset.Value==expected && IdentityMismatch(asset,expected).empty());
    assert(!asset.Empty && asset.StorageCount==23 && asset.Capacity==24);
    const auto fresh=ReadIdentity<wchar_t>(nullptr,0,0);
    assert(fresh.Empty && !IdentityMismatch(fresh,expected).empty()); // Asset ID must never be empty.
    assert(IdentityMismatch(fresh,expected,true).empty()); // Verified native serialization cache.
    const auto empty=ReadIdentity(L"",1,16);
    assert(empty.Empty && IdentityMismatch(empty,expected,true).empty());
    const auto loaded=ReadIdentity(canonical.c_str(),23,23);
    assert(IdentityMismatch(loaded,expected,true).empty()); // Saved/reloaded cache agrees.
    const auto mismatched=ReadIdentity(L"VT39acMY4k62LnArwSMkEQ",23,24);
    assert(!IdentityMismatch(mismatched,expected,true).empty());
    for(const auto shape:std::array<std::array<int,2>,7>{{{-1,0},{23,22},{0,-1},{1,0},{22,24},{24,24},{1000000,1000000}}}) {
        const auto read=ReadIdentity(canonical.c_str(),shape[0],shape[1]);assert(!IdentityMismatch(read,expected,true).empty());
    }
    assert(!ReadIdentity<wchar_t>(nullptr,23,24).Error.empty());
    assert(!ReadIdentity<wchar_t>(nullptr,0,24).Error.empty());
    auto bad=canonical;bad.push_back(L'x'); // Deliberately non-NUL at index22.
    assert(!ReadIdentity(bad.data(),23,24).Error.empty());
    bad=canonical;bad[5]=0;assert(!ReadIdentity(bad.c_str(),23,24).Error.empty());
    bad=canonical;bad[5]=L'\u0100';assert(!ReadIdentity(bad.c_str(),23,24).Error.empty());
    bad=canonical;bad[5]=L'\n';const auto unsafe=ReadIdentity(bad.c_str(),23,24);
    assert(!unsafe.Error.empty() && unsafe.Value.find('\n')==unsafe.Value.npos);
    bad=canonical;bad[21]=L'B';assert(!ReadIdentity(bad.c_str(),23,24).Error.empty()); // Noncanonical final bits.
    assert(!IdentityMismatch(asset,"not-an-identity").empty());
    assert(AllowEmptySaveIdentity(true,true,false,true,true));
    for(int bit=0;bit<5;++bit) {
        bool save=true,transient=true,net=false,slot=true,native=true;
        if(bit==0)save=false;if(bit==1)transient=false;if(bit==2)net=true;if(bit==3)slot=false;if(bit==4)native=false;
        assert(!AllowEmptySaveIdentity(save,transient,net,slot,native));
    }
    // Reading neither borrows beyond the bounded string nor modifies its ID.
    assert(std::wstring(canonical.c_str())==canonical);
}
