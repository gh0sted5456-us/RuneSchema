#include "Loader/RecipeUnlockPolicy.h"
#include <cassert>
int main() {
    using PS::RecipeUnlockPolicy::Automatic;
    using nlohmann::json;
    assert(!Automatic(json::object()));
    assert(Automatic({{"Unlock",true}}));
    assert(!Automatic({{"Unlock",false}}));
    assert(!Automatic({{"Unlock",false},{"AddTo",json::array({{{"Category","Weapons"}}})}}));
    assert(!Automatic({{"AddTo",json::array({{{"Category","Weapons"}}})}}));
    bool rejected=false; try { Automatic({{"Unlock","false"}}); } catch(const std::exception&) { rejected=true; }
    assert(rejected);
    using PS::RecipeUnlockPolicy::LearnOnce;
    assert(LearnOnce(false,false).AddVisible&&!LearnOnce(false,false).RemoveExclusion);
    assert(!LearnOnce(true,false).AddVisible&&!LearnOnce(true,false).RemoveExclusion);
    assert(!LearnOnce(true,true).AddVisible&&LearnOnce(true,true).RemoveExclusion);
    assert(LearnOnce(false,true).AddVisible&&LearnOnce(false,true).RemoveExclusion);
    // No process-wide once flag: a new character still receives its first grant.
    assert(LearnOnce(false,false).AddVisible);
}
