#include "Core/PlayerCarryWeight.h"
#include <cassert>
#include <limits>
int main(){
    using namespace PS::PlayerCarryWeight;
    assert(Verified(1200,1200)&&Verified(1200.001,1200));
    assert(!Verified(560,1200)&&!Verified(std::numeric_limits<double>::quiet_NaN(),1200));
    Owners owners;
    assert(!owners.Record("player",{"Bigger Inventory","CarryWeightMultiplier=3"}));
    const auto conflict=owners.Record("player",{"PlayerProfiles","CarryWeightMultiplier=1.4"});
    assert(conflict&&conflict->Owner=="Bigger Inventory");
    assert(owners.ByPlayer.at("player").Owner=="PlayerProfiles");
    assert(!owners.Record("other-player",{"PlayerProfiles","MaxCarryWeight=400"}));
    assert(!owners.Record("player",{"PlayerProfiles","CarryWeightMultiplier=1.4"}));
}
