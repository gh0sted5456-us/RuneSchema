#include "Core/AlphabeticalItemCatalog.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <iostream>
using namespace PS::AlphabeticalItemCatalog;
template<class F> void Reject(F fn){bool rejected=false;try{fn();}catch(...){rejected=true;}assert(rejected);}
int main(int argc,char** argv){
    assert(PS::ItemNameOrder::Key("  aPPLe \t","path")==PS::ItemNameOrder::Key("Apple","path"));
    Cache cache;cache.Scope("world/source/clone-1");unsigned scans=0;
    const auto source=[&](size_t offset){
        ++scans;assert(offset==0||offset==4096);
        if(!offset)return Json{{"Rows",Json::array({{{"Path","/Game/Z"},{"Name","Zinc"}},{{"Path","/Mod/B"},{"Name","apple"}}})},{"NextOffset",4096},{"HasMore",true}};
        return Json{{"Rows",Json::array({{{"Path","/Game/A"},{"Name","Apple"}},{{"Path","/Game/B"},{"Name","Bronze"}},{{"Path","/Game/Z"},{"Name","Zinc"}}})},{"NextOffset",8192},{"HasMore",false}};
    };
    auto first=cache.Read("all","",2,source);
    assert(scans==1&&first["Rows"].empty()&&first["HasMore"]==true&&first["AlphabeticalReady"]==false);
    const auto scanCursor=first["NextCursor"].get<std::string>();
    first=cache.Read("all",scanCursor,2,source);
    assert(scans==2&&first["AlphabeticalReady"]==true&&first["IndexedItems"]==4);
    assert(first["Rows"][0]["Path"]=="/Game/A"&&first["Rows"][1]["Path"]=="/Mod/B"&&first["HasMore"]==true);
    const auto cursor=first["NextCursor"].get<std::string>();
    auto second=cache.Read("all",cursor,2,source);
    assert(scans==2&&second["Rows"][0]["Name"]=="Bronze"&&second["Rows"][1]["Name"]=="Zinc"&&!second.value("HasMore",true));
    auto replay=cache.Read("all",cursor,2,source);assert(replay==second&&scans==2);
    assert(cache.Read("all","",2,source)["Rows"]==first["Rows"]&&scans==2);
    for(int i=0;i<8;++i){auto filtered=cache.Read("search"+std::to_string(i),"",2,source,[](const Json& row){return row.at("Name")=="Bronze";});
        assert(filtered["Rows"].size()==1&&filtered["Rows"][0]["Name"]=="Bronze"&&!filtered.value("HasMore",true));}
    assert(scans==2);std::cout<<"Shared index: 2 native slices for 9 queries; former per-query index required 18.\n";
    const auto stale=cursor;cache.Scope("world/source/clone-after-change");
    auto recovered=cache.ReadRecovering("all",stale,2,source);
    assert(recovered["Reset"]==true&&recovered["CursorRecovered"]==true&&recovered["HasMore"]==true);
    recovered=cache.ReadRecovering("all",recovered["NextCursor"].get<std::string>(),2,source);
    assert(recovered["Rows"].size()==2&&recovered["Rows"][0]["Name"]=="Apple");
    // Recreate the all-query ticket if LRU eviction removed it.
    first=cache.Read("all","",2,source);
    Reject([&]{cache.Read("all","A:999:0:0",2,source);});
    Reject([&]{cache.Read("all","A:1:-1:0",2,source);});
    Reject([&]{cache.Read("all","A:1:999:0",2,source);});
    Reject([&]{cache.Read("all","A:1:0:0:bad",2,source);});
    Reject([&]{cache.Read("all","",129,source);});
    const auto noRows=[](size_t){return Json{{"Rows",Json::array()},{"NextOffset",10},{"HasMore",false}};};
    for(size_t i=0;i<Cache::MaxQueries;++i)cache.Read("filter"+std::to_string(i),"",10,noRows);
    assert(cache.Queries()==Cache::MaxQueries);
    Reject([&]{cache.Read("all",cursor,2,source);});
    cache.Scope("single-player/source/clone-2");assert(cache.Queries()==0);
    Reject([&]{cache.Read("all",cursor,2,source);});
    auto empty=cache.Read("empty","",10,noRows);assert(empty["Rows"].empty()&&!empty.value("HasMore",true));
    Cache stalled;Reject([&]{stalled.Read("stalled","",10,[](size_t){return Json{{"Rows",Json::array()},{"NextOffset",0},{"HasMore",true}};});});
    // Empty filters and sparse matches advance without another native scan.
    Cache large;size_t sourceCalls=0;const auto largeSource=[&](size_t){++sourceCalls;Json rows=Json::array();for(size_t i=0;i<5000;++i)rows.push_back({{"Path","/Game/"+std::to_string(i)},{"Name",std::to_string(i)}});return Json{{"Rows",rows},{"NextOffset",5000},{"HasMore",false}};};
    std::string largeCursor;Json page;for(int i=0;i<12;++i){page=large.Read("none",largeCursor,2,largeSource,[](const Json&){return false;});largeCursor=page["NextCursor"];if(!page.value("HasMore",true))break;}
    assert(sourceCalls==1&&page["Rows"].empty()&&!page.value("HasMore",true));
    Cache sparse;size_t calls=0;std::string next;
    for(size_t i=0;i<12;++i){auto result=sparse.Read("sparse",next,1,[&](size_t offset){++calls;return Json{{"Rows",Json::array()},{"NextOffset",offset+2048},{"HasMore",true}};});
        assert(result["Rows"].empty()&&result["HasMore"]==true);assert(result["NextCursor"]!=next);next=result["NextCursor"];
    }assert(calls==12);
    if(argc>=3){
        const auto read=[](const char* path){std::ifstream input(path,std::ios::binary);assert(input.good());return std::string(std::istreambuf_iterator<char>(input),{});};
        assert(read(argv[1])==read(argv[2]));
        if(argc==4){const auto native=read(argv[3]);assert(native.find("PS::AssetSearch::AlphabeticalItemWindow(query,cursor,limit,fieldSet)")!=std::string::npos);}
    }
}
