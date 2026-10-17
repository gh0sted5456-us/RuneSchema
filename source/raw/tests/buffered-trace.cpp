#include "Core/BufferedTrace.h"
#include <cassert>
#include <iostream>
using namespace PS::BufferedTrace;
int main(){
    using namespace std::chrono;
    FlushPolicy policy;const auto start=steady_clock::time_point{};size_t flushes=0;
    for(size_t i=0;i<2000;++i)if(policy.Record(start))++flushes;
    assert(flushes==31);assert(policy.Record(start,true));++flushes;assert(flushes==32);
    policy.Reset();assert(!policy.Record(start));assert(!policy.Record(start+milliseconds(99)));assert(policy.Record(start+milliseconds(100)));
    assert(!policy.Record(start+milliseconds(101)));assert(policy.Record(start+milliseconds(102),true));
    assert(Checkpoint("ERROR initialize: assets")&&Checkpoint("GameInstanceInit loaders complete (deferred world work may remain)")&&Checkpoint("shutdown complete"));
    assert(!Checkpoint("load loader begin: Mod/assets")&&!Checkpoint("load loader end: Mod/assets"));
    std::cout<<"2000 trace rows: 32 checkpointed flushes instead of 2000. All rows retained.\n";
}
