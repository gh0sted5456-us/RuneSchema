#include "Loader/GhostScope.h"
#include "Generator/PlayerTraceOptions.h"
#include <iostream>
#include <stdexcept>
using nlohmann::json;
int main() {
    unsigned checks=0;
    auto expect=[&](json input,bool valid) {
        bool accepted=true;try { PS::PlayerTrace::ValidateOptions(input); }catch(...) { accepted=false; }
        if(accepted!=valid)throw std::runtime_error("Trace option validation failed");++checks;
    };
    expect(json::object(),true);
    expect({{"Seconds",60},{"MaxEvents",4096},{"Categories",31},{"Filter","OnEquip"},{"SuppressTicks",false}},true);
    for(const auto& key:{"Seconds","MaxEvents","Categories"}) {
        expect({{key,0}},false);expect({{key,-1}},false);expect({{key,1.5}},false);expect({{key,"30"}},false);
    }
    expect({{"Seconds",61}},false);expect({{"MaxEvents",4097}},false);expect({{"Categories",32}},false);
    expect({{"Filter",std::string(128,'a')}},true);expect({{"Filter",std::string(129,'a')}},false);
    expect({{"SuppressTicks",1}},false);expect({{"Unknown",true}},false);
    if(DragonWilds::PlayerMeshOnly(json::object()))throw std::runtime_error("Legacy scope changed");++checks;
    if(!DragonWilds::PlayerMeshOnly({{"Target","PlayerMesh"}}))throw std::runtime_error("Player mesh scope");++checks;
    if(DragonWilds::PlayerMeshOnly({{"Target","EntirePerson"}}))throw std::runtime_error("Entire person scope");++checks;
    for(const json& value:{json(true),json(1),json(nullptr),json("playerMesh"),json("All")}) {
        bool rejected=false;try{DragonWilds::PlayerMeshOnly({{"Target",value}});}catch(...){rejected=true;}
        if(!rejected)throw std::runtime_error("Invalid ghost scope accepted");++checks;
    }
    std::cout<<checks<<" ghost/trace option checks passed\n";
}
