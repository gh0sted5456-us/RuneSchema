#pragma once
#include "Core/NativeServerFilterContract.h"
#include "Runtime/Storefront.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/WeakObjectHandle.h"
#include "Utility/Logging.h"
#include <Unreal/AGameModeBase.hpp>
#include <Unreal/Hooks.hpp>
#include <Unreal/World.hpp>

namespace PS::ServerFilterStartup {
namespace Detail {
using namespace RC::Unreal;
inline Hook::GlobalCallbackId WorldHook=Hook::ERROR_ID;
inline WeakObjectHandle InitializedInstance;
using ApplySettings=void(*)(UObject*,bool,UWorld*);
inline ApplySettings ApplyNative=nullptr;
inline void Warn(const std::exception& error) {
    Log<RC::LogLevel::Warning>(STR("[RS-SRV-002] Native server filter setup unavailable: {}. Existing game handling retained.\n"),ToWideSafe(error.what()));
}
inline void Apply(AGameModeBase* mode) {
    if(!ApplyNative || !mode || mode->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject)))return;
    auto* world=mode->GetWorld();if(!world)return;
    auto* instance=DragonWilds::ActorHelper::GetObjectRef(world,TEXT("OwningGameInstance"));
    if(!instance)throw std::runtime_error("Server world has no owning game instance");
    if(InitializedInstance.Get()==instance)return;
    auto* defaults=UECustom::UObjectGlobals::StaticFindObject(nullptr,nullptr,TEXT("/Script/Dominion.Default__DominionUserSettings"));
    if(!defaults)throw std::runtime_error("Native user-settings defaults are unavailable");
    DragonWilds::ActorHelper::FunctionCall lookup(defaults,TEXT("/Script/Dominion.DominionUserSettings:GetDominionUserSettings"));
    lookup.Invoke();auto* settings=lookup.Result<UObject*>();
    if(!settings)throw std::runtime_error("Native user settings are unavailable");
    auto* accessibility=DragonWilds::ActorHelper::GetObjectRef(settings,TEXT("AccessibilitySettings"));
    if(!accessibility || RC::to_string(accessibility->GetClassPrivate()->GetPathName())!="/Script/Dominion.DominionAccessibilitySettings")
        throw std::runtime_error("Native accessibility settings type differs");
    // The client settings path has no server world; initialize the same configured filters here.
    ApplyNative(accessibility,true,world);
    InitializedInstance.Assign(instance);
    Log<RC::LogLevel::Normal>(STR("[RS-SRV-001] Requested native server filter setup using existing accessibility settings.\n"));
}
}
inline void Stop() noexcept {
    if(Detail::WorldHook!=RC::Unreal::Hook::ERROR_ID){RC::Unreal::Hook::UnregisterCallback(Detail::WorldHook);Detail::WorldHook=RC::Unreal::Hook::ERROR_ID;}
    Detail::ApplyNative=nullptr;Detail::InitializedInstance.Reset();
}
inline void Start() {
    Stop();if(!Storefront::IsDedicatedServer())return;
    try {
        using namespace RC::Unreal;
        const auto* image=reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
        const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
        const auto* pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(image+dos->e_lfanew);
        if(!NativeServerFilterContract::Supports(pe->FileHeader.TimeDateStamp,pe->OptionalHeader.SizeOfImage))
            throw std::runtime_error("This dedicated-server build's native filter setup has not been verified");
        NativeServerFilterContract::Verify(pe->FileHeader.TimeDateStamp,pe->OptionalHeader.SizeOfImage,
            {image+NativeServerFilterContract::ApplyAddress,NativeServerFilterContract::ApplyCode.size()});
        Detail::ApplyNative=reinterpret_cast<Detail::ApplySettings>(const_cast<unsigned char*>(image)+NativeServerFilterContract::ApplyAddress);
        Hook::FCallbackOptions options{};options.OwnerModName=TEXT("RuneSchema");options.HookName=TEXT("NativeServerFilterStartup");
        Detail::WorldHook=Hook::RegisterInitGameStatePostCallback([](auto&,AGameModeBase* mode){
            try{Detail::Apply(mode);}catch(const std::exception& error){Detail::Warn(error);}
        },options);
        if(Detail::WorldHook==Hook::ERROR_ID)throw std::runtime_error("Server filter startup callback is unavailable");
        TArray<UObject*> modes;UECustom::UObjectGlobals::GetObjectsOfClass(AGameModeBase::StaticClass(),modes,true);
        for(auto* mode:modes)Detail::Apply(static_cast<AGameModeBase*>(mode));
    }catch(const std::exception& error){Stop();Detail::Warn(error);}
}
}
