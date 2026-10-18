#include "Core/RecipeItemRouting.h"
#include <cassert>
#include <iostream>
#include <vector>

using PS::RecipeItemRouting::Index;
using PS::RecipeItemRouting::Route;
using nlohmann::json;
template<class F>bool Rejects(F action){try{action();}catch(const std::runtime_error&){return true;}return false;}
int main(){
    std::vector<std::pair<std::string,std::string>> roster;
    std::unordered_map<std::string,std::string> live;
    for(std::size_t index=0;index<40000;++index){
        const auto digits=std::to_string(index);
        const auto id=std::string(21-digits.size(),'A')+digits+"A";
        const auto path="/Game/Items/ITEM_"+digits+".ITEM_"+digits;
        roster.emplace_back(id,path);live.emplace(path,id);
    }
    Index routes;std::size_t scans=0,resolutions=0;
    const auto prepare=[&]{routes.Ensure([&](Index::Builder& builder){++scans;for(const auto& [id,path]:roster)builder.Add(id,path);});};
    const auto resolve=[&](const std::string& path,const std::string& identity){
        ++resolutions;auto item=live.find(path);
        if(item==live.end()||(!identity.empty()&&item->second!=identity))throw std::runtime_error("missing/replaced live ItemData");
        return path;
    };
    const auto& [id,path]=roster.back();
    const auto explicitItems=json::array({{{"ItemData",path},{"Count",2}}});
    // Recipe finalization and the 66 reported vendor offers share this actual
    // routing helper. Fully qualified paths must not enumerate any ItemData.
    for(int offer=0;offer<66;++offer){
        assert(Route("ItemsConsumed",explicitItems,routes,prepare,resolve)==explicitItems);
        assert(Route("ItemsCreated",explicitItems,routes,prepare,resolve)==explicitItems);
    }
    assert(scans==0&&resolutions==132);
    const auto legacy=json::array({{{"ItemData",id},{"Count",2}}});
    for(int offer=0;offer<66;++offer)assert(Route("ItemsCreated",legacy,routes,prepare,resolve)==explicitItems);
    assert(scans==1&&routes.Resolve(roster[32768].first)==roster[32768].second&&routes.Resolve(id)==path);
    assert(Route("OtherField",legacy,routes,prepare,resolve)==legacy);
    assert(Rejects([&]{Route("ItemsCreated",json::array({{{"ItemData","not a path"}}}),routes,prepare,resolve);}));
    live[path]="changed-identity";
    assert(Rejects([&]{Route("ItemsCreated",legacy,routes,prepare,resolve);}));
    live.erase(path);
    assert(Rejects([&]{Route("ItemsCreated",explicitItems,routes,prepare,resolve);}));
    live[path]=id;
    Index ambiguous;ambiguous.Ensure([&](Index::Builder& builder){builder.Add(id,path);builder.Add(id,path);builder.Add(id,"/Game/B.B");builder.Add(id,"/Game/C.C");});
    assert(ambiguous.Ambiguous(id)&&!ambiguous.Matches(id,path));
    assert(Rejects([&]{ambiguous.Resolve(id);}));
    Index empty;std::size_t emptyScans=0;
    const auto emptyPrepare=[&]{empty.Ensure([&](auto&){++emptyScans;});};
    for(int offer=0;offer<66;++offer)assert(Rejects([&]{Route("ItemsCreated",legacy,empty,emptyPrepare,resolve);}));
    assert(emptyScans==1);
    Index failed;std::size_t failedScans=0;
    const auto failedPrepare=[&]{failed.Ensure([&](auto& builder){++failedScans;builder.Add(id,path);throw std::runtime_error("fixture scan failure");});};
    for(int offer=0;offer<66;++offer)assert(Rejects([&]{Route("ItemsCreated",legacy,failed,failedPrepare,resolve);}));
    assert(failedScans==1);
    assert(Route("ItemsCreated",explicitItems,failed,failedPrepare,resolve)==explicitItems);
    failed.Reset();failed.Ensure([&](auto& builder){++failedScans;builder.Add(id,path);});
    assert(failed.Resolve(id)==path&&failedScans==2);
    std::cout<<"recipe-item-routing: PASS (40000 items; direct paths; PID; 66 offers; ambiguous/stale/missing assets; empty/failed cache)\n";
}
