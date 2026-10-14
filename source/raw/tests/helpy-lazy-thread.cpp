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
    ClearRemote();
}
