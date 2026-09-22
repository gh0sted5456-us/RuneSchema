#include "Generator/PlayerTrace.h"
#include "Generator/PlayerTraceOptions.h"
#include "Generator/FocusedCapture.h"
#include "Generator/EventParameters.h"
#include "Unreal/Hooks.hpp"
#include "SDK/WeakObjectHandle.h"
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
struct Event { uint64_t first,last; std::string function,context,source; unsigned repeats=1; nlohmann::json fields=nullptr, parameters=nullptr; };
bool captureParameters=false;
unsigned parameterCaptureCount=0;
nlohmann::json eventCaptures = nlohmann::json::array();
unsigned fieldCaptureCount = 0;
bool readingFields = false;
std::vector<Event> events;
PS::WeakObjectHandle playerRoot,controllerRoot,targetRoot;
bool targetScope=false;
std::string targetPath;
std::atomic<bool> active{false};
std::atomic<unsigned> otherThreads{};
Hook::GlobalCallbackId callback=Hook::ERROR_ID;
bool session{};std::atomic<unsigned> thread{};unsigned startThread{},mask=15;uint64_t start{},duration=30000;
size_t limit=1024;std::string filter;bool suppressTicks=true;
NameFilters nameFilters;
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
    if(eventCaptures.empty() && !captureParameters && !events.empty() && events.back().function==name && events.back().context==context && events.back().source==source) {
        ++events.back().repeats;events.back().last=elapsed;
    } else if(events.size()<limit)events.push_back({elapsed,elapsed,std::move(name),std::move(context),std::move(source),1});
    if(++seen>=limit) { reason="event limit";active.store(false,std::memory_order_release); }
}
void Observe(Hook::TCallbackIterationData<void>&,UObject* context,UFunction* function,void* parameters) {
    if(!active.load(std::memory_order_acquire))return;
    if(readingFields)return;
    if(GetTickCount64()-start>=duration) { reason="duration limit";active.store(false);return; }
    if(!context || !function)return;
    auto* player=playerRoot.Get();auto* controller=controllerRoot.Get();
    if(!targetScope && (!player || !controller)) { reason="player/controller unavailable";active.store(false);return; }
    auto* target=targetRoot.Get();
    if(targetScope && !target) { reason="selected object unavailable";active.store(false);return; }
    bool owned=false;
    unsigned depth=0;
    for(auto* outer=context;outer && depth++<8;outer=outer->GetOuterPrivate()) {
        if(targetScope ? outer==target : (outer==player || outer==controller)) { owned=true;break; }
        if(outer->GetOuterPrivate()==outer)break;
    }
    if(!owned)return;
    const auto currentThread=GetCurrentThreadId();
    unsigned expected=0;
    if(!thread.compare_exchange_strong(expected,currentThread,std::memory_order_acq_rel)
        && expected!=currentThread) { otherThreads.fetch_add(1,std::memory_order_relaxed);return; }
    try {
        auto name=to_string(function->GetPathName());auto lower=Lower(name);
        if((suppressTicks && (lower.find("tick")!=lower.npos || lower.find("updateanimation")!=lower.npos
            || lower.find("blueprintpostevaluateanimation")!=lower.npos
            || lower.find("blueprintpreevaluateanimation")!=lower.npos))
            || !(mask&Category(lower)) || (!filter.empty() && lower.find(filter)==lower.npos) || !nameFilters.MatchesLower(lower)) { ++suppressed;return; }
        Append(std::move(name),to_string(context->GetPathName()),"reflected ProcessEvent post");
        if (captureParameters && parameterCaptureCount<16 && !events.empty()) {
            ++parameterCaptureCount;
            events.back().parameters=InspectionTools::CaptureEventParameters(function,parameters);
        }
        if (!eventCaptures.empty() && fieldCaptureCount < 16 && !events.empty()) {
            struct ReadingGuard { ReadingGuard(){readingFields=true;} ~ReadingGuard(){readingFields=false;} } guard;
            ++fieldCaptureCount;
            auto& fields=events.back().fields;
            fields=nlohmann::json::array();
            InspectionTools::CaptureBudget budget;
            budget.remaining=512;budget.maxDepth=4;budget.maxEntries=32;budget.maxSparseSlots=128;
            for (const auto& capture : eventCaptures) {
                const auto root=capture["Root"].get<std::string>();
                auto* object=root=="Context"?context:root=="Player"?player:root=="Target"?target:controller;
                try { fields.push_back({{"capture",capture},{"report",InspectionTools::CaptureProperty(object,capture["Path"],budget)}}); }
                catch (const std::exception& error) { fields.push_back({{"capture",capture},{"error",error.what()}}); }
            }
        }
    } catch(...) { reason="capture error";active.store(false); }
}
void Detach() { active.store(false);if(callback!=Hook::ERROR_ID) { Hook::UnregisterCallback(callback);callback=Hook::ERROR_ID; } }
}
void Start(UObject* player,UObject* controller,const nlohmann::json& options,UObject* target) {
    if(session)throw std::runtime_error("Stop/export the previous Player Trace first.");
    ValidateOptions(options);
    targetScope=options.value("TraceSelectedObject",false);
    if(!targetScope && (!player || !controller))throw std::runtime_error("Enter a world before tracing, or select a live object for object trace.");
    if(targetScope && (!target || target->IsA(UClass::StaticClass()) || target->IsA(UFunction::StaticClass())))
        throw std::runtime_error("Inspect/select a live object before tracing that target.");
    targetRoot=PS::WeakObject(targetScope?target:nullptr);
    targetPath=targetScope?to_string(target->GetPathName()):std::string{};
    eventCaptures=options.value("EventCaptures",nlohmann::json::array());fieldCaptureCount=0;readingFields=false;
    captureParameters=options.value("CaptureParameters",false);parameterCaptureCount=0;
    auto seconds=options.value("Seconds",30);auto maximum=options.value("MaxEvents",1024);auto categories=options.value("Categories",15);
    auto text=options.value("Filter",std::string{});
    nameFilters=NameFilters(options);
    playerRoot=PS::WeakObject(player);controllerRoot=PS::WeakObject(controller);
    startThread=GetCurrentThreadId();thread.store(0,std::memory_order_relaxed);
    playerPath=player?to_string(player->GetPathName()):std::string{};
    controllerPath=controller?to_string(controller->GetPathName()):std::string{};
    mask=categories;filter=Lower(text);duration=seconds*1000ull;limit=maximum;suppressTicks=options.value("SuppressTicks",true);
    events.clear();events.reserve(limit);seen=0;suppressed=0;otherThreads.store(0);reason="manual stop";start=GetTickCount64();
    Hook::FCallbackOptions hookOptions{};hookOptions.OwnerModName=TEXT("RuneSchema");hookOptions.HookName=TEXT("ManualPlayerTrace");
    callback=Hook::RegisterProcessEventPostCallback(Observe,hookOptions);
    if(callback==Hook::ERROR_ID) { Cancel();throw std::runtime_error("UE4SS Player Trace callback unavailable."); }
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
    nlohmann::json result={{"kind","PlayerTrace1"},{"player",playerPath},{"controller",controllerPath},{"threadId",thread.load()},
        {"startThreadId",startThread},
        {"reason",reason},{"matchedEvents",seen},{"filteredOwnedCalls",suppressed},{"skippedOtherThreadCalls",otherThreads.load()},
        {"categories",mask},{"filter",filter},{"suppressTicks",suppressTicks},{"durationLimitMs",duration},{"eventLimit",limit},{"events",nlohmann::json::array()},
        {"coverage","Reflected post-calls within the selected scope, captured on the thread of its first owned call. Direct native calls and objects outside the selected Outer hierarchy may be absent. Categories are inferred from names; rows are completion order, not call nesting. Optional field snapshots are post-call object state, not function parameters."}};
    result["eventCapturePaths"]=eventCaptures;
    result["includeAny"]=nameFilters.Include;result["excludeAny"]=nameFilters.Exclude;
    result["selectedTarget"]=targetPath;
    result["scope"]=targetScope?"selected object and Outer descendants":"player/controller and Outer descendants";
    result["fieldCaptureCount"]=fieldCaptureCount;
    result["fieldCaptureLimit"]=16;
    result["parameterCaptureCount"]=parameterCaptureCount;
    result["parameterCaptureLimit"]=16;
    result["parameterCoverage"]="Opt-in post-call numeric, enum, native bool and direct object-path values, including nested structs. Arrays expose Count only; elements and references are not traversed. Strings, maps, sets, optional values and return values are omitted. Unsupported layouts are omitted. IDs remain raw.";
    for(size_t i=0;i<events.size();++i) {
        const auto& e=events[i];result["events"].push_back({{"sequence",i},{"firstMs",e.first},{"lastMs",e.last},
            {"function",e.function},{"context",e.context},{"source",e.source},{"consecutiveCount",e.repeats}});
        if (!e.fields.is_null()) result["events"].back()["fieldCaptures"]=e.fields;
        if (!e.parameters.is_null()) result["events"].back()["parameters"]=e.parameters;
    }
    Cancel();return result;
}
void Cancel() {
    Detach();std::vector<Event>().swap(events);session=false;
    eventCaptures=nullptr;
    nameFilters=NameFilters();
    playerRoot.Reset();
    controllerRoot.Reset();
    targetRoot.Reset();
    std::string().swap(playerPath);std::string().swap(controllerPath);std::string().swap(targetPath);
    std::string().swap(filter);std::string().swap(reason);
}
}
