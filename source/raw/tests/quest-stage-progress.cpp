#include "Loader/QuestStageProgress.h"
#include "Loader/QuestAcquisition.h"
#include <cassert>
#include <map>
#include <limits>
using namespace DragonWilds::Quests::Stages;
int main() {
    const Area west{{0,0,0},100},east{{50000,0,0},100};
    Graph graph{{{"patrol",{{"west",Kind::Kill,{"goblin"},2,west},{"east",Kind::Kill,{"wolf"},1,east}}},
        {"supply",{{"gather",Kind::Gather,{"cabbage"},3,west},{"build",Kind::Build,{"bench"},1,west}}},
        {"return",{{"reach",Kind::Reach,{},1,east}}}}};
    std::map<std::string,int> nativeInts;
    auto read=[&](const std::string& key){return nativeInts[key];};
    auto write=[&](const std::string& key,int value){nativeInts[key]=value;};
    Progress progress(graph,1,read,write);progress.BeginRun();
    assert(progress.ActiveMarkers().size()==2);
    assert(progress.Apply({Kind::Kill,"goblin",2,std::array<double,3>{10001,0,0}})==0);
    assert(progress.Apply({Kind::Kill,"goblin",2,std::nullopt})==0);
    assert(progress.Apply({Kind::Kill,"wolf",1,std::array<double,3>{0,0,0}})==0);
    assert(progress.Apply({Kind::Kill,"goblin",1,std::array<double,3>{10000,0,99999}})==1);
    assert(progress.Count(0,0)==1);
    // Reconstruct from the same native save integers; no process state needed.
    Progress reloaded(graph,1,read,write);reloaded.BeginRun();
    assert(reloaded.Count(0,0)==1);
    reloaded.Apply({Kind::Kill,"goblin",999,std::array<double,3>{0,0,0}});
    assert(reloaded.ActiveMarkers()==std::vector<std::string>{"patrol:east"});
    reloaded.Apply({Kind::Gather,"cabbage",3,std::array<double,3>{0,0,0}});
    assert(reloaded.Count(1,0)==0);
    reloaded.Apply({Kind::Kill,"wolf",1,std::array<double,3>{50000,0,0}});
    assert(reloaded.ActiveStage()==1);
    reloaded.Apply({Kind::Build,"bench",1,std::array<double,3>{0,0,0}});
    reloaded.Apply({Kind::Gather,"cabbage",3,std::array<double,3>{0,0,0}});
    assert(reloaded.ActiveStage()==2);
    reloaded.Apply({Kind::Reach,"",1,std::array<double,3>{50000,0,0}});
    assert(reloaded.Complete() && reloaded.ActiveMarkers().empty());
    // Mixed objectives retain only their own unfinished circles across reloads.
    Graph regional{{{"regions",{{"west_kills",Kind::Kill,{"goblin"},2,west},
        {"east_harvest",Kind::Gather,{"stone"},3,east}}}}};
    std::map<std::string,int> regionalInts;
    auto regionalRead=[&](const std::string& key){return regionalInts[key];};
    auto regionalWrite=[&](const std::string& key,int value){regionalInts[key]=value;};
    Progress regions(regional,1,regionalRead,regionalWrite);regions.BeginRun();
    assert(regions.ActiveMarkers().size()==2);
    assert(regions.Apply({Kind::Gather,"stone",3,std::array<double,3>{0,0,0}})==0);
    assert(regions.Apply({Kind::Kill,"goblin",2,std::array<double,3>{50000,0,0}})==0);
    regions.Apply({Kind::Gather,"stone",3,std::array<double,3>{50000,0,0}});
    assert(regions.ActiveMarkers()==std::vector<std::string>{"regions:west_kills"});
    Progress regionsReloaded(regional,1,regionalRead,regionalWrite);regionsReloaded.BeginRun();
    assert(regionsReloaded.Count(0,1)==3 && !regionsReloaded.Complete());
    assert(regionsReloaded.ActiveMarkers()==std::vector<std::string>{"regions:west_kills"});
    regionsReloaded.Apply({Kind::Kill,"goblin",2,std::array<double,3>{0,0,0}});
    assert(regionsReloaded.Complete() && regionsReloaded.ActiveMarkers().empty());
    Graph acquisition{{{"gather",{{"west",Kind::Acquire,{"stone"},2,west},
        {"east",Kind::Acquire,{"stone"},2,east}}},
        {"next",{{"more",Kind::Acquire,{"stone"},1,east}}}}};
    std::map<std::string,int> acquisitionInts;
    auto acquisitionRead=[&](const auto& key){return acquisitionInts[key];};
    auto acquisitionWrite=[&](const auto& key,int value){acquisitionInts[key]=value;};
    Progress acquired(acquisition,1,acquisitionRead,acquisitionWrite);acquired.BeginRun();
    DragonWilds::Quests::AcquisitionBaseline inventory;
    auto observe=[&](int count,const std::array<double,3>& position){
        const int delta=inventory.Observe("stone",count,true);
        return delta?acquired.Apply({Kind::Acquire,"stone",delta,position}):0;
    };
    assert(observe(10,{0,0,0})==0);
    assert(observe(12,{25000,0,0})==0); // Outside both circles.
    assert(observe(12,{0,0,0})==0); // Moving inside cannot credit existing items.
    assert(observe(14,{0,0,0})==1);
    assert(acquired.ActiveMarkers()==std::vector<std::string>{"gather:east"});
    assert(observe(14,{50000,0,0})==0); // Duplicate callback in another circle.
    assert(observe(12,{50000,0,0})==0); // Dropping does not subtract progress.
    assert(observe(15,{50000,0,0})==1); // Re-pickup allowed, capped at target.
    assert(acquired.ActiveStage()==1 && acquired.Count(1,0)==0);
    Progress acquisitionReloaded(acquisition,1,acquisitionRead,acquisitionWrite);
    acquisitionReloaded.BeginRun();
    assert(acquisitionReloaded.Count(0,0)==2 && acquisitionReloaded.Count(0,1)==2);
    DragonWilds::Quests::AcquisitionBaseline reloadInventory,otherPlayer;
    assert(reloadInventory.Observe("stone",15,true)==0);
    assert(otherPlayer.Observe("stone",90,true)==0);
    assert(otherPlayer.Observe("stone",91,true)==1);
    assert(reloadInventory.Observe("stone",15,true)==0);
    const auto finalDelta=reloadInventory.Observe("stone",16,true);
    assert(acquisitionReloaded.Apply({Kind::Acquire,"stone",finalDelta,std::array<double,3>{50000,0,0}})==1);
    assert(acquisitionReloaded.Complete() && acquisitionReloaded.ActiveMarkers().empty());
    Progress acquisitionRepeat(acquisition,2,acquisitionRead,acquisitionWrite);
    acquisitionRepeat.BeginRun();
    assert(acquisitionRepeat.ActiveStage()==0 && acquisitionRepeat.Count(0,0)==0);
    assert(acquisitionRepeat.ActiveMarkers().size()==2);
    Progress repeat(graph,2,read,write);repeat.BeginRun();assert(repeat.ActiveStage()==0 && repeat.Count(0,0)==0);
    auto throws=[](auto action){try{action();return false;}catch(const std::exception&){return true;}};
    Graph secret{{{"stage",{{"required",Kind::Kill,{"goblin"},2,west},
        {"bonus",Kind::Kill,{"wolf"},1,west,true,true}}}}};
    std::map<std::string,int> secrets;
    Progress hidden(secret,1,[&](const auto& key){return secrets[key];},[&](const auto& key,int value){secrets[key]=value;});
    hidden.BeginRun();assert(hidden.ActiveMarkers().size()==1);
    hidden.Apply({Kind::Kill,"wolf",1,std::array<double,3>{0,0,0}});
    assert(hidden.Count(0,1)==1 && !hidden.Complete());
    hidden.Apply({Kind::Kill,"goblin",2,std::array<double,3>{0,0,0}});
    assert(hidden.Complete());
    Progress secretRepeat(secret,2,[&](const auto& key){return secrets[key];},[&](const auto& key,int value){secrets[key]=value;});
    secretRepeat.BeginRun();secretRepeat.Apply({Kind::Kill,"goblin",2,std::array<double,3>{0,0,0}});
    assert(secretRepeat.Complete() && secretRepeat.Count(0,1)==0);
    assert(throws([&]{progress.ActiveStage();}));
    nativeInts["RuneSchema.Objective:patrol:west"]=-1;
    assert(throws([&]{repeat.ActiveStage();}));
    auto bad=graph;bad.Stages[0].Objectives.push_back(bad.Stages[0].Objectives[0]);assert(throws([&]{Validate(bad);}));
    bad=graph;bad.Stages[0].Id="bad:id";assert(throws([&]{Validate(bad);}));
    bad=graph;bad.Stages[0].Objectives[0].Region->RadiusMeters=std::numeric_limits<double>::infinity();assert(throws([&]{Validate(bad);}));
    // Failed read-back never reports success.
    nativeInts.clear();Progress broken(graph,1,read,[](const std::string&,int){});
    assert(throws([&]{broken.BeginRun();}));
}
