#include "Loader/VendorSpawnGate.h"
#include "Loader/PinnedObjectIdentity.h"
#include "Utility/BuildInfo.h"
#include "Loader/VendorIdentity.h"
#include "Loader/VendorTraceBudget.h"
#include "Loader/VendorAcknowledgement.h"
#include "Loader/VendorOffers.h"
#include <cassert>
#include <string>

int main() {
    namespace Ack = DragonWilds::VendorAcknowledgement;
    assert(Ack::PrimaryPress(false, false));
    assert(!Ack::PrimaryPress(true, false));
    assert(!Ack::PrimaryPress(false, true));
    assert(!Ack::PrimaryPress(true, true));
    assert(Ack::ValidLayout(10, {{{0,8},{8,1},{9,1}}}));
    assert(Ack::ValidLayout(16, {{{8,8},{0,1},{1,1}}})); // reflected offsets, not hardcoded
    assert(!Ack::ValidLayout(9, {{{0,8},{8,1},{9,1}}}));
    assert(!Ack::ValidLayout(10, {{{0,8},{7,1},{9,1}}}));
    assert(!Ack::ValidLayout(10, {{{0,8},{8,0},{9,1}}}));
    assert(!Ack::ValidLayout(0, {{{0,8},{8,1},{9,1}}}));
    assert(!Ack::ValidLayout(65, {{{0,8},{8,1},{9,1}}}));
    DragonWilds::VendorTraceBudget trace;
    assert(trace.Admit("cow:Interaction:First"));
    for (int repeat = 0; repeat < 10000; ++repeat)
        assert(!trace.Admit("cow:Interaction:First"));
    assert(!trace.Exhausted());
    for (unsigned event = 1; event < trace.Limit; ++event)
        assert(trace.Admit("event:" + std::to_string(event)));
    assert(trace.Exhausted() && !trace.Admit("overflow"));
    trace.Reset();
    assert(!trace.Exhausted() && trace.Admit("cow:Interaction:First"));
    assert(std::string(PS::BuildInfo::Version) == "0.7.5.14");
    assert(std::string(PS::BuildInfo::Name) == "RuneSchema 0.7.5.14 Universal");
    const auto merchantId = DragonWilds::VendorIdentity::ForOwner(
        DragonWilds::VendorOffers::Owner("RuneSchema2VendorTest", "cabbage_trader"));
    assert(merchantId == DragonWilds::VendorIdentity::ForOwner(
        DragonWilds::VendorOffers::Owner("RuneSchema2VendorTest", "cabbage_trader")));
    assert(merchantId != DragonWilds::VendorIdentity::ForOwner(
        DragonWilds::VendorOffers::Owner("RuneSchema2VendorTest", "other_trader")));
    assert(merchantId != DragonWilds::VendorIdentity::ForOwner(
        DragonWilds::VendorOffers::Owner("OtherMod", "cabbage_trader")));
    assert(merchantId[0] != 0x48435352); // /spawns orphan sweep must not own vendors
    DragonWilds::VendorScanBudget budget;
    for (int menuTick = 0; menuTick < 10000; ++menuTick) assert(!budget.Begin(false));
    assert(budget.Passes() == 0 && !budget.Exhausted());
    for (unsigned pass = 0; pass < budget.Limit; ++pass) assert(budget.Begin(true));
    assert(budget.Exhausted() && !budget.Begin(true));
    budget.ResetForMap();
    assert(budget.Passes() == 0 && budget.Begin(true));
    using DragonWilds::VendorSpawnGate;
    VendorSpawnGate first, second;
    assert(first.Pending());
    // Waiting for assets does not consume the single construction attempt.
    for (int scan = 0; scan < 40; ++scan) assert(first.Pending());
    assert(first.Begin());
    // A failed attachment, a success, and an expired actor weak reference all
    // leave the same gate closed. Event volume cannot cause another attempt.
    for (int event = 0; event < 20000; ++event) {
        assert(!first.Pending());
        assert(!first.Begin());
    }
    assert(second.Begin()); // vendor isolation
    first.ResetForMap();
    assert(first.Pending());
    assert(first.Begin());
    assert(!first.Begin());
    assert(!second.Pending());
    int original = 0, other = 0;
    using DragonWilds::PinnedObjectSlotMatches;
    // An engine object with no allocated weak serial must remain identifiable
    // while its explicit root and object-array slot are valid.
    assert(PinnedObjectSlotMatches(&original, &original, true, true));
    assert(!PinnedObjectSlotMatches(&original, &other, true, true));
    assert(!PinnedObjectSlotMatches(&original, nullptr, true, true));
    assert(!PinnedObjectSlotMatches(nullptr, nullptr, true, true));
    assert(!PinnedObjectSlotMatches(&original, &original, false, true));
    assert(!PinnedObjectSlotMatches(&original, &original, true, false));
}
