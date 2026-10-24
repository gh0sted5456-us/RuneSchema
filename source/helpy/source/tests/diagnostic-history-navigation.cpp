#include "Core/DiagnosticHistoryNavigation.h"
#include <cassert>
#include <deque>
#include <optional>
#include <string>
#include <vector>
using namespace PS::DiagnosticHistoryNavigation;
struct Event{std::uint64_t Id;};
int main(){
    const std::vector<std::string> sessions{"s-003","s-002","s-001"};
    assert(AdjacentSession(sessions,"",1).value()=="s-003");
    assert(!AdjacentSession(sessions,"",-1));
    assert(AdjacentSession(sessions,"s-003",-1).value().empty());
    assert(AdjacentSession(sessions,"s-003",1).value()=="s-002");
    assert(AdjacentSession(sessions,"s-002",-1).value()=="s-003");
    assert(AdjacentSession(sessions,"s-002",1).value()=="s-001");
    assert(!AdjacentSession(sessions,"s-001",1));
    assert(!AdjacentSession(sessions,"missing",1));
    assert(!AdjacentSession(sessions,"s-002",0));
    assert(!AdjacentSession({},"",1));
    const std::deque<std::uint64_t> samples{11,22,33};
    assert(SelectedOccurrence(samples,0)==33);
    assert(SelectedOccurrence(samples,22)==22);
    assert(SelectedOccurrence(samples,99)==33);
    assert(AdjacentOccurrence(samples,0,1)==22);
    assert(AdjacentOccurrence(samples,22,1)==11);
    assert(AdjacentOccurrence(samples,22,-1)==33);
    assert(AdjacentOccurrence(samples,11,1)==0);
    assert(AdjacentOccurrence(samples,33,-1)==0);
    assert(SelectedOccurrence({},99)==0);
    assert(AdjacentOccurrence({},0,1)==0);
    assert(AdjacentOccurrence(samples,22,0)==0);
    const std::vector<Event> events{{100},{200},{300}};
    assert(EventIndex(events,200).value()==1);
    assert(!EventIndex(events,0));
    assert(!EventIndex(events,999));
}
