#include "Loader/VendorPolicy.h"
#include <cassert>
using namespace DragonWilds::VendorPolicy;
int main() {
    assert(Stage(nlohmann::json::object()) == VendorStage::Visual);
    assert(Stage({{"Stage","Interaction"}}) == VendorStage::Interaction);
    assert(Stage({{"Stage","Merchant"}}) == VendorStage::Merchant);
    bool rejected=false; try { Stage({{"Stage","All"}}); } catch (...) { rejected=true; }
    assert(rejected);
}
