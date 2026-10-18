#include "Core/ItemCatalogFilter.h"
#include "Core/FocusedItemQuery.h"
#include <cassert>
using namespace PS::ItemCatalogFilter;
template<class F> void Reject(F fn){bool failed=false;try{fn();}catch(...){failed=true;}assert(failed);}
int main(){
    const nlohmann::json helm={{"Path","/Dye/Armor/ITEM_Helm.ITEM_Helm"},{"Name","Fallen Hoplite's Helm (Dyed Chrome)"},
        {"Category","Armor"},{"SourceKind","modded"},{"SourceName","Dye"},{"InternalName","chrome_helm"},{"Tags","Metal Heavy"},{"Class","/Script/Dominion.ItemData"}};
    assert(Query("")(helm));assert(Query("Chrome helm")(helm));assert(Query("metal Heavy")(helm));assert(Query("dominion")(helm));
    assert(Query("__helpysource:modded __helpycat:armor __helpyfamily:fallen_hoplite_dyed_chrome chrome")(helm));
    assert(!Query("__helpysource:vanilla")(helm));assert(!Query("__helpycat:weapons")(helm));assert(!Query("__helpyfamily:fallen_hoplite_dyed_gold")(helm));assert(!Query("chrome wood")(helm));
    auto sword=helm;sword["Name"]="Bronze sword";sword["SourceKind"]="vanilla";sword["Category"]="Weapons";
    assert(Query("__helpysource:vanilla __helpycat:weapons bronze")(sword));
    assert(!Query("")(nullptr));Reject([&]{Query invalid("__helpysource:invented");});Reject([&]{Query invalid(std::string(513,'x'));});
    auto health=helm;health["Name"]="Health Tincture";health["PersistenceID"]="tincture-health-42";
    assert(Query("Health")(health)&&Query("tincture health")(health)&&Query("health-42")(health));
    PS::FocusedItemQuery::Misses misses;
    const nlohmann::json pending={{"Rows",nlohmann::json::array()},{"HasMore",true}};
    const nlohmann::json empty={{"Rows",nlohmann::json::array()},{"HasMore",false}};
    const nlohmann::json found={{"Rows",nlohmann::json::array({health})},{"HasMore",true}};
    assert(!misses.NeedsLive("scope1","Health",pending));
    assert(!misses.NeedsLive("scope1","Health",found));
    assert(!misses.NeedsLive("scope1","Health",empty)); // Empty tail is not a cache miss.
    assert(misses.NeedsLive("scope1","uncached",empty));
    assert(!misses.NeedsLive("scope1","",empty)); // Browsing never triggers a focused lookup.
    assert(misses.NeedsLive("scope2","Health",empty)); // World/source changes discard old hit state.
}
