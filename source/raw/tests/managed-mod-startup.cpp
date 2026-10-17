#include "Core/ManagedModStartup.h"
#include <cassert>
int main(){
    using namespace PS::ManagedModStartup;
    Queue queue;
    assert(SafeName(L"MaxCape")&&!SafeName(L"bad\tname")&&!SafeName(L"bad\x01name")&&!SafeName(L"bad.")&&!SafeName(L"bad "));
    assert(PackageRelative(L"ue4ss/MaxCape",L"MaxCape",L"Owner"));
    assert(PackageRelative(L"ue4ss",L"Owner",L"Owner"));
    assert(!PackageRelative(L"ue4ss",L"Different",L"Owner"));
    assert(!PackageRelative(L"assets/MaxCape",L"MaxCape",L"Owner"));
    assert(!PackageRelative(L"ue4ss/MaxCape/other",L"MaxCape",L"Owner"));
    assert(!PackageRelative(L"../Other/ue4ss/MaxCape",L"MaxCape",L"Owner"));
    assert(!PackageRelative(L"ue4ss/Wrong",L"MaxCape",L"Owner"));
    assert(queue.Add({L"Owner",L"MaxCape",L"X:/Owner/ue4ss/MaxCape",true,false})==Result::Queued);
    assert(queue.Add({L"Other",L"maxcape",L"X:/Other/ue4ss/maxcape",false,true})==Result::Collision);
    auto initial=queue.Take();assert(initial.size()==1&&queue.Take().empty());
    assert(queue.Owns(L"MAXCAPE")&&!queue.Owns(L"RuneSchema"));
    // New registrations are appended to a queue, never the host's active
    // vector while a constructor/start callback is iterating it.
    assert(queue.Add({L"Owner",L"Native",L"X:/Owner/ue4ss/Native",false,true})==Result::Queued);
    queue.Confirm(initial.front());assert(queue.Snapshot().size()==1);
    auto next=queue.Take();assert(next.size()==1&&next[0].Native);
    queue.Close();assert(queue.Add(next[0])==Result::TooLate);
    queue.Reset();assert(queue.Snapshot().empty());assert(queue.Add(next[0])==Result::Queued);
    assert(queue.Add({L"Owner",L"Empty",L"X:/Empty",false,false})==Result::Invalid);
}
