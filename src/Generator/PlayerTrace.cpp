#include "Generator/PlayerTrace.h"
#include "Generator/PlayerTraceOptions.h"
#include "Unreal/Hooks.hpp"
#include "Unreal/FWeakObjectPtr.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include <Windows.h>
#include <atomic>
#include <algorithm>
#include <cctype>
#include <vector>
#include <stdexcept>

using namespace RC;
using namespace RC::Unreal;
namespace PS::PlayerTrace {
namespace {
struct Event { uint64_t first,last; std::string function,context,source; unsigned repeats=1; };
std::vector<Event> events;
FWeakObjectPtr playerRoot,controllerRoot;
std::atomic<bool> active{false};
std::atomic<unsigned> otherThreads{};
Hook::GlobalCallbackId callback=Hook::ERROR_ID;
bool session{};unsigned thread{},mask=15;uint64_t start{},duration=30000;
size_t limit=1024;std::string filter;bool suppressTicks=true;
std::string playerPath,controllerPath,reason;
unsigned seen{},suppressed{};
std::string Lower(std::string text) { for(auto& ch:text)ch=static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));return text; }
unsigned Category(const std::string& name) {
    if(name.find("equip")!=name.npos || name.find("customiz")!=name.npos || name.find("material")!=name.npos)return 1;
    if(name.find("attack")!=name.npos || name.find("spell")!=name.npos || name.find("magic")!=name.npos || name.find("damage")!=name.npos)return 2;
    if(name.find("interact")!=name.npos || name.find("inventory")!=name.npos || name.find("craft")!=name.npos || name.find("item")!=name.npos)return 4;
    if(name.find("move")!=name.npos || name.find("jump")!=name.npos || name.find("evade")!=name.npos || name.find("sprint")!=name.npos)return 8;
    return 16;
}
void Append(std::string name,std::string context,std::string source) {
    const auto elapsed=GetTickCount64()-start;
    if(!events.empty() && events.back().function==name && events.back().context==context && events.back().source==source) {
        ++events.back().repeats;events.back().last=elapsed;
    } else if(events.size()<limit)events.push_back({elapsed,elapsed,std::move(name),std::move(context),std::move(source),1});
    if(++seen>=limit) { reason="event limit";active.store(false,std::memory_order_release); }
}
void Observe(Hook::TCallbackIterationData<void>&,UObject* context,UFunction* function,void*) {
    if(!active.load(std::memory_order_acquire))return;
    if(GetCurrentThreadId()!=thread) { otherThreads.fetch_add(1,std::memory_order_relaxed);return; }
    if(GetTickCount64()-start>=duration) { reason="duration limit";active.store(false);return; }
    if(!context || !function)return;
    auto* player=playerRoot.Get();auto* controller=controllerRoot.Get();
    if(!player || !controller) { reason="player/controller unavailable";active.store(false);return; }
    bool owned=false;
    unsigned depth=0;
    for(auto* outer=context;outer && depth++<8;outer=outer->GetOuterPrivate()) {
        if(outer==player || outer==controller) { owned=true;break; }
        if(outer->GetOuterPrivate()==outer)break;
    }
    if(!owned)return;
    try {
        auto name=to_string(function->GetPathName());auto lower=Lower(name);
        if((suppressTicks && (lower.find("tick")!=lower.npos || lower.find("updateanimation")!=lower.npos))
            || !(mask&Category(lower)) || (!filter.empty() && lower.find(filter)==lower.npos)) { ++suppressed;return; }
        Append(std::move(name),to_string(context->GetPathName()),"reflected ProcessEvent post");
    } catch(...) { reason="capture error";active.store(false); }
}
void Detach() { active.store(false);if(callback!=Hook::ERROR_ID) { Hook::UnregisterCallback(callback);callback=Hook::ERROR_ID; } }
}
void Start(UObject* player,UObject* controller,const nlohmann::json& options) {
    if(session)throw std::runtime_error("Stop/export the previous Player Trace first.");
    if(!player || !controller)throw std::runtime_error("Enter a world before tracing.");
    ValidateOptions(options);
    auto seconds=options.value("Seconds",30);auto maximum=options.value("MaxEvents",1024);auto categories=options.value("Categories",15);
    auto text=options.value("Filter",std::string{});
    playerRoot=player;controllerRoot=controller;thread=GetCurrentThreadId();
    playerPath=to_string(player->GetPathName());controllerPath=to_string(controller->GetPathName());
    mask=categories;filter=Lower(text);duration=seconds*1000ull;limit=maximum;suppressTicks=options.value("SuppressTicks",true);
    events.clear();events.reserve(limit);seen=0;suppressed=0;otherThreads.store(0);reason="manual stop";start=GetTickCount64();
    Hook::FCallbackOptions hookOptions{};hookOptions.OwnerModName=TEXT("RuneSchema");hookOptions.HookName=TEXT("ManualPlayerTrace");
    callback=Hook::RegisterProcessEventPostCallback(Observe,hookOptions);
    if(callback==Hook::ERROR_ID)throw std::runtime_error("UE4SS Player Trace callback unavailable.");
    session=true;active.store(true,std::memory_order_release);
}
void Tick() {
    if(callback==Hook::ERROR_ID)return;
    if(GetTickCount64()-start>=duration && active.load()) { reason="duration limit";active.store(false); }
    if(!active.load())Detach();
}
void Marker(const std::string& text) {
    if(!active.load())throw std::runtime_error("Start Player Trace before adding a marker.");
    if(text.empty() || text.size()>128)throw std::runtime_error("Marker requires 1-128 characters.");
    Append(text,"user","user marker");
}
nlohmann::json Stop() {
    if(!session)throw std::runtime_error("No Player Trace session is available.");
    Detach();
    nlohmann::json result={{"kind","PlayerTrace1"},{"player",playerPath},{"controller",controllerPath},{"threadId",thread},
        {"reason",reason},{"matchedEvents",seen},{"filteredOwnedCalls",suppressed},{"skippedOtherThreadCalls",otherThreads.load()},
        {"categories",mask},{"filter",filter},{"durationLimitMs",duration},{"eventLimit",limit},{"events",nlohmann::json::array()},
        {"coverage","Reflected post-calls on player/controller and their Outer descendants, captured on the starting game thread. Direct native calls and external world-object/held-actor calls may be absent. Categories are inferred from names; rows are completion order, not call nesting."}};
    for(size_t i=0;i<events.size();++i) {
        const auto& e=events[i];result["events"].push_back({{"sequence",i},{"firstMs",e.first},{"lastMs",e.last},
            {"function",e.function},{"context",e.context},{"source",e.source},{"consecutiveCount",e.repeats}});
    }
    events.clear();session=false;return result;
}
void Cancel() { Detach();events.clear();session=false; }
}
