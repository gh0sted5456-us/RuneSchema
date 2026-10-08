#include "Loader/QuestKillCredit.h"
#include <cassert>
using namespace DragonWilds::Quests;
template<class F>bool Rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
int main(){
    Json doc={{"Id","goblins"},{"PersistenceID","AAAAAAAAAAAAAAAAAAAAAA"},{"Title","Goblins"},{"Description","Defeat goblins"},
        {"Objective",{{"Id","kills"},{"Type","Kill"},{"Text","Kill 5 goblins"},{"AIClasses",{"/Game/Goblin.Goblin_C"}},{"Count",5}}},
        {"Reward",{{"Item","/Game/Reward.Reward"},{"Count",1}}}};
    auto quest=Parse("Test",doc);assert(quest.Kill && quest.Required.Count==5 && quest.Required.Item.empty());
    auto bad=doc;bad["Objective"]["Item"]="/Game/Item.Item";assert(Rejects([&]{Parse("Test",bad);}));
    bad=doc;bad["Objective"]["AIClasses"]={"Goblin"};assert(Rejects([&]{Parse("Test",bad);}));
    bad=doc;bad["Objective"]["AIClasses"]={"/Game/G.G_C","/Game/G.G_C"};assert(Rejects([&]{Parse("Test",bad);}));
    bad=doc;bad["Objective"]["IncludeDerived"]="true";assert(Rejects([&]{Parse("Test",bad);}));
    KillCredits ledger;ledger.BeginWorld(1);int saved=0;
    KillSignal signal{1,10,25,"player","/Game/Goblin.Goblin_C","",{},true,true};
    auto apply=[&]{return ledger.Apply(quest,signal,"player",true,[&]{return saved;},[&](int n){saved=n;});};
    assert(apply()==KillResult::Counted && saved==1);assert(apply()==KillResult::Duplicate && saved==1);
    signal.ObjectIndex++;signal.CreditedCharacter="other";assert(apply()==KillResult::Ignored && saved==1);
    signal.CreditedCharacter="player";signal.ConfirmedDeath=false;assert(apply()==KillResult::Ignored);
    signal.ConfirmedDeath=true;signal.Authoritative=false;assert(apply()==KillResult::Ignored);signal.Authoritative=true;
    signal.VictimClass="/Game/Wolf.Wolf_C";assert(apply()==KillResult::Ignored);
    signal.VictimBaseClasses={"/Game/Goblin.Goblin_C"};assert(apply()==KillResult::Counted && saved==2);
    ledger.BeginWorld(2);assert(apply()==KillResult::Ignored);signal.WorldEpoch=2;
    assert(apply()==KillResult::Counted && saved==3); // persisted count, new world identity
    signal.ObjectIndex++;
    assert(Rejects([&]{ledger.Apply(quest,signal,"player",true,[&]{return saved;},[&](int n){saved=n;throw std::runtime_error("after write");});}));
    assert(saved==4);assert(apply()==KillResult::Counted && saved==4);assert(apply()==KillResult::Duplicate);
    signal.ObjectIndex++;quest.Kill->EventKey="Test:encounter";assert(apply()==KillResult::Ignored);
    signal.EventKey="Test:encounter";assert(apply()==KillResult::Counted && saved==5);
    signal.ObjectIndex++;assert(apply()==KillResult::Capped && saved==5);
    assert(Rejects([&]{ledger.BeginWorld(2);}));
    auto areaQuest=quest;areaQuest.Key="Test:area";areaQuest.Marker=Location{"area",{0,0,0},100};
    int areaCount=0;
    const auto areaApply=[&]{return ledger.Apply(areaQuest,signal,"player",true,[&]{return areaCount;},[&](int value){areaCount=value;});};
    assert(areaApply()==KillResult::Ignored);
    signal.EventKey="";signal.Position=std::array<double,3>{10001,0,0};assert(areaApply()==KillResult::Ignored);
    signal.Position=std::array<double,3>{10000,0,50000};assert(areaApply()==KillResult::Counted && areaCount==1);
    assert(ledger.StageForSignal("Test:stages",signal,0)==0);
    assert(ledger.StageForSignal("Test:stages",signal,1)==0);
    signal.ObjectIndex++;assert(ledger.StageForSignal("Test:stages",signal,1)==1);
    auto customDoc=doc;customDoc["Objective"]["EventID"]="patrol";customDoc["Objective"]["SpawnID"]="bandit";
    auto custom=Parse("Test",customDoc);assert(custom.Kill->SpawnKey=="Test:bandit");
    int customCount=0;signal.EventKey="Test:patrol";signal.SpawnKey="Other:bandit";
    const auto customApply=[&]{return ledger.Apply(custom,signal,"player",true,[&]{return customCount;},[&](int value){customCount=value;});};
    assert(customApply()==KillResult::Ignored && customCount==0);
    signal.SpawnKey="Test:bandit";signal.EventKey="Other:patrol";assert(customApply()==KillResult::Ignored);
    signal.EventKey="Test:patrol";assert(customApply()==KillResult::Counted && customCount==1);
    assert(customApply()==KillResult::Duplicate && customCount==1);
    customDoc["Objective"].erase("EventID");assert(Rejects([&]{Parse("Test",customDoc);}));
}
