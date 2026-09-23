#include "Loader/QuestSaveOwnership.h"
#include <cassert>
using namespace DragonWilds::Quests;
using nlohmann::json;
template<class F>bool Rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
int main(){
    const std::string id="VT39acMY4k62LnArwSMkEQ";
    const json owned={{"QuestId",id},{"QuestState",2},{"QuestInts",OwnershipVariables("Example",id)},{"UnknownFutureField",23}};
    assert(OwnedBy(owned)=="Example");
    const json vanilla={{"QuestId","vanilla"},{"QuestInts",json::array()},{"Other",true}};
    const json original={{"GameProgress",{{"QuestProgress",{{"Quests",json::array({vanilla,owned})},{"QuestTracked",id},{"QuestLocations",json::array({{{"QuestLocationId","keep"},{"QuestLocationsState",true}}})}}},{"Inventory",{{"do_not_touch",42}}}}},{"meta_data",{{"private","preserve"}}}};
    assert(CleanRemovedQuests(original,{},true).Save==original);
    const auto cleaned=CleanRemovedQuests(original,{"Example"},true);
    assert(cleaned.RemovedQuestIds==std::set<std::string>{id});
    auto expected=original;expected["GameProgress"]["QuestProgress"]["Quests"]=json::array({vanilla});expected["GameProgress"]["QuestProgress"]["QuestTracked"]="";
    assert(cleaned.Save==expected);assert(original["GameProgress"]["QuestProgress"]["Quests"].size()==2);
    assert(Rejects([&]{CleanRemovedQuests(original,{"Example"},false);}));
    auto malformed=owned;malformed["QuestInts"].erase(1);
    assert(Rejects([&]{OwnedBy(malformed);}));
    malformed=owned;malformed["QuestInts"][0]["QuestVariableValue"]=999;
    assert(Rejects([&]{OwnedBy(malformed);}));
    auto duplicate=original;duplicate["GameProgress"]["QuestProgress"]["Quests"].push_back({{"QuestId",id},{"QuestInts",json::array()}});
    assert(Rejects([&]{CleanRemovedQuests(duplicate,{"Example"},true);}));
    assert(CleanRemovedQuests(original,{"DisabledButPresent"},true).Save==original);
}
