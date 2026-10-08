#include "Loader/QuestDefinition.h"
#include "Loader/QuestInspectorModel.h"
#include "Loader/QuestProgressText.h"
#include <cassert>
#include <fstream>
using namespace DragonWilds::Quests;
template<class F> bool Rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
Json Example(){return {{"Id","supper"},{"PersistenceID","AAAAAAAAAAAAAAAAAAAAAA"},{"Title","Supper"},
    {"Description","Bring food."},{"Objective",{{"Id","bring_food"},{"Text","Bring three cabbages."},{"Item","/Game/Test/Cabbage.Cabbage"},{"Count",3}}},
    {"Reward",{{"Item","/Game/Test/Potato.Potato"},{"Count",1}}}};}
int main(int argc,char** argv){
    auto data=Example();auto quest=Parse("Test",data);
    assert(quest.Key=="Test:supper" && quest.Required.Count==3 && quest.Reward.Count==1);
    assert(!quest.Marker);
    auto task=data;task["Category"]="Task";assert(Parse("Test",task).Task && !Parse("Test",task).Story);
    data["Objective"]["ProgressText"]="Bring {remaining} more cabbages";
    data["Objective"]["CompleteText"]="All cabbages collected";
    data["Objective"]["AnnounceProgress"]=true;
    const auto progressQuest=Parse("Progress",data);
    assert(progressQuest.AnnounceProgress && ObjectiveProgressText(progressQuest,1)=="Bring 2 more cabbages");
    assert(ObjectiveProgressText(progressQuest,3)=="All cabbages collected");
    data=Example();auto staged=data;staged.erase("Objective");staged["Category"]="Story";staged["Prerequisites"]={"intro"};
    staged["Stages"]=Json::array({{{"Id","supplies"},{"Objectives",Json::array({data["Objective"]})}},
        {{"Id","patrol"},{"Objectives",Json::array({{{"Id","goblins"},{"Text","Defeat goblins"},{"Type","Kill"},{"AIClasses",{"/Game/Test/Goblin.Goblin_C"}},{"Count",2},{"Location",{0,0,0}},{"RadiusMeters",100}}})}}});
    const auto stages=Parse("Test",staged);
    assert(stages.Stages.size()==2 && stages.Story && stages.Prerequisites==std::vector<std::string>{"Test:intro"});
    assert(stages.Stages[1].second[0].ObjectiveId=="patrol:goblins");
    assert(stages.Stages[1].second[0].Marker->Name=="RuneSchema_Quest_AAAAAAAAAAAAAAAAAAAAAA_patrol_goblins");
    assert(!stages.Kill && !stages.Marker);
    const auto timeline=InspectorModel(stages);
    assert(timeline.at("Stages")==Json::array({"supplies","patrol"}));
    assert(timeline.at("Timeline")[0].at("Objectives")[0].at("Kind")=="TurnIn");
    const auto& patrol=timeline.at("Timeline")[1].at("Objectives")[0];
    assert(patrol.at("Counter")=="RuneSchema.Objective:patrol:goblins");
    assert(patrol.at("Required")==2 && patrol.at("Area").at("RadiusMeters")==100);
    const auto single=InspectorModel(quest);
    assert(single.at("Stages").empty() && single.at("Timeline").size()==1);
    assert(single.at("Timeline")[0].at("Objectives")[0].at("Counter")=="bring_food");
    auto invalidStage=staged;invalidStage["Objective"]=data["Objective"];assert(Rejects([&]{Parse("Test",invalidStage);}));
    invalidStage=staged;invalidStage["Stages"][1]["Id"]="supplies";assert(Rejects([&]{Parse("Test",invalidStage);}));
    invalidStage=staged;invalidStage["Stages"][0]["Objectives"].push_back(data["Objective"]);assert(Rejects([&]{Parse("Test",invalidStage);}));
    invalidStage=staged;invalidStage["Stages"][0]["Objectives"][0]["Type"]="Gather";assert(Rejects([&]{Parse("Test",invalidStage);}));
    invalidStage=staged;invalidStage["Prerequisites"]={"supper"};assert(Rejects([&]{Parse("Test",invalidStage);}));
    invalidStage=staged;invalidStage["Stages"][0]["Id"]="__ready";assert(Rejects([&]{Parse("Test",invalidStage);}));
    auto located=data;located["Objective"]["Location"]={16280,187278,-3099};
    const auto marker=Parse("Test",located).Marker;
    assert(marker && marker->Position[2]==-3099 && marker->Name=="RuneSchema_Quest_AAAAAAAAAAAAAAAAAAAAAA_bring_food");
    assert(!marker->RadiusMeters);
    auto area=located;area["Objective"]["RadiusMeters"]=75;
    assert(Parse("Test",area).Marker->RadiusMeters==75);
    assert(RadiusCentimeters(75)==7500 && RadiusCentimeters(0.01)==1 && RadiusCentimeters(10000)==1000000);
    for(const auto& radius:{Json(),Json(true),Json("75"),Json(0),Json(-1),Json(0.001),Json(10001),Json(1e100)}) {
        auto invalid=located;invalid["Objective"]["RadiusMeters"]=radius;
        assert(Rejects([&]{Parse("Test",invalid);}));
    }
    auto missingCenter=data;missingCenter["Objective"]["RadiusMeters"]=75;
    assert(Rejects([&]{Parse("Test",missingCenter);}));
    for(const auto& position:{Json(),Json::array(),Json::array({1,2}),Json::array({1,2,3,4}),Json::array({true,2,3}),Json::array({"1",2,3}),Json::array({1e100,2,3})}) {
        auto invalid=located;invalid["Objective"]["Location"]=position;
        assert(Rejects([&]{Parse("Test",invalid);}));
    }
    for(auto count:{Json(0),Json(-1),Json(1000),Json(true),Json(2.5),Json("2"),Json(UINT64_MAX)}) {
        auto bad=data;bad["Objective"]["Count"]=count;assert(Rejects([&]{Parse("Test",bad);}));
    }
    for(const auto field:{"Id","PersistenceID","Title","Description","Objective","Reward"}) {
        auto bad=data;bad.erase(field);assert(Rejects([&]{Parse("Test",bad);}));
    }
    auto bad=data;bad["PersistenceID"]="AAAAAAAAAAAAAAAAAAAAAB";assert(Rejects([&]{Parse("Test",bad);}));
    bad=data;bad["Objective"]["Item"]="Cabbage";assert(Rejects([&]{Parse("Test",bad);}));
    bad=data;bad["Scope"]="World";assert(Rejects([&]{Parse("Test",bad);}));
    bad=data;bad["Objective"]["Type"]="Wave";assert(Rejects([&]{Parse("Test",bad);}));
    Catalog catalog;catalog.Add("Test",data);
    assert(catalog.Find("Other","Test:supper").Key==quest.Key);
    assert(Rejects([&]{catalog.Find("Other","supper");}));
    assert(Rejects([&]{catalog.Add("Test",data);}));
    assert(Rejects([&]{catalog.Add("Other",data);}));
    bad=data;bad["Id"]="another";assert(Rejects([&]{catalog.Add("Test",bad);}));
    if(argc==2){std::ifstream input(argv[1]);Json supplied;input>>supplied;Parse("FetchQuestTest",supplied);}
}
