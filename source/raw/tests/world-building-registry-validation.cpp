#include "Runtime/WorldBuildingRegistryValidation.h"
#include <iostream>
namespace V = PS::WorldBuildingRegistryValidation;
void Need(bool ok) { if (!ok) throw std::runtime_error("test failed"); }
template<class F> void Reject(F operation) {
    try { operation(); } catch (const std::runtime_error&) { return; }
    throw std::runtime_error("incomplete registry accepted");
}
int main() {
    const std::vector<V::Entry> entries{{"vanilla",1},{"dye-station",2}};
    const std::vector<V::Expected> enabled{{"Dye","station","dye-station",2}};
    const std::vector<V::ReverseEntry> reverse{{1,0},{2,1}};
    const std::vector<V::InternalEntry> names{{"vanilla",1},{"dye-station",2}};
    Need(V::Validate(entries,{1,2},enabled,reverse,names).size()==2);
    Reject([&]{V::Validate(entries,{1,2},enabled,{{1,0}},names);});
    Reject([&]{V::Validate(entries,{1,2},enabled,{{1,1},{2,0}},names);});
    // The dedicated server's internal-name map is sparse; every entry it has
    // must be valid, but it need not cover the authoritative network array.
    Need(V::Validate(entries,{1,2},enabled,reverse,{{"vanilla",1}}).size()==2);
    Need(V::Validate(entries,{1,2},enabled,reverse,{}).size()==2);
    Reject([&]{V::Validate(entries,{1,2},enabled,reverse,{{"vanilla",1},{"unknown",3}});});
    Need(V::Validate(entries,{1,2},enabled,reverse,
        {{"vanilla",1},{"dye-station",2},{"dye-alias",2}}).size()==2);
    Need(V::Validate(entries,{1,2},enabled,reverse,
        {{"",1},{"dye-station",2}}).size()==2);
    Reject([&]{V::Validate({{"vanilla",1}},{1},enabled);});
    Need(V::Validate({{"vanilla",1}},{1},enabled,{{1,0}},{{"vanilla",1}},
        V::ExpectedMode::ProtectPendingDefinition).contains("dye-station"));
    // A standalone menu has the same pre-world timing as the dedicated
    // server: all enabled cooked pieces may still be pending registration.
    const std::vector<V::Expected> menuEnabled{{"Dye","dye_station","dye-station",2},
        {"Dye","pigment_mill","pigment-mill",3},
        {"Dye","mint","mint",4}};
    const auto menuLive=V::Validate({{"vanilla",1}},{1},menuEnabled,
        {{1,0}},{{"vanilla",1}},V::ExpectedMode::ProtectPendingDefinition);
    Need(menuLive.size()==4 && menuLive.contains("vanilla")
        && menuLive.contains("dye-station") && menuLive.contains("pigment-mill")
        && menuLive.contains("mint"));
    Reject([&]{V::Validate({{"vanilla",1},{"other",2}},{1,2},enabled,
        {{1,0},{2,1}},{{"vanilla",1}},V::ExpectedMode::ProtectPendingDefinition);});
    Reject([&]{V::Validate({{"vanilla",1}},{1},{{"Dye","station","dye-station",1}},
        {{1,0}},{{"vanilla",1}},V::ExpectedMode::ProtectPendingDefinition);});
    Reject([&]{V::Validate(entries,{1},enabled);});
    // A positively identified enabled definition can reach the persistence
    // map before native network-ID assignment. Never classify it as orphaned.
    Need(V::Validate(entries,{1},enabled,{{1,0}},{{"vanilla",1}},
        V::ExpectedMode::ProtectPendingDefinition).contains("dye-station"));
    Reject([&]{V::Validate(entries,{1,1},enabled);});
    Reject([&]{V::Validate(entries,{1,3},enabled);});
    Reject([&]{V::Validate({{"vanilla",1},{"vanilla",2}},{1,2},{});});
    Reject([&]{V::Validate({{"vanilla",1},{"dye-station",1}},{1,2},enabled);});
    Reject([&]{V::Validate(entries,{1,2},{{"Dye","station","dye-station",3}});});
    Reject([&]{V::Validate({}, {}, enabled);});
    // A disabled/removed mod contributes no expected definition. Its old ID
    // is absent from the validated live set, while vanilla remains present.
    const auto afterRemoval = V::Validate({{"vanilla",1}},{1},{},
        {{1,0}},{{"vanilla",1}});
    Need(afterRemoval.contains("vanilla") && !afterRemoval.contains("dye-station"));
    Need(V::Validate(entries,{1},{},{{1,0}},{{"vanilla",1}}).contains("dye-station"));
    std::cout << "world-building-registry-validation: PASS (including empty auxiliary alias view)\n";
}
