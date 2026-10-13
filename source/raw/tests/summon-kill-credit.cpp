#include "Loader/SummonKillCreditPolicy.h"
#include <cassert>
using namespace DragonWilds::Quests::Summons;
int main() {
    // Direct class, scroll, and raised children share one native contract.
    for(int attacker:{10,11,12}) {
        assert(MatchesEnrollment(attacker,1,1,1,1,attacker,true));
        assert(!MatchesEnrollment(attacker,1,2,1,1,attacker,true));
        assert(!MatchesEnrollment(attacker,1,1,2,1,attacker,true));
        assert(!MatchesEnrollment(attacker,1,1,1,2,attacker,true));
        assert(!MatchesEnrollment(attacker,1,1,1,1,99,true));
        assert(!MatchesEnrollment(attacker,1,1,1,1,attacker,false));
        // An AI-owned child cannot inherit its parent's ownership certificate.
        assert(!MatchesEnrollment(attacker,1,10,10,10,attacker,true));
    }
    assert(!MatchesEnrollment(0,1,1,1,1,0,true));
    assert(!MatchesEnrollment(1,1,1,1,1,1,true));
    for(int mask=0;mask<8;++mask) {
        const NativeAllegiance state{bool(mask&1),bool(mask&2),bool(mask&4)};
        assert(state.Certified()==(mask==7));
    }
    NativeDispatches ledger;
    const ObjectIdentity world{1,100},victim{2,101},player{3,102};
    assert(ledger.Allow(1,world,victim,player,true));
    // Point/general fatal notifications and native callbacks cannot replay a
    // summon reward; neither can a nested caster-normalized callback.
    assert(!ledger.Allow(1,world,victim,player,true));
    assert(!ledger.Allow(1,world,victim,player,false));
    const ObjectIdentity reusedSlot{2,103};
    assert(ledger.Allow(1,world,reusedSlot,player,true));
    assert(ledger.Allow(2,world,victim,player,true));
    assert(ledger.Allow(2,{1,104},victim,player,true));
    assert(!ledger.Allow(2,{1,104},victim,player,true));
    assert(!ledger.Allow(0,world,victim,player,true));
    assert(!ledger.Allow(2,world,{-1,0},player,true));
    assert(!ledger.Allow(2,world,victim,{3,0},true));
    NativeDispatches mixed;
    assert(mixed.Allow(1,world,victim,player,false));
    assert(mixed.Allow(1,world,victim,player,false)); // preserve vanilla repeats
    assert(!mixed.Allow(1,world,victim,player,true));
    mixed.Prune([&](ObjectIdentity identity){return identity!=victim;});
    assert(mixed.Allow(1,world,victim,player,true));
    // A later pre-hook vetoed the original. No award happened, so retry may run.
    mixed.Cancel(1,world,victim,player);
    assert(mixed.Allow(1,world,victim,player,true));
    mixed.Cancel(0,world,victim,player); // stale cancellation cannot erase credit
    assert(!mixed.Allow(1,world,victim,player,true));
}
