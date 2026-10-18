#include "Loader/QuestDefinition.h"
#include "Loader/QuestInspectorModel.h"
#include <cassert>
using namespace DragonWilds::Quests;
template<class F>bool Rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
int main(){
    assert(RegionName("EQuestRegion::Brynmoor")=="Brynmoor");
    assert(ResolveRegion("Brynmoor",{{"EQuestRegion::Brynmoor",19},{"Ghornfell",7}})==19);
    assert(ResolveRegion("Future_Location",{{"Future_Location",121}})==121);
    assert(Rejects([]{RegionName("None");})&&Rejects([]{RegionName("MAX");}));
    assert(Rejects([]{ResolveRegion("Not_In_Game",{{"Brynmoor",1}});}));
    assert(Rejects([]{ResolveRegion("Brynmoor",{{"Brynmoor",1},{"Brynmoor",2}});}));
    assert(Rejects([]{ResolveRegion("Brynmoor",{{"Brynmoor",256}});}));
    Json doc={{"Id","region-test"},{"PersistenceID","VT39acMY4k62LnArwSMkEQ"},{"Title","Test"},{"Description","Test"},
        {"Objective",{{"Id","collect"},{"Text","Collect"},{"Item","/Game/Item.Item"},{"Count",1}}},
        {"Reward",{{"Item","/Game/Reward.Reward"},{"Count",1}}}};
    assert(Parse("Test",doc).Region.empty());
    doc["Region"]="Brynmoor";const auto quest=Parse("Test",doc);assert(quest.Region=="Brynmoor"&&InspectorModel(quest).at("Region")=="Brynmoor");
    doc["Region"]="Future_Location";assert(Parse("Test",doc).Region=="Future_Location");
    doc["Region"]=1;assert(Rejects([&]{Parse("Test",doc);}));
    doc["Region"]="a location";assert(Rejects([&]{Parse("Test",doc);}));
}
