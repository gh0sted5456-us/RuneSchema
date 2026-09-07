#include "Loader/AppearanceEvents.h"
#include "Generator/AppearanceTraceContract.h"
#include <Windows.h>
#include <safetyhook.hpp>
#include <atomic>
#include <stdexcept>
#include <cstring>
namespace DragonWilds::AppearanceEvents {
namespace {
using namespace PS::AppearanceTraceContract;
std::array<safetyhook::MidHook,std::size(Sites)> hooks;
std::atomic<Observer> observers[2]{};
void Notify(unsigned site, uintptr_t token, uint32_t parameter=0) {
    for(auto& slot:observers) if(auto observer=slot.load(std::memory_order_acquire)) observer(site,token,parameter);
}
void Wearable(safetyhook::Context& c) { Notify(0,c.rdi); }
void Left(safetyhook::Context& c) { Notify(1,c.rbx); }
void Right(safetyhook::Context& c) { Notify(2,c.rbx); }
void Customization(safetyhook::Context& c) { Notify(3,c.rbx); }
void Ramp(safetyhook::Context& c) { Notify(4,c.rcx,static_cast<uint32_t>(c.rdx)); }
constexpr safetyhook::MidHookFn callbacks[]{Wearable,Left,Right,Customization,Ramp};
struct ShutdownGuard { ~ShutdownGuard() {
    for(auto& observer:observers) observer.store(nullptr);
    for(auto& hook:hooks) hook={};
} } shutdownGuard;
}
void Subscribe(Consumer consumer, Observer observer) {
    const auto index=static_cast<unsigned>(consumer);
    if (!observer) throw std::runtime_error("Missing appearance observer.");
    if (hooks[0]) { observers[index].store(observer,std::memory_order_release); return; }
    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) throw std::runtime_error("Invalid game image.");
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64
        || nt->FileHeader.TimeDateStamp != Timestamp || nt->OptionalHeader.SizeOfImage != ImageSize)
        throw std::runtime_error("Appearance trace game version mismatch; no hooks installed.");
    for (const auto& site : Sites)
        if (site.rva + site.bytes.size() > ImageSize || std::memcmp(reinterpret_cast<void*>(base + site.rva),site.bytes.data(),site.bytes.size()))
            throw std::runtime_error("Appearance trace instruction mismatch; no hooks installed.");
    try {
        for (size_t i=0; i<hooks.size(); ++i) {
            auto hook=safetyhook::MidHook::create(reinterpret_cast<void*>(base+Sites[i].rva),callbacks[i],safetyhook::MidHook::StartDisabled);
            if (!hook) throw std::runtime_error("Appearance hook creation failed.");
            hooks[i]=std::move(*hook);
        }
        for (auto& hook:hooks) if (!hook.enable()) throw std::runtime_error("Appearance hook activation failed.");
    } catch (...) { for(auto& hook:hooks) hook={}; throw; }
    observers[index].store(observer,std::memory_order_release);
}
void Unsubscribe(Consumer consumer) {
    observers[static_cast<unsigned>(consumer)].store(nullptr,std::memory_order_release);
    if(observers[0].load() || observers[1].load()) return;
    for(auto& hook:hooks) hook={};
}
}
