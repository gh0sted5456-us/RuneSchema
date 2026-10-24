#include "Core/NativePersistencePolicy.h"
#include <cassert>
#include <stdexcept>
#include <vector>
using namespace PS::NativePersistencePolicy;
int main(){
    for(bool bypass:{false,true})for(const auto* mode:{"vanilla","lessRestrictive","quickLaunch"}){
        assert(SessionRequested(false,bypass,mode)==(std::string_view(mode)!="vanilla"));
        assert(SessionRequested(true,bypass,mode));
        assert(BypassRequested(false,bypass,mode)==(bypass||std::string_view(mode)!="vanilla"));
        assert(BypassRequested(true,bypass,mode));
        // Direct native loads have no reflected preflight token; rejection fallback still applies.
        assert(BypassRequested(false,bypass,mode)==(SessionRequested(false,bypass,mode)||bypass));
    }
    assert(Parse(" 0 ")==false&&Parse("True")==true&&Parse("1")==true);
    assert(!Parse("")&&!Parse("missing")&&!Parse("42")&&!Parse("false; other.command"));
    Lease lease;bool strict=true;int reads=0;std::vector<bool> writes;
    const auto read=[&]()->std::optional<bool>{++reads;return strict;};
    const auto write=[&](bool value){writes.push_back(value);strict=value;};
    assert(Acquire(lease,false,read,write)==Outcome::Disabled&&reads==0&&writes.empty());
    assert(Acquire(lease,true,[]{return std::optional<bool>{};},write)==Outcome::Unavailable&&writes.empty());
    strict=false;assert(Acquire(lease,true,read,write)==Outcome::AlreadyTolerant&&!lease.RestoreNeeded&&writes.empty());
    strict=true;assert(Acquire(lease,true,read,write)==Outcome::Applied&&!strict&&lease.RestoreNeeded);
    const auto acquiredReads=reads;assert(Acquire(lease,true,read,write)==Outcome::Failed&&reads==acquiredReads);
    assert(Release(lease,read,write)==Outcome::Restored&&strict&&!lease.RestoreNeeded);
    const auto releasedWrites=writes.size();assert(Release(lease,read,write)==Outcome::Restored&&writes.size()==releasedWrites);
    // A read-only native variable must never be reported as tolerant.
    assert(Acquire(lease,true,read,[](bool){})==Outcome::Failed&&strict&&!lease.RestoreNeeded);
    // A setter can take effect and then throw. Roll back, do not falsely report success.
    const auto throwsAfterWrite=[&](bool value){strict=value;if(!value)throw std::runtime_error("setter failed after mutation");};
    assert(Acquire(lease,true,read,throwsAfterWrite)==Outcome::Failed&&strict&&!lease.RestoreNeeded);
    const auto rejectsWrites=[](bool){throw std::runtime_error("unavailable");};
    strict=false;lease.RestoreNeeded=true;
    assert(Release(lease,read,rejectsWrites)==Outcome::RestoreFailed&&lease.RestoreNeeded);
    assert(Release(lease,read,write)==Outcome::Restored&&strict&&!lease.RestoreNeeded);
    // Another native owner already restored strict=true: don't write over it.
    lease.RestoreNeeded=true;const auto prior=writes.size();
    assert(Release(lease,read,write)==Outcome::Restored&&writes.size()==prior);
    // Repeated loads don't accumulate leases or leave the process in tolerant mode.
    for(int i=0;i<100;++i){assert(Acquire(lease,true,read,write)==Outcome::Applied);assert(Release(lease,read,write)==Outcome::Restored);assert(strict);}
    // Session policy survives repeated loads, menu return and Quick Launch cancellation.
    writes.clear();strict=true;lease={};
    assert(Hold(lease,false,read,write)==Outcome::Disabled&&writes.empty()&&strict);
    assert(Hold(lease,true,read,write)==Outcome::Applied&&!strict&&lease.RestoreNeeded);
    for(int i=0;i<100;++i)assert(Hold(lease,true,read,write)==Outcome::AlreadyTolerant&&!strict);
    assert(writes.size()==1);strict=true; // A native world transition resets the variable.
    assert(Hold(lease,true,read,write)==Outcome::Applied&&!strict&&writes.size()==2);
    assert(Hold(lease,true,[]{return std::optional<bool>{};},write)==Outcome::Unavailable&&lease.RestoreNeeded);
    assert(Release(lease,read,write)==Outcome::Restored&&strict&&!lease.RestoreNeeded);
    strict=false;writes.clear();assert(Hold(lease,true,read,write)==Outcome::AlreadyTolerant);
    assert(Release(lease,read,write)==Outcome::Restored&&!strict&&writes.empty());
}
