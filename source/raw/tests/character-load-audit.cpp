#include "Core/CharacterLoadAudit.h"
#include <cassert>
#include <thread>
using namespace PS::CharacterLoadAudit;
int main(){
    assert(!Token()&&!End(0));Pruned(5);assert(!Bypassed(Gate::Validation));
    const auto first=Begin();assert(first&&Token()==first&&!Begin());
    Pruned(2);assert(Bypassed(Gate::Validation)&&!Bypassed(Gate::Validation));
    assert(Bypassed(Gate::PlayerState));assert(!End(first+1)&&Token()==first);
    std::uint64_t child=0;std::thread worker([&]{assert(!Token());child=Begin();Pruned(9);const auto result=End(child);assert(result&&result->RegistryPruned==9);});worker.join();
    assert(child!=first&&Token()==first);
    const auto receipt=End(first);assert(receipt&&receipt->RegistryPruned==2&&receipt->ValidationBypasses==2&&receipt->PlayerStateBypasses==1&&!receipt->Ambiguous&&!Token());
    const auto second=Begin();assert(second!=first);Pruned(1);Ambiguous();assert(!Token());Pruned(20);assert(!Bypassed(Gate::PlayerState));
    const auto nested=End(second);assert(nested&&nested->Ambiguous&&nested->RegistryPruned==1&&!nested->PlayerStateBypasses);
    assert(!End(second));const auto third=Begin();assert(End(third)->RegistryPruned==0);
}
