#pragma once
#include <Windows.h>
#include <safetyhook.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <unordered_map>
#include "Core/RegistrationDiagnostics.h"
#include "Loader/QuestObjectReference.h"
#include "SDK/WeakObjectHandle.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Structs/Custom/FManagedValue.h"
#include "SDK/Structs/Custom/FScriptMapHelper.h"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Runtime/Storefront.h"
#include "Runtime/NativeExecutableDiscovery.h"
#include "Runtime/NativeHookPatterns.h"
#include "Utility/Logging.h"

namespace DragonWilds::EquipmentWearableBridge {
using namespace RC::Unreal;
// Native code, not RuneSchema, allocates and populates the opaque cache values.
// Known legacy profiles are a fast path only. Updated executables use unique,
// unwind-bounded code discovery plus the reflected cache-layout checks below.
struct NativeProfile {
    uint32_t Timestamp, ImageSize;
    PS::Storefront::NativeLane Lane;
    uintptr_t BuilderRva, EvaluatorRva;
    std::array<uint8_t,24> BuilderBytes, EvaluatorBytes;
    const TCHAR* Name;
};
inline constexpr NativeProfile SteamProfile{
    0xd702bba2,0xddf1000,PS::Storefront::NativeLane::SteamNative,
    0x6384dd0,0x657f900,
    {0x48,0x89,0x5c,0x24,0x08,0x44,0x88,0x44,0x24,0x18,0x55,0x56,
     0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8b,0xec},
    {0x48,0x85,0xc9,0x0f,0x84,0x0b,0x04,0x00,0x00,0x4c,0x89,0x44,
     0x24,0x18,0x53,0x56,0x57,0x41,0x56,0x48,0x81,0xec,0x98,0x00},
    TEXT("Steam")};
inline constexpr NativeProfile GamePassProfile{
    0xb0da0883,0x0db4e000,PS::Storefront::NativeLane::GamePassNative,
    0x6caf850,0x6e7de30,
    {0x44,0x88,0x44,0x24,0x18,0x48,0x89,0x54,0x24,0x10,0x48,0x89,
     0x4c,0x24,0x08,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41},
    {0x48,0x85,0xc9,0x0f,0x84,0x53,0x05,0x00,0x00,0x4c,0x89,0x4c,
     0x24,0x20,0x4c,0x89,0x44,0x24,0x18,0x56,0x41,0x56,0x48,0x81},
    TEXT("Game Pass/WinGDK 100.8.0.0")};
using Builder=void(*)(UObject*,void*,uint8_t);
inline Builder BuildCache{};
inline safetyhook::MidHook EvaluatorHook;
inline std::atomic<DWORD> GameThread{};
inline std::atomic<bool> Inactive{};
inline std::atomic<bool> ServerPathReported{};
inline bool InstallAttempted=false;
inline thread_local bool Refreshing=false;
inline std::unordered_map<UObject*,PS::WeakObjectHandle> Registered;
inline PS::RegistrationDiagnostics::Sources PendingDiagnostics;
inline void ReportDiagnostics() {
    const auto counts=PendingDiagnostics.Take();
    if(!PS::PSConfig::Get()->IsDebugLoggingEnabled())return;
    for(const auto& [source,count]:counts)
        PS::Log<RC::LogLevel::Verbose>(TEXT("[WEARABLE-BRIDGE][SUMMARY] source='{}' added={} existing={}; individual asset messages suppressed.\n"),
            PS::ToWideSafe(source),count.Added,count.Existing);
}
struct Attempt { PS::WeakObjectHandle Component,Mesh; uint8_t Gender; };
// Retain only failed enrollments. A successful cache entry may later be
// invalidated by the game when equipment changes, so it must be retryable.
inline std::vector<Attempt> FailedAttempts;

inline bool Live(UObject* object) {
    return object && !object->HasAnyFlags(static_cast<EObjectFlags>(
        RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed));
}
inline bool IsWearable(UObject* item){
    auto* type=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.WearableEquipmentData"));
    return Live(item)&&type&&item->IsA(type);
}

inline UObject* ObjectField(UObject* object,const TCHAR* name) {
    auto* property=CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(object->GetClassPrivate(),name));
    return property?property->GetObjectPropertyValue(property->ContainerPtrToValuePtr<void>(object)):nullptr;
}

