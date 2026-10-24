#include "Core/BufferedTrace.h"
#include "Core/StartupTimeline.h"
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
    PS::StartupTimeline::State timeline;timeline.Record("InitCore begin",10);assert(timeline.Rows().empty());timeline.Begin();
    timeline.Record("load loader begin: HugeMod/assets",20);assert(timeline.Rows().empty());
    for(int i=0;i<100;++i)timeline.Record("InitCore begin",i);
    assert(timeline.Rows().size()==64&&timeline.Dropped()==36&&!timeline.Complete());
    timeline.Record("GameInstanceInit loaders complete (deferred world work may remain)",90);
    assert(timeline.Complete()&&timeline.Rows().back().ElapsedMs==99);
    timeline.Record("PostEngineInit complete",200);assert(timeline.Rows().back().ElapsedMs==99);
    timeline.Begin();assert(timeline.Rows().empty()&&!timeline.Complete()&&timeline.Dropped()==0);
    std::cout<<"2000 trace rows: 32 checkpointed flushes instead of 2000. All rows retained.\n";
}
