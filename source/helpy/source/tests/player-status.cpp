#include "Core/PlayerStatus.h"
#include <cassert>
#include <iostream>
int main(){
    using namespace PS::PlayerStatus;using namespace std::chrono_literals;
    const Flags flags{1,2,4,8,128};assert(flags.Valid());assert(!Flags{}.Valid());
    assert(Describe(14,flags,false)["Role"]=="member");assert(Describe(14,flags,false)["Privileges"].size()==3);
    assert(Describe(1,flags,false)["Role"]=="admin");assert(Describe(128|14,flags,false)["Role"]=="owner");
    assert(Describe(14,flags,true)["Role"]=="host");assert(Describe(16,flags,false)["Role"]=="unknown");
    assert(Describe(0,{},true)["Role"]=="host");assert(!Describe(0,{},true)["NativeKnown"].get<bool>());
    // Reflected enum values, not a hardcoded storefront/version mask.
    const Flags changed{16,8,4,2,64};assert(changed.Valid());assert(Describe(16,changed,false)["Role"]=="admin");
    auto role=Describe(14,flags,false);assert(Valid(role));auto bad=role;bad["Role"]="admin";bad["NativeKnown"]=false;assert(!Valid(bad));
    bad=role;bad["Privileges"].push_back("GiveItems");assert(!Valid(bad));
    const std::string alice(32,'A'),bob(32,'B');
    const Json rows=Json::array({{{"Path","/host/alice"},{"Name","Same"},{"PlayerId",3},{"Guid",alice},{"RoleStatus",Describe(1,flags,false)}},
        {{"Path","/host/bob"},{"Name","Same"},{"PlayerId",4},{"Guid",bob},{"RoleStatus",role}}});
    Cache cache;const auto now=Cache::Clock::now();auto snapshot=Snapshot(rows,7);
    assert(cache.Apply(snapshot,7,now));assert(cache.Find("/host/alice",alice,now)->at("Role")=="admin");
    assert(cache.Find("",bob,now)->at("Role")=="member");assert(!cache.Find("Same","",now));
    assert(!cache.Find("/host/alice",std::string(32,'C'),now)); // reused name/path cannot inherit another character's badge
    assert(!cache.Apply(snapshot,7,now+8s));assert(!cache.Find("",alice,now+12s)); // replay cannot renew freshness
    auto revoked=rows;revoked[0]["RoleStatus"]=role;snapshot=Snapshot(revoked,7);assert(cache.Apply(snapshot,7,now+9s));
    assert(cache.Find("",alice,now+9s)->at("Role")=="member");
    auto duplicate=rows;duplicate[1]["Guid"]=alice;assert(cache.Apply(Snapshot(duplicate,7),7,now+10s));assert(!cache.Find("",alice,now+10s));
    assert(cache.Find("/host/alice","",now+10s)); // ambiguous map identity never picks a player by name
    assert(cache.Apply(Snapshot(Json::array(),7),7,now+11s));assert(!cache.Find("/host/alice",alice,now+11s));
    assert(!cache.Apply(snapshot,8,now+11s));assert(!cache.Find("",alice,now+11s)); // new world, stale envelope
    std::cout<<"Native role flags, no mask-14 admin, session replacement, expiry and replay tests passed\n";
}