inline bool ContainsMesh(UObject* component,UObject* mesh) {
    auto* property=CastField<FMapProperty>(PropertyHelper::GetPropertyByName(
        component->GetClassPrivate(),TEXT("EquipmentMeshDataToMaterialsToHide")));
    if(!property || property->GetOffset_Internal()!=0xc0 || property->GetSize()!=0x50)
        throw std::runtime_error("wearable hide-map layout differs from the verified native profile");
    auto* key=CastField<FObjectPropertyBase>(property->GetKeyProp());
    if(!key || key->GetElementSize()!=sizeof(UObject*))
        throw std::runtime_error("wearable hide-map key layout changed");
    auto* data=property->ContainerPtrToValuePtr<void>(component);
    auto* map=static_cast<FScriptMap*>(data);
    if(map->Num()<0 || map->Num()>8192 || map->GetMaxIndex()>16384)
        throw std::runtime_error("wearable hide-map exceeds diagnostic bounds");
    bool found=false;
    UECustom::FScriptMapHelper helper(property,data);
    helper.ForEachPair([&](void* k,void*) { if(key->GetObjectPropertyValue(k)==mesh) found=true; });
    return found;
}

inline void BeforeEvaluation(safetyhook::Context& context) noexcept {
    if(Inactive.load(std::memory_order_acquire) || Refreshing
        || GetCurrentThreadId()!=GameThread.load(std::memory_order_acquire)) return;
    try {
        auto* item=reinterpret_cast<UObject*>(context.rcx);
        const auto registered=Registered.find(item);
        if(registered==Registered.end()||registered->second.Get()!=item) return;
        auto* component=reinterpret_cast<UObject*>(context.r9);
        const auto gender=static_cast<uint8_t>(context.rdx);
        if(gender>1 || !Live(item) || !Live(component) || !context.r8) return;
        auto* mesh=ObjectField(item,gender==1?TEXT("FemaleMeshData"):TEXT("MaleMeshData"));
        if(!Live(mesh) || ContainsMesh(component,mesh)) return;
        // The game passes a live FScriptInterface pair here. Its second pointer
        // is the same mesh interface used by the normal native cache builder.
        auto** pair=reinterpret_cast<void**>(context.r8);
        if(!Live(static_cast<UObject*>(pair[0])) || !pair[1]) return;
        std::erase_if(FailedAttempts,[](const Attempt& a){return !Live(a.Component.Get()) || !Live(a.Mesh.Get());});
        if(std::ranges::any_of(FailedAttempts,[&](const Attempt& a){return a.Component.Get()==component && a.Mesh.Get()==mesh && a.Gender==gender;})) return;
        if(FailedAttempts.size()>=256) throw std::runtime_error("wearable refresh failure budget exhausted");
        Refreshing=true;
        BuildCache(component,pair[1],gender);
        Refreshing=false;
        const bool enrolled=ContainsMesh(component,mesh);
        if(!enrolled) FailedAttempts.push_back({PS::WeakObject(component),PS::WeakObject(mesh),gender});
        if(!enrolled)PS::Log<RC::LogLevel::Warning>(TEXT("[WEARABLE-BRIDGE][NOT-ENROLLED] item='{}'; native hide-cache rebuild did not confirm enrollment.\n"),item->GetPathName());
    } catch(const std::exception& error) {
        Refreshing=false;
        Inactive.store(true,std::memory_order_release);
        PS::Log<RC::LogLevel::Warning>(TEXT("[WEARABLE-BRIDGE][INACTIVE] {}; native cache refresh disabled.\n"),PS::ToWideSafe(error.what()));
    } catch(...) {
        Refreshing=false;
        Inactive.store(true,std::memory_order_release);
        PS::Log<RC::LogLevel::Warning>(TEXT("[WEARABLE-BRIDGE][INACTIVE] unknown native cache refresh failure.\n"));
    }
}

