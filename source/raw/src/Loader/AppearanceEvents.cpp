#include "Loader/AppearanceEvents.h"
#include "Generator/AppearanceTraceContract.h"
#include "Generator/AppearanceResolver.h"
#include <Windows.h>
#include <safetyhook.hpp>
#include <atomic>
#include <stdexcept>
#include <cstring>
#include <mutex>
namespace DragonWilds::AppearanceEvents {
namespace {
using namespace PS::AppearanceTraceContract;
std::array<safetyhook::MidHook,std::size(Sites)> hooks;
std::atomic<Observer> observers[2]{};
std::once_flag resolutionOnce;
std::array<uintptr_t,std::size(Sites)> resolved{};
std::array<std::array<uint8_t,16>,std::size(Sites)> originalBytes{};
std::atomic<bool> unavailable{false};
std::string resolutionError;
void Resolve() {
    std::call_once(resolutionOnce,[] {
        try {
            const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
            const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
            if(!dos || dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<0 || dos->e_lfanew>0x100000)
                throw std::runtime_error("Invalid appearance game image");
            const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
            if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64
               || nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC
               || nt->FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER64)
               || !nt->FileHeader.NumberOfSections || nt->FileHeader.NumberOfSections>96)
                throw std::runtime_error("Unsupported appearance PE layout");
            const auto size=nt->OptionalHeader.SizeOfImage;
            std::vector<PS::AppearanceResolver::Section> sections;
            const auto* section=IMAGE_FIRST_SECTION(nt);
            if(reinterpret_cast<uintptr_t>(section)-base+sizeof(*section)*nt->FileHeader.NumberOfSections>size)
                throw std::runtime_error("Invalid appearance section table");
            for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
                if(!(section[i].Characteristics&IMAGE_SCN_MEM_READ) || (section[i].Characteristics&IMAGE_SCN_MEM_DISCARDABLE))continue;
                sections.push_back({section[i].VirtualAddress,section[i].Misc.VirtualSize,
                    (section[i].Characteristics&IMAGE_SCN_MEM_EXECUTE)!=0});
            }
            std::span<const uint8_t> image(reinterpret_cast<const uint8_t*>(base),size);
            for(size_t i=0;i<resolved.size();++i) {
                resolved[i]=PS::AppearanceResolver::Resolve(image,sections,PS::AppearanceSignatures::Definitions[i]);
                std::memcpy(originalBytes[i].data(),image.data()+resolved[i],originalBytes[i].size());
            }
        } catch(const std::exception& error) {
            resolutionError=error.what();unavailable.store(true,std::memory_order_release);
        }
    });
    if(unavailable.load(std::memory_order_acquire))throw std::runtime_error(resolutionError);
}
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
    if(index>=std::size(observers))throw std::runtime_error("Invalid appearance consumer");
    if (!observer) throw std::runtime_error("Missing appearance observer.");
    if (hooks[0]) { observers[index].store(observer,std::memory_order_release); return; }
    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    Resolve();
    try {
        for (size_t i=0; i<hooks.size(); ++i) {
            if(std::memcmp(reinterpret_cast<const void*>(base+resolved[i]),originalBytes[i].data(),originalBytes[i].size()))
                throw std::runtime_error("Appearance hook instructions changed after resolution");
            auto hook=safetyhook::MidHook::create(reinterpret_cast<void*>(base+resolved[i]),callbacks[i],safetyhook::MidHook::StartDisabled);
            if (!hook) throw std::runtime_error("Appearance hook creation failed.");
            hooks[i]=std::move(*hook);
        }
        for (auto& hook:hooks) if (!hook.enable()) throw std::runtime_error("Appearance hook activation failed.");
    } catch(const std::exception& error) {
        for(auto& hook:hooks)hook={};
        resolutionError=error.what();unavailable.store(true,std::memory_order_release);throw;
    }
    observers[index].store(observer,std::memory_order_release);
}
void Unsubscribe(Consumer consumer) {
    if(static_cast<unsigned>(consumer)>=std::size(observers))return;
    observers[static_cast<unsigned>(consumer)].store(nullptr,std::memory_order_release);
    if(observers[0].load() || observers[1].load()) return;
    for(auto& hook:hooks) hook={};
}
bool Unavailable() { return unavailable.load(std::memory_order_acquire); }
uintptr_t ResolvedRva(unsigned site) { return site<resolved.size()?resolved[site]:0; }
}
