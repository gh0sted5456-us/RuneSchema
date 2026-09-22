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
}
