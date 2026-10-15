#include "Generator/LazyCatalog.h"
#include <cassert>
#include <thread>
int main(){
    using namespace PS::LazyCatalog;
    PS::SpawnToolRequests::Generation=10;
    unsigned submissions=0;
    ConfigureRemote([&](json){++submissions;return true;},[](int,uint64_t){});
    FirstOpenPendingGeneration=10;For(0)=State{};
    std::thread producer([]{for(unsigned i=0;i<20000;++i)BackendSignal(10);});
    // Producer does not read or mutate any UI-owned JSON or state.
    producer.join();assert(submissions==0);assert(!For(0).Pending);
    PumpBackendSignal();assert(submissions==1);assert(For(0).Pending);
    BackendSignal(10);PumpBackendSignal();assert(submissions==1);
    ResetForGeneration(11);assert(For(0).RuntimeGeneration==11 && For(0).Rows.empty());
    PS::SpawnToolRequests::Generation=11;For(0)=State{};submissions=0;
    assert(Request(0,"Search","test",true,20));
    const auto respond=[&](int start,int count,bool reset,bool more){
        auto& state=For(0);json rows=json::array();for(int i=start;i<start+count;++i)rows.push_back({{"Path","/Game/Item"+std::to_string(i)}});
        ApplyWindow({{"Category",0},{"Mode","Search"},{"CancelGeneration",state.CancelGeneration},{"RuntimeGeneration",11},
            {"RequestId",state.ActiveRequestId},{"Reset",reset},{"Rows",rows},{"HasMore",more}},submissions);
    };
    respond(0,20,true,true);assert(!For(0).Pending&&submissions==1);
    FillTo(0,60);assert(For(0).Pending&&For(0).FillTarget==60&&submissions==2);
    respond(20,20,false,true);assert(For(0).Pending&&For(0).FillTarget==60&&submissions==3);
    respond(40,20,false,true);assert(!For(0).Pending&&For(0).Rows.size()==60&&submissions==3);
    FillTo(0,60);assert(submissions==3); // No moving target or background full-catalog drain.
    FillTo(0,80);assert(submissions==4);
    respond(0,0,false,true);respond(0,0,false,true);respond(0,0,false,true);
    assert(!For(0).Pending&&!For(0).LastError.empty()&&For(0).StalledWindows==3);
    const auto stopped=submissions;FillTo(0,80);assert(submissions==stopped);
    ClearRemote();
}
