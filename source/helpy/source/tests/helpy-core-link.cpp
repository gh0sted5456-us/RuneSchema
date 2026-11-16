#include "Runtime/HelpyAchievements.h"
#include <cassert>
#include <iostream>
#include <thread>
// Helpy's core link runs requests on its own thread, coalesces by key and never blocks the caller.
using namespace PS;using Json=nlohmann::json;using namespace std::chrono_literals;
int main(){
    HelpyCoreLink::Link link;
    std::atomic<int> calls=0;std::atomic<bool> release=false;
    const auto slow=[&](const Json& request,Json& reply){++calls;while(!release)std::this_thread::sleep_for(1ms);reply={{"Echo",request}};return true;};
    assert(!link.Post("x",{{"A",1}},slow));                                    // Not started.
    link.Start();
    // Posting never waits for a slow core: the UI thread returns at once.
    const auto started=std::chrono::steady_clock::now();
    std::vector<int> seen;
    assert(link.Post("slow",{{"N",0}},slow,[&](bool ok,const Json& reply){assert(ok);seen.push_back(reply["Echo"]["N"]);}));
    while(calls==0)std::this_thread::sleep_for(1ms);                            // Running now.
    for(int i=1;i<=5;++i)assert(link.Post("visibility",{{"N",i}},slow,[&](bool,const Json& reply){seen.push_back(reply["Echo"]["N"]);}));
    assert(std::chrono::steady_clock::now()-started<200ms);
    assert(link.Busy("slow")&&link.Busy("visibility")&&!link.Busy("other"));
    assert(link.Snapshot().Coalesced==4&&link.Snapshot().Queued==1);           // Latest visibility wins.
    assert(link.Drain()==0);                                                     // Nothing finished yet.
    release=true;
    while(link.Busy("visibility")||link.Busy("slow"))std::this_thread::sleep_for(1ms);
    std::this_thread::sleep_for(5ms);
    assert(link.Drain()==2&&(seen==std::vector<int>{0,5}));                    // Callbacks run on the draining thread, in order.
    // Failures and exceptions are reported, not thrown.
    bool failed=false;link.Post("",{},[](const Json&,Json&)->bool{throw std::runtime_error("pipe");},[&](bool ok,const Json&){failed=!ok;});
    while(link.Snapshot().Executed<3)std::this_thread::sleep_for(1ms);
    link.Drain();assert(failed&&link.Snapshot().Failed==1);
    // The queue is bounded.
    release=false;{const int before=calls;link.Post("hold",{},slow);while(calls==before)std::this_thread::sleep_for(1ms);} // Running, not just queued.
    int accepted=0;for(int i=0;i<100;++i)accepted+=link.Post("",{},slow)?1:0;assert(accepted==64);
    const auto beforeStop=calls.load();
    std::thread stopping([&]{link.Stop();});
    while(link.Busy("visibility"))std::this_thread::yield();
    while(link.Snapshot().Queued!=0)std::this_thread::yield();
    assert(!link.Post("late",{},slow));
    release=true;stopping.join();
    assert(calls==beforeStop&&link.Drain()==0&&!link.Post("late",{},slow));
    // The final input release follows the last possible transport call.
    link.Start();release=false;
    std::atomic<bool> capture=false;
    assert(link.Post("input",{},[&](const Json&,Json&){while(!release)std::this_thread::yield();capture=true;return true;}));
    while(link.Snapshot().Queued)std::this_thread::yield();
    release=true;link.Stop();capture=false;
    assert(!capture&&!link.Busy("input"));
    namespace A=HelpyAchievements;
    namespace Runtime=PluginRuntimeServices;
    A::ProgressLoaded=true;
    Runtime::GameProcessId=42;
    Runtime::ConnectionEpoch=1;
    link.Start();
    std::atomic_bool entered=false;
    release=false;
    const auto oldFeed=[&](const Json&,Json& reply){
        entered=true;
        while(!release)std::this_thread::yield();
        reply={{"Session","old"},{"Events",Json::array()}};return true;
    };
    A::Poll(link,oldFeed,true);
    while(!entered)std::this_thread::yield();
    ++Runtime::ConnectionEpoch; // A reconnect to the same PID still invalidates the old reply.
    release=true;
    while(link.Busy("achievements"))std::this_thread::yield();
    const auto posted=link.Snapshot().Posted;
    A::Poll(link,oldFeed,true);
    assert(link.Snapshot().Posted==posted);
    assert(link.Drain()==1&&A::Progress.Session.empty()&&!A::Dirty);
    A::Poll(link,[](const Json&,Json& reply){reply={{"Session","new"},{"Events",Json::array()}};return true;},true);
    while(link.Busy("achievements"))std::this_thread::yield();
    assert(link.Drain()==1&&A::Progress.Session=="new");
    A::NextSave=A::Clock::now()+1h;
    A::Definitions=ModAchievements::Parse(Json::parse(R"({"format":1,"achievements":[{"id":"once","title":"Once","conditions":[{"event":"death","count":3}]}]})"),"mods/test","Test");
    A::Apply(true,{{"Session","new"},{"Events",Json::array({{{"Kind","death"},{"Sequence",uint64_t(1)}},{{"Kind","death"},{"Sequence",uint64_t(1)}}})}},nullptr,false);
    assert(A::Progress.Cursor==1&&A::Progress.Entries.at("mods/test|once").Counters.at(0)==1);
    A::Dirty=false;link.Stop();
    std::cout<<"Helpy core link coalesces requests, stops before input release, and rejects stale feed replies\n";
}
