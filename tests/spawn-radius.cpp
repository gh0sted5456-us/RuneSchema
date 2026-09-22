#include "Loader/SpawnRadius.h"
#include "Generator/LoaderCapabilities.h"
#include <cassert>
using nlohmann::json;
using DragonWilds::Spawns::RadiusProperties;
template<class F> bool Rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
int main(){
    assert(RadiusProperties(json::object()).empty());
    const auto result=RadiusProperties({{"SpawnRadiusMeters",100}});
    assert(result.at("MaxDistanceForSpawnPointToSpawn")==10000);
    assert(result.at("MinDistanceForSpawnPointToSpawn")==0);
    assert(result.at("bRequiresActivation")==false && result.at("bOverrideSpawnRadius")==true);
    for(const auto& value:{json("100"),json(true),json(nullptr),json(0),json(-1),json(10001)})
        assert(Rejects([&]{RadiusProperties({{"SpawnRadiusMeters",value}});}));
    for(const auto* field:{"MinSpawnDistance","MaxSpawnDistance"})
        assert(Rejects([&]{RadiusProperties({{"SpawnRadiusMeters",100},{field,10}});}));
    assert(Rejects([]{RadiusProperties({{"SpawnRadiusMeters",100},{"RequiresActivation",true}});}));
    for(const auto* field:{"bRequiresActivation","bOverrideSpawnRadius","MinDistanceForSpawnPointToSpawn","MaxDistanceForSpawnPointToSpawn"})
        assert(Rejects([&]{RadiusProperties({{"SpawnRadiusMeters",100},{"Properties",{{field,0}}}});}));
    assert(RadiusProperties({{"MinSpawnDistance",0},{"MaxSpawnDistance",2000}}).at("MaxDistanceForSpawnPointToSpawn")==2000);
    assert(Rejects([]{RadiusProperties({{"MinSpawnDistance",3000},{"MaxSpawnDistance",2000}});}));
    for(size_t i=1;i<PS::LoaderCapabilities.size();++i)
        assert(std::string_view(PS::LoaderCapabilities[i-1].Name)<PS::LoaderCapabilities[i].Name);
}
