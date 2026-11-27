#include "Generator/ToolRequest.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
// Catalog snapshots are built from shared pieces outside the tool mutex, so a Helpy read never holds the
// lock the game thread needs while large catalogs are copied.
using PS::SpawnToolRequests;using nlohmann::json;using Clock=std::chrono::steady_clock;
static json Catalog(int items){json rows=json::array();for(int i=0;i<items;++i)rows.push_back({{"Path","/Game/Items/DA_Item_"+std::to_string(i)+".DA_Item_"+std::to_string(i)},{"Name","Item "+std::to_string(i)},
    {"Category","Weapons"},{"Icon","/Game/UI/Icons/T_"+std::to_string(i)},{"Stack",20},{"Tags",json::array({"a","b","c"})},{"Value",i*3}});return rows;}
int main(){
    SpawnToolRequests::Available=true;SpawnToolRequests::Clear();
    SpawnToolRequests::CatalogProgress({{"Items",Catalog(3)},{"Definitions",json::array({{{"Type","AI"},{"Key","a"}},{{"Type","Resource"},{"Key","r"}},{{"Type","NPC"},{"Key","n"}}})},
        {"ServerMods",json::array({"m"})},{"CatalogReady",true},{"IndexDone",5}});
    // Skill-save acknowledgements survive the compact one-shot receipt path.
    assert(SpawnToolRequests::TrySubmit({{"Action","SetSkillLevel"},{"_RequestId",uint64_t(7001)}}));
    assert(SpawnToolRequests::Take());
    const json skill={{"CharacterGuid","1234567890ABCDEF1234567890ABCDEF"},{"SkillId","native-skill"},{"Applied",true},{"SaveRequested",true}};
    SpawnToolRequests::Publish({{"SkillEdit",skill},{"Status","Native save requested"}});
    json skillReceipt;assert(SpawnToolRequests::TakeCompleted(7001,skillReceipt)&&skillReceipt.at("SkillEdit")==skill);
    assert(!SpawnToolRequests::TakeCompleted(7001,skillReceipt));
    // Category views match the previous behaviour.
    uint64_t revision=0;json out;
    assert(SpawnToolRequests::ReadQuickIfChanged(revision,0,out)&&out["Items"].size()==3&&!out.contains("Definitions")&&out["_Category"]==0&&out["CatalogReady"]==true&&out["ServerMods"].size()==1);
    assert(!SpawnToolRequests::ReadQuickIfChanged(revision,0,out));                   // Unchanged revision.
    revision=0;assert(SpawnToolRequests::ReadQuickIfChanged(revision,1,out)&&out["Definitions"].size()==2&&!out.contains("Items"));
    revision=0;assert(SpawnToolRequests::ReadQuickIfChanged(revision,2,out)&&out["Definitions"].size()==1&&out["Definitions"][0]["Key"]=="r");
    revision=0;assert(SpawnToolRequests::ReadQuickIfChanged(revision,-2,out)&&out.size()==1&&out.contains("ServerMods"));
    revision=0;assert(SpawnToolRequests::ReadQuickIfChanged(revision,-1,out)&&out.contains("Items")&&out.contains("Definitions")&&out.contains("Status")&&out["IndexDone"]==5);
    // Shared pieces are immutable: a later progress replaces, never mutates, what a reader holds.
    const auto held=SpawnToolRequests::CatalogShared();
    SpawnToolRequests::CatalogProgress({{"Items",Catalog(1)}});
    assert(held.at("Items")->size()==3&&SpawnToolRequests::CatalogShared().at("Items")->size()==1);
    SpawnToolRequests::Clear();assert(SpawnToolRequests::CatalogShared().empty());
    SpawnToolRequests::Publish({{"Status","Ready."},{"Items",Catalog(6000)},
        {"ServerMods",json::array({"fallback"})}});
    SpawnToolRequests::CatalogMap saved;
    {std::lock_guard lock(SpawnToolRequests::Mutex);saved=SpawnToolRequests::Result;}
    SpawnToolRequests::Progress("Working.",{{"IndexDone",1}});
    revision=0;
    assert(SpawnToolRequests::ReadIfChanged(revision,out)&&out["Items"].size()==6000&&out["Status"]=="Working.");
    assert(*saved.at("Status")=="Ready."&&saved.at("Items")->size()==6000);
    SpawnToolRequests::Clear();
    auto projected=SpawnToolRequests::CatalogSnapshot(json::object(),saved,-2);
    assert(projected.size()==1&&projected["ServerMods"][0]=="fallback");
    // Contention: how long does the game thread's CatalogProgress wait while a reader is busy?
    const json big=Catalog(6000);SpawnToolRequests::CatalogProgress({{"Items",big}});
    const auto measure=[&](auto read){
        std::atomic<bool> stop=false;std::thread reader([&]{while(!stop){read();std::this_thread::sleep_for(std::chrono::milliseconds(1));}});
        double worst=0;for(int i=0;i<40;++i){const json small={{"IndexDone",i}};const auto t=Clock::now();SpawnToolRequests::CatalogProgress(small);
            worst=std::max(worst,std::chrono::duration<double,std::milli>(Clock::now()-t).count());std::this_thread::sleep_for(std::chrono::milliseconds(2));}
        stop=true;reader.join();return worst;};
    // Previous Read: deep copy of the catalog while holding the mutex.
    const double before=measure([]{std::lock_guard lock(SpawnToolRequests::Mutex);json copy=SpawnToolRequests::Expand(SpawnToolRequests::Result);for(const auto& [key,value]:SpawnToolRequests::QuickCatalogData)copy[key]=*value;});
    // New Read: pointers under the mutex, snapshot after release.
    const double after=measure([]{uint64_t latest=0;json view;(void)SpawnToolRequests::ReadQuickIfChanged(latest,-1,view);});
    std::cout<<"game-thread CatalogProgress worst wait with a busy reader: before "<<before<<" ms, after "<<after<<" ms\n";
    (void)before;(void)after; // Reported, not asserted: timing on shared CI runners is noisy.
    std::cout<<"Tool catalog snapshots are shared and built outside the mutex\n";
}