inline void Install() {
    if(InstallAttempted) return;
    InstallAttempted=true;
    const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if(!dos || dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>4096) return;
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    const auto lane=PS::Storefront::CurrentNativeLane();
    const NativeProfile* profile=nullptr;
    for(const auto* candidate:{&SteamProfile,&GamePassProfile}) {
        if(candidate->Timestamp==nt->FileHeader.TimeDateStamp
            && candidate->ImageSize==nt->OptionalHeader.SizeOfImage
            && candidate->Lane==lane) { profile=candidate; break; }
    }
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64)return;
    void* builder=nullptr;void* evaluator=nullptr;
    if(profile
        &&profile->BuilderRva+profile->BuilderBytes.size()<=nt->OptionalHeader.SizeOfImage
        &&profile->EvaluatorRva+profile->EvaluatorBytes.size()<=nt->OptionalHeader.SizeOfImage
        &&!std::memcmp(reinterpret_cast<void*>(base+profile->BuilderRva),profile->BuilderBytes.data(),profile->BuilderBytes.size())
        &&!std::memcmp(reinterpret_cast<void*>(base+profile->EvaluatorRva),profile->EvaluatorBytes.data(),profile->EvaluatorBytes.size())) {
        builder=reinterpret_cast<void*>(base+profile->BuilderRva);evaluator=reinterpret_cast<void*>(base+profile->EvaluatorRva);
    } else if(lane==PS::Storefront::NativeLane::GamePassNative) {
        builder=PS::NativeExecutableDiscovery::Resolve(PS::NativeHookPatterns::GamePassWearableBuilder);
        evaluator=PS::NativeExecutableDiscovery::Resolve(PS::NativeHookPatterns::GamePassWearableEvaluator);
    } else if(lane==PS::Storefront::NativeLane::SteamNative) {
        builder=PS::NativeExecutableDiscovery::Resolve(PS::NativeHookPatterns::SteamWearableBuilder);
        evaluator=PS::NativeExecutableDiscovery::Resolve(PS::NativeHookPatterns::SteamWearableEvaluator);
    }
    if(!builder||!evaluator) {
        PS::Log<RC::LogLevel::Warning>(TEXT("[WEARABLE-BRIDGE][INACTIVE] compatible native cache functions could not be uniquely identified; item registration remains available.\n"));
        return;
    }
    auto hook=safetyhook::MidHook::create(evaluator,BeforeEvaluation,safetyhook::MidHook::StartDisabled);
    if(!hook) throw std::runtime_error("wearable evaluator hook creation failed");
    BuildCache=reinterpret_cast<Builder>(builder);
    EvaluatorHook=std::move(*hook);
    if(!EvaluatorHook.enable()) { EvaluatorHook.reset(); BuildCache=nullptr; throw std::runtime_error("wearable evaluator hook activation failed"); }
    PS::Log<RC::LogLevel::Normal>(TEXT("[WEARABLE-BRIDGE][ACTIVE] native cache builder and wearable evaluator verified.\n"));
}

