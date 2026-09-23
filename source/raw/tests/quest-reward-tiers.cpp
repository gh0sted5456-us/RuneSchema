#include "Loader/QuestDefinition.h"
#include "Loader/DialogueSaveIdentity.h"
#include "Loader/QuestAcquisition.h"
#include "Loader/QuestProgressText.h"
#include "Loader/QuestHandIns.h"
#include <cassert>
using namespace DragonWilds::Quests;
template<class F>bool Rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
int main(){
    AcquisitionBaseline acquired;
    assert(acquired.Observe("stone",20,true)==0); // First observation/reload is not credit.
    assert(acquired.Observe("stone",25,true)==5);
    assert(acquired.Observe("stone",25,true)==0); // Duplicate callback.
    assert(acquired.Observe("stone",10,true)==0); // Removal.
    assert(acquired.Observe("stone",15,true)==5); // Picking up again is allowed.
    assert(acquired.Observe("stone",30,false)==0); // Explicit initialization.
    assert(acquired.Observe("stone",31,true)==1);
    assert(Rejects([&]{acquired.Observe("stone",-1,true);}));
    Json item={{"Item","/Game/Test.Item"},{"Count",1}};
    Json objective=item;objective["Id"]="collect";objective["Text"]="Collect";
    Json document={{"Id","tiers"},{"PersistenceID","VT39acMY4k62LnArwSMkEQ"},{"Title","Test"},{"Description","Test"},{"Objective",objective},{"Reward",item},
        {"EntryOptions",Json::array({{{"Id","standard"},{"Cost",item}},{{"Id","premium"},{"Cost",item}}})},
        {"ResultTiers",Json::array({{{"Id","fast"},{"Priority",10},{"MaxElapsedSeconds",60},{"Reward",item}},
            {{"Id","premium"},{"Priority",20},{"EntryID","premium"},{"Reward",item}},
            {{"Id","premium_fast"},{"Priority",30},{"EntryID","premium"},{"MaxElapsedSeconds",60},{"Reward",item}}})}};
    const auto quest=Parse("Test",document);
    assert(!quest.AutomaticReward);
    auto automatic=document;automatic["Completion"]="Automatic";assert(Parse("Test",automatic).AutomaticReward);
    automatic["Completion"]="ReturnToNPC";assert(!Parse("Test",automatic).AutomaticReward);
    for(const auto& invalid:{Json("Auto"),Json(true),Json(2),Json(nullptr)}) {
        automatic["Completion"]=invalid;assert(Rejects([&]{Parse("Test",automatic);}));
    }
    auto acquisition=document;acquisition["Objective"]["Type"]="Acquire";
    assert(Parse("Test",acquisition).Acquire);
    auto repeated=document;repeated["Repeatable"]=true;repeated["RepeatReward"]={{"Item","/Game/Test.Repeat"},{"Count",2}};
    const auto repeatQuest=Parse("Test",repeated);
    assert(RewardForRun(repeatQuest,1,0).Item=="/Game/Test.Item");
    assert(RewardForRun(repeatQuest,2,0).Item=="/Game/Test.Repeat");
    assert(RewardForRun(repeatQuest,2,1).Item=="/Game/Test.Item");
    assert(Rejects([&]{RewardForRun(repeatQuest,0,0);}));
    repeated["Repeat"]={{"RewardMultiplierPerRun",0.5},{"MaximumRewardMultiplier",2.0}};
    const auto scaledRepeat=Parse("Test",repeated);
    assert(RewardForRun(scaledRepeat,2,0).Count==3);
    assert(RewardForRun(scaledRepeat,3,0).Count==4);
    assert(RewardForRun(scaledRepeat,9,0).Count==4);
    repeated["Repeat"].erase("MaximumRewardMultiplier");assert(Rejects([&]{Parse("Test",repeated);}));
    repeated.erase("Repeatable");assert(Rejects([&]{Parse("Test",repeated);}));
    assert(SelectRewardTier(quest,"standard",61)==0);
    assert(SelectRewardTier(quest,"standard",60)==1);
    assert(SelectRewardTier(quest,"premium",61)==2);
    assert(SelectRewardTier(quest,"premium",60)==3);
    assert(SelectRewardTier(quest,"standard",std::nullopt)==0);
    assert(SelectRewardTier(quest,"premium",std::nullopt)==2);
    auto secretQuest=quest;secretQuest.ResultTiers[2].Objectives={"patrol:bonus"};
    assert(SelectRewardTier(secretQuest,"premium",30)==2);
    assert(SelectRewardTier(secretQuest,"premium",30,[](const auto&){return false;})==2);
    assert(SelectRewardTier(secretQuest,"premium",30,[](const auto& id){return id=="patrol:bonus";})==3);
    auto invalid=document;invalid["StartCost"]=item;assert(Rejects([&]{Parse("Test",invalid);}));
    invalid=document;invalid["ResultTiers"][0]["Priority"]=20;assert(Rejects([&]{Parse("Test",invalid);}));
    invalid=document;invalid["ResultTiers"][0]["EntryID"]="unknown";assert(Rejects([&]{Parse("Test",invalid);}));
    invalid=document;invalid["ResultTiers"][0]["MaxElapsedSeconds"]=0;assert(Rejects([&]{Parse("Test",invalid);}));
    invalid=document;invalid["Objective"]["Optional"]=true;assert(Rejects([&]{Parse("Test",invalid);}));
    invalid=document;invalid["ResultTiers"][0]["RequiresObjectives"]=Json::array({"stage:bonus"});
    assert(Rejects([&]{Parse("Test",invalid);}));
    auto staged=document;staged.erase("Objective");
    auto bonus=objective;bonus["Id"]="bonus";bonus["Hidden"]=true;bonus["Optional"]=true;
    staged["Stages"]=Json::array({{{"Id","stage"},{"Objectives",Json::array({bonus,objective})}}});
    staged["ResultTiers"][0]["RequiresObjectives"]=Json::array({"stage:bonus"});
    const auto stagedQuest=Parse("Test",staged);
    auto areaA=quest,areaB=quest,kill=quest,optional=quest;
    areaA.ObjectiveId="a";areaB.ObjectiveId="b";kill.Kill=KillTarget{};
    optional.Optional=true;optional.ObjectiveId="bonus";
    std::vector<Definition> handIns{kill,areaA,areaB,optional};
    const auto zero=[](size_t){return 0;};
    const auto one=[](const auto&){return 1;};
    auto selected=SelectHandIns(handIns,zero,[](const auto& o){return o.ObjectiveId=="a";},one);
    assert(selected.Objectives==std::vector<size_t>{1}); // Other area/kill cannot block A.
    assert(selected.Amounts.at(areaA.Required.Item)==1);
    selected=SelectHandIns(handIns,[](size_t i){return i==1?1:0;},
        [](const auto& o){return o.ObjectiveId=="b";},one);
    assert(selected.Objectives==std::vector<size_t>{2}); // A stays completed while B commits.
    selected=SelectHandIns(handIns,zero,[](const auto&){return true;},one);
    assert(selected.Objectives==std::vector<size_t>{1}); // Never reserve the same item twice.
    handIns[0]=optional;handIns[3]=kill;
    selected=SelectHandIns(handIns,zero,[](const auto&){return true;},one);
    assert(selected.Objectives==std::vector<size_t>{1}); // Required precedes authored bonus.
    assert(SelectHandIns(handIns,zero,[](const auto&){return false;},one).Objectives.empty());
    assert(Rejects([&]{SelectHandIns(handIns,[](size_t){return -1;},[](const auto&){return true;},one);}));
    auto automaticStages=staged;automaticStages["Completion"]="Automatic";
    assert(Parse("Test",automaticStages).AutomaticReward);
    assert(!stagedQuest.Hidden && !stagedQuest.Optional);
    assert(stagedQuest.Stages[0].second[0].Hidden);
    int reads=0;
    const auto shown=ProgressText(stagedQuest,"stage",[&](const auto& entry){++reads;assert(!entry.Hidden);return 0;});
    assert(shown && *shown=="Collect (0/1)" && reads==1);
    assert(ProgressText(stagedQuest,"stage",[](const auto&){return 1;})=="Collect (1/1)");
    assert(ProgressText(quest,"collect",[](const auto&){return 0;})=="Collect (0/1)");
    auto runText=quest;runText.ProgressText="Run {run}: {remaining} left";
    assert(ObjectiveProgressText(runText,0,4)=="Run 4: 1 left");
    assert(!ProgressText(quest,"__ready",[](const auto&){return 0;}));
    assert(!ProgressText(stagedQuest,"missing",[](const auto&){return 0;}));
    assert(Rejects([&]{ProgressText(quest,"collect",[](const auto&){return 2;});}));
    assert(Rejects([&]{ProgressText(quest,"collect",[](const auto&){return -1;});}));
    invalid=staged;invalid["ResultTiers"][0]["RequiresObjectives"]=Json::array({"stage:missing"});
    assert(Rejects([&]{Parse("Test",invalid);}));
    invalid=staged;invalid["Stages"][0]["Objectives"]=Json::array({bonus});
    assert(Rejects([&]{Parse("Test",invalid);}));
    std::set<std::string> identities;
    for(int i=0;i<256;++i){const auto id=DragonWilds::DialogueSave::PersistenceId("Mod"+std::to_string(i));assert(DragonWilds::IsCanonicalPersistenceId(id));assert(identities.insert(id).second);}
    assert(DragonWilds::DialogueSave::PersistenceId("Example")==DragonWilds::DialogueSave::PersistenceId("Example"));
}
