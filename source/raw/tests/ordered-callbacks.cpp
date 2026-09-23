#include "Utility/OrderedCallbacks.h"
#include <functional>
#include <thread>
#include <atomic>
#include <cassert>
#include <iostream>
int main() {
    PS::OrderedCallbacks<std::function<void()>> callbacks;
    std::vector<int> order;
    auto a=callbacks.Add([&]{ order.push_back(1); });
    auto b=callbacks.Add([&]{ order.push_back(2); });
    callbacks.Add([&]{ order.push_back(3); callbacks.Remove(b); });
    for (auto& fn:callbacks.Snapshot()) fn();
    assert((order==std::vector<int>{1,2,3}));
    order.clear();
    for (auto& fn:callbacks.Snapshot()) fn();
    assert((order==std::vector<int>{1,3}));
    callbacks.Remove(a);
    PS::OrderedCallbacks<std::function<void()>> parallel;
    std::atomic<int> called{0};
    std::vector<std::thread> threads;
    for (int i=0;i<4;++i) threads.emplace_back([&]{for(int j=0;j<1000;++j) parallel.Add([&]{++called;});});
    for(auto& t:threads)t.join();
    for(auto& fn:parallel.Snapshot()) fn();
    assert(called==4000);
    PS::OrderedCallbacks<std::function<void()>> nested;
    bool recursed=false;
    nested.Add([&]{ if(!recursed){recursed=true; for(auto& fn:nested.Snapshot())fn();} });
    for(auto& fn:nested.Snapshot())fn();
    assert(recursed);
    std::cout<<"PASS: registration order, removal during dispatch, nested snapshots and 4000 concurrent registrations.\n";
}