// Call after persistence/network identity registration, on the game thread.
inline void Register(UObject* subsystem,UObject* item) {
    if(!Live(subsystem)||!IsWearable(item))return;
    auto* property=CastField<FMapProperty>(PropertyHelper::GetPropertyByName(subsystem->GetClassPrivate(),TEXT("AssetNameToDataMap")));
    if(!property || !CastField<FStrProperty>(property->GetKeyProp())) throw std::runtime_error("wearable enumeration registry unavailable");
    auto* value=CastField<FObjectPropertyBase>(property->GetValueProp());
    if(!value || value->GetElementSize()!=sizeof(UObject*) || !value->GetPropertyClass().Get() || !item->IsA(value->GetPropertyClass().Get()))
        throw std::runtime_error("wearable enumeration value type mismatch");
    QuestRegistry::ValidateObjectReferenceLayout(RC::to_string(value->GetClass().GetName()),
        value->GetElementSize(),value->GetArrayDim(),sizeof(UObject*));
    const FString name(item->GetName().c_str());
    auto* data=property->ContainerPtrToValuePtr<void>(subsystem);
    auto* map=static_cast<FScriptMap*>(data);
    if(map->Num()<0 || map->Num()>65535 || map->GetMaxIndex()>131072) throw std::runtime_error("wearable enumeration registry exceeds bounds");
    UECustom::FScriptMapHelper helper(property,data);
    bool found=false;
    helper.ForEachPair([&](void* k,void* v){
        if(property->GetKeyProp()->Identical(k,const_cast<FString*>(&name))) {
            if(found || value->GetObjectPropertyValue(v)!=item) throw std::runtime_error("wearable asset name collides with another registry entry");
            found=true;
        }
    });
    if(!found) {
        UECustom::FManagedValue pair;
        helper.InitializePair(pair);
        property->GetKeyProp()->CopyCompleteValue(helper.GetKeyPtr(pair.GetData()),&name);
        // Match the supported QuestNativeRegistry path; the host does not implement
        // FObjectPropertyBase::SetObjectPropertyValue for this engine build.
        QuestRegistry::CopyResolvedReference(item,helper.GetValuePtr(pair.GetData()),
            [&](void* target,const void* source){value->CopyCompleteValue(target,source);},
            [&](void* target){return value->GetObjectPropertyValue(target);});
        helper.Add(pair);
        auto* stored=helper.FindValue(const_cast<FString*>(&name));
        if(!stored || value->GetObjectPropertyValue(stored)!=item) throw std::runtime_error("wearable enumeration insertion did not round-trip");
    }
    const auto prior=Registered.find(item);
    if(prior==Registered.end()||prior->second.Get()!=item){
        Registered.insert_or_assign(item,PS::WeakObject(item));
        if(PS::PSConfig::Get()->IsDebugLoggingEnabled())
            PendingDiagnostics.Add(RC::to_string(item->GetPathName()),!found);
    }
    if (PS::Storefront::IsDedicatedServer()) {
        if (!ServerPathReported.exchange(true, std::memory_order_relaxed))
            PS::Log<RC::LogLevel::Normal>(TEXT("[WEARABLE-BRIDGE][SERVER-DATA] Authoritative wearable enumeration is active; visual hide-map refresh remains client-only.\n"));
        return;
    }
    GameThread.store(GetCurrentThreadId(),std::memory_order_release);
    Install();
}

