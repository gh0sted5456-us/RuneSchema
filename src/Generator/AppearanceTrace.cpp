#include "Generator/AppearanceTrace.h"
#include "Generator/AppearanceTraceContract.h"
#include <Windows.h>
#include "Loader/AppearanceEvents.h"
#include <atomic>
#include <mutex>
#include <stdexcept>

namespace PS::AppearanceTrace {
namespace {
using namespace AppearanceTraceContract;
struct Event { uint64_t elapsed; unsigned site; uint32_t thread; uint32_t parameter; };
std::array<Event,256> events;
std::mutex eventMutex;
std::atomic<bool> active{false};
uintptr_t equipmentToken{}, customizationToken{}, cpdToken{};
uint64_t started{};
size_t count{};
bool reachedCapacity{};
bool session{};
bool detached=true;

// Native callbacks only record address tokens.
void Record(unsigned site, uintptr_t token, uint32_t parameter = 0) noexcept {
    if (!active.load(std::memory_order_acquire)) return;
    const auto expected = site < 3 ? equipmentToken : site == 3 ? customizationToken : cpdToken;
    if (token != expected) return;
    const auto now = GetTickCount64();
    std::lock_guard lock(eventMutex);
    if (!active.load(std::memory_order_relaxed)) return;
    if (now - started >= 60000) { active.store(false, std::memory_order_release); return; }
    events[count++] = {now - started, site, GetCurrentThreadId(), parameter};
    if (count == events.size()) { reachedCapacity = true; active.store(false, std::memory_order_release); }
}
void Detach() {
    active.store(false,std::memory_order_release);
    DragonWilds::AppearanceEvents::Unsubscribe(DragonWilds::AppearanceEvents::Consumer::Trace);
    detached=true;
}

}
void Start(uintptr_t equipment, uintptr_t customization, uintptr_t cpd) {
    if (session) throw std::runtime_error("Stop and export the existing appearance trace first.");
    if (!equipment || !customization || !cpd) throw std::runtime_error("Player appearance components unavailable; enter a world first.");
    equipmentToken=equipment; customizationToken=customization; cpdToken=cpd;
    { std::lock_guard lock(eventMutex); count=0; reachedCapacity=false; started=GetTickCount64(); }
    DragonWilds::AppearanceEvents::Subscribe(DragonWilds::AppearanceEvents::Consumer::Trace,Record);
    session=true;
    detached=false;
    active.store(true,std::memory_order_release);
}
void Cancel() { Detach(); session=false; }
void Tick() {
    if(!session || detached)return;
    if(!active.load(std::memory_order_acquire) || GetTickCount64()-started>=60000)Detach();
}
nlohmann::json Stop() {
    if (!session) throw std::runtime_error("No appearance trace has been started.");
    Detach();
    std::lock_guard lock(eventMutex);
    nlohmann::json result={{"kind","AppearanceEventTrace1"},{"durationLimitMs",60000},{"eventLimit",events.size()},
        {"reachedCapacity",reachedCapacity},{"elapsedMs",GetTickCount64()-started},{"events",nlohmann::json::array()},
        {"note","Routine returns do not prove asynchronous work is complete. Address tokens are never dereferenced by callbacks."}};
    for (size_t i=0;i<count;++i) {
        const auto& event=events[i];
        result["events"].push_back({{"sequence",i},{"elapsedMs",event.elapsed},{"site",Sites[event.site].name},
            {"rva",Sites[event.site].rva},{"threadId",event.thread},{"parameterIndex",event.parameter}});
    }
    session=false;
    return result;
}
}
