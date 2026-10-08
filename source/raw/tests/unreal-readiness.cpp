#include "Utility/UnrealReadinessGate.h"
#include <cassert>
#include <thread>
#include <iostream>
#include <atomic>
int main() {
 using Gate=PS::UnrealReadinessGate<int*,4>;int a,b,c,d,e;
 Gate g;assert(!g.Observe(&a));assert(!g.Observe(&a));assert(g.Begin()==Gate::BeginResult::Wait);
 g.MarkUnrealReady();assert(g.Begin()==Gate::BeginResult::Start);assert(g.Begin()==Gate::BeginResult::Wait);
 assert(!g.Observe(&b));auto q=g.Complete();assert(q.size()==2&&q[0]==&a&&q[1]==&b);assert(g.IsActive()&&g.Observe(&c));
 Gate overflow;overflow.Observe(&a);overflow.Observe(&b);overflow.Observe(&c);overflow.Observe(&d);overflow.Observe(&e);overflow.MarkUnrealReady();assert(overflow.Begin()==Gate::BeginResult::Overflow);assert(!overflow.IsActive());
 Gate during;during.MarkUnrealReady();assert(during.Begin()==Gate::BeginResult::Start);during.Observe(&a);during.Observe(&b);during.Observe(&c);during.Observe(&d);during.Observe(&e);assert(during.Complete().empty()&&!during.IsActive());
 Gate failed;failed.MarkUnrealReady();failed.Begin();failed.Fail();assert(!failed.Observe(&a)&&failed.Begin()==Gate::BeginResult::Wait);
 PS::UnrealReadinessGate<int*,128> concurrent;int values[64];std::vector<std::thread> threads;
 for(int i=0;i<64;++i)threads.emplace_back([&,i]{assert(!concurrent.Observe(&values[i]));assert(!concurrent.Observe(&values[i]));});
 for(auto&t:threads)t.join();threads.clear();concurrent.MarkUnrealReady();std::atomic<int> starters{0};
 for(int i=0;i<16;++i)threads.emplace_back([&]{if(concurrent.Begin()==decltype(concurrent)::BeginResult::Start)++starters;});
 for(auto&t:threads)t.join();assert(starters==1);assert(concurrent.Complete().size()==64);
 std::cout<<"Readiness gate: early events, deduplication/order, reentrant arrivals, one initializer, concurrent capture, overflow before/during initialization, terminal failure passed\n";
}