// Presentation support is optional: a bridge failure must not reject an item
// that already has valid persistence and network registration. Stop after the
// first failure instead of retrying the same unsupported operation for every item.
inline void TryRegister(UObject* subsystem,UObject* item) noexcept {
    if(Inactive.load(std::memory_order_acquire)) return;
    try {
        Register(subsystem,item);
    } catch(const std::exception& error) {
        Inactive.store(true,std::memory_order_release);
        PS::Log<RC::LogLevel::Warning>(TEXT("[WEARABLE-BRIDGE][INACTIVE] item='{}': {}; normal item registration continues.\n"),
            item?item->GetPathName():TEXT("<null>"),PS::ToWideSafe(error.what()));
    } catch(...) {
        Inactive.store(true,std::memory_order_release);
        PS::Log<RC::LogLevel::Warning>(TEXT("[WEARABLE-BRIDGE][INACTIVE] unknown wearable enrollment failure; normal item registration continues.\n"));
    }
}
// Schema patches may arrive after the settled registrar. Re-enroll only an
// item whose existing persistence AND two-way network identity round-trip.
// Never allocate a network ID, synthesize an item, or mutate an opaque cache.
inline bool RefreshRegistered(UObject* item) noexcept {
    try {
        if(Inactive.load(std::memory_order_acquire)||!IsWearable(item))return false;
        auto* idField=CastField<FStrProperty>(PropertyHelper::GetPropertyByName(item->GetClassPrivate(),TEXT("PersistenceID")));
        if(!idField)return false;const auto id=idField->GetPropertyValue(idField->ContainerPtrToValuePtr<void>(item));
        if(id.GetCharArray().Num()<=1)return false;
        auto* type=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.ItemSubsystem"));
        if(!type)return false;TArray<UObject*> instances;UECustom::UObjectGlobals::GetObjectsOfClass(type,instances,true);
        if(instances.Num()<0||instances.Num()>256)return false;
        bool enrolled=false;
        for(auto* subsystem:instances){
            if(!Live(subsystem))continue;
            auto* identity=CastField<FMapProperty>(PropertyHelper::GetPropertyByName(type,TEXT("PersistenceIDToDataMap")));
            auto* reverse=CastField<FMapProperty>(PropertyHelper::GetPropertyByName(type,TEXT("DataToNetIdMap")));
            auto* forward=CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(type,TEXT("NetIdToData")));
            if(!identity||!reverse||!forward)continue;
            auto* identityValue=CastField<FObjectPropertyBase>(identity->GetValueProp());
            auto* reverseKey=CastField<FObjectPropertyBase>(reverse->GetKeyProp());
            auto* reverseValue=CastField<FNumericProperty>(reverse->GetValueProp());
            auto* forwardValue=CastField<FObjectPropertyBase>(forward->GetInner());
            if(!CastField<FStrProperty>(identity->GetKeyProp())||!identityValue||identityValue->GetElementSize()!=sizeof(UObject*)
                ||!reverseKey||reverseKey->GetElementSize()!=sizeof(UObject*)||!reverseValue||!reverseValue->IsInteger()
                ||reverseValue->GetElementSize()!=sizeof(uint16_t)||!forwardValue||forwardValue->GetElementSize()!=sizeof(UObject*))continue;
            for(auto* field:{identityValue,reverseKey,forwardValue})QuestRegistry::ValidateObjectReferenceLayout(
                RC::to_string(field->GetClass().GetName()),field->GetElementSize(),field->GetArrayDim(),sizeof(UObject*));
            auto* identityData=identity->ContainerPtrToValuePtr<FScriptMap>(subsystem);
            auto* reverseData=reverse->ContainerPtrToValuePtr<FScriptMap>(subsystem);
            auto* array=forward->ContainerPtrToValuePtr<FScriptArray>(subsystem);
            if(!identityData||!reverseData||!array||identityData->Num()<0||identityData->Num()>65536||identityData->GetMaxIndex()>131072
                ||reverseData->Num()<0||reverseData->Num()>65536||reverseData->GetMaxIndex()>131072||array->Num()<0||array->Num()>65536)continue;
            UECustom::FScriptMapHelper ids(identity,identityData),network(reverse,reverseData);
            const auto* stored=ids.FindValue(const_cast<FString*>(&id));
            if(!stored||identityValue->GetObjectPropertyValue(stored)!=item)continue;
            const auto* net=network.FindValue(&item);if(!net)continue;
            uint16_t index=0;std::memcpy(&index,net,sizeof(index));if(index>=array->Num())continue;
            FScriptArrayHelper values(forward,array);
            if(forwardValue->GetObjectPropertyValue(values.GetRawPtr(index))!=item)continue;
            TryRegister(subsystem,item);const auto found=Registered.find(item);
            enrolled=enrolled||(found!=Registered.end()&&found->second.Get()==item);
        }
        return enrolled;
    }catch(...){return false;} // Optional presentation must not reject a valid item.
}
}
