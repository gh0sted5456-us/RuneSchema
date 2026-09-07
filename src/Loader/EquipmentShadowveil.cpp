#include "Loader/EquipmentShadowveil.h"
#include "Loader/ShadowveilNativeContract.h"
#include "SDK/Helper/PropertyHelper.h"
#include "Unreal/UObject.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Helpers/Casting.hpp"
#include "Utility/Logging.h"
#include <Windows.h>
#include <safetyhook.hpp>
#include <atomic>
#include <cstring>
#include <vector>

using namespace RC;
using namespace RC::Unreal;
namespace DragonWilds {
namespace {
std::array<safetyhook::MidHook, std::size(ShadowveilNative::Sites)> Hooks;
std::vector<std::pair<RC::StringType, ShadowveilRules::ActionMask>> WearablePaths;
ShadowveilRules::ActionMask EnabledActions{};
static_assert(std::size(ShadowveilNative::Sites) == std::size(ShadowveilRules::ActionNames));
static_assert(std::size(ShadowveilNative::ServerSites) == std::size(ShadowveilNative::Sites));
std::atomic<bool> Active{false};
std::atomic<unsigned> Observed{0};
uintptr_t ImageBase{};
const NativeHookContract::Profile<ShadowveilNative::Site>* SelectedProfile{};

UObject* ObjectRef(UObject* owner, const TCHAR* name) {
    if (!owner || !owner->GetClassPrivate()) return nullptr;
    auto* field = CastField<FObjectProperty>(PropertyHelper::GetPropertyByName(owner->GetClassPrivate(), name));
    if (!field || field->GetArrayDim() != 1 || field->GetElementSize() != sizeof(UObject*)) return nullptr;
    return field->GetObjectPropertyValue(field->ContainerPtrToValuePtr<void>(owner));
}
bool ClassIs(UObject* object, const TCHAR* path) {
    if (!object) return false;
    for (UStruct* type = object->GetClassPrivate(); type; type = type->GetSuperStruct())
        if (type->GetPathName() == path) return true;
    return false;
}
bool IsConfiguredEffect(uintptr_t address, unsigned site) noexcept {
    if (!Active.load(std::memory_order_acquire) || !address) return false;
    try {
        auto* effect = reinterpret_cast<UObject*>(address);
        if (!ClassIs(effect, TEXT("/Script/Dominion.ShadowVeilGameplayEffectInstance"))) return false;
        auto* data = ObjectRef(effect, TEXT("Data"));
        if (!data || data->GetPathName() != TEXT("/Game/Gameplay/UtilityMagic/PerkSpells/ShadowVeil/GE_ShadowVeil.GE_ShadowVeil_C")) return false;
        auto* source = ObjectRef(effect, TEXT("Source"));
        if (!ClassIs(source, TEXT("/Script/Dominion.WearableEquipment"))) return false;
        // Match the originating item; replicated equipment slots can lag.
        unsigned inspected = 0;
        for (UStruct* type = source->GetClassPrivate(); type; type = type->GetSuperStruct()) {
            for (auto* property : TFieldRange<FProperty>(type, EFieldIterationFlags::None)) {
                if (++inspected > 128) return false;
                auto* ref = CastField<FObjectProperty>(property);
                if (!ref || ref->GetArrayDim() != 1 || ref->GetElementSize() != sizeof(UObject*)) continue;
                auto* asset = ref->GetObjectPropertyValue(ref->ContainerPtrToValuePtr<void>(source));
                if (!ClassIs(asset, TEXT("/Script/Dominion.WearableEquipmentData"))) continue;
                const auto path = asset->GetPathName();
                for (const auto& [allowed, actions] : WearablePaths) {
                    if (path != allowed) continue;
                    if (!(actions & (1u << site))) return false;
                    const unsigned bit = 1u << site;
                    if (!(Observed.fetch_or(bit, std::memory_order_relaxed) & bit))
                        PS::Log<LogLevel::Verbose>(TEXT("Equipment Shadowveil: {} binding skipped for configured source.\n"), RC::to_generic_string(ShadowveilRules::ActionNames[site]));
                    return true;
                }
            }
        }
        if (!(Observed.fetch_or(256u, std::memory_order_relaxed) & 256u))
            PS::Log<LogLevel::Warning>(TEXT("Equipment Shadowveil: source asset could not be matched; native behavior retained.\n"));
    } catch (...) { Active.store(false, std::memory_order_release); }
    return false;
}
void Melee(safetyhook::Context& c) { if (IsConfiguredEffect(c.rbx, 0)) c.rip = ImageBase + SelectedProfile->sites[0].resume; }
void Ranged(safetyhook::Context& c) { if (IsConfiguredEffect(c.rbx, 1)) c.rip = ImageBase + SelectedProfile->sites[1].resume; }
void Evade(safetyhook::Context& c) { if (IsConfiguredEffect(c.rbx, 2)) c.rip = ImageBase + SelectedProfile->sites[2].resume; }
void Magic(safetyhook::Context& c) { if (IsConfiguredEffect(c.rbx, 3)) c.rip = ImageBase + SelectedProfile->sites[3].resume; }
void Utility(safetyhook::Context& c) { if (IsConfiguredEffect(c.rbx, 4)) c.rip = ImageBase + SelectedProfile->sites[4].resume; }
constexpr safetyhook::MidHookFn Callbacks[]{Melee, Ranged, Evade, Magic, Utility};

void ResetHooks() {
    Active.store(false, std::memory_order_release);
    for (auto& hook : Hooks) hook = {};
}
bool Install(const TCHAR*& failure) {
    failure = TEXT("invalid executable headers");
    ImageBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(ImageBase);
    if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 4096) return false;
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(ImageBase + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) return false;
    failure = TEXT("unsupported executable build");
    SelectedProfile = NativeHookContract::Select(nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage, ShadowveilNative::Profiles);
    if (!SelectedProfile) return false;
    failure = TEXT("native hook or resume bytes differ");
    if (!NativeHookContract::Validate(std::span(reinterpret_cast<const unsigned char*>(ImageBase), SelectedProfile->imageSize), SelectedProfile->sites)) return false;
    failure = TEXT("native hook creation failed");
    for (size_t i = 0; i < Hooks.size(); ++i) {
        if (!(EnabledActions & (1u << i))) continue;
        auto hook = safetyhook::MidHook::create(reinterpret_cast<void*>(ImageBase + SelectedProfile->sites[i].rva),
            Callbacks[i], safetyhook::MidHook::StartDisabled);
        if (!hook) { ResetHooks(); return false; }
        Hooks[i] = std::move(*hook);
    }
    failure = TEXT("native hook activation failed");
    for (auto& hook : Hooks) if (hook && !hook.enable()) { ResetHooks(); return false; }
    Active.store(true, std::memory_order_release);
    return true;
}
}

EquipmentShadowveilStatus InitializeEquipmentShadowveil(const ShadowveilRules::Rules& rules) {
    if (rules.empty()) return {};
    for (const auto& [path, actions] : rules) {
        WearablePaths.emplace_back(RC::to_generic_string(path), actions);
        EnabledActions |= actions;
    }
    const TCHAR* failure{};
    if (Install(failure)) {
        const bool server=SelectedProfile == &ShadowveilNative::Profiles[1];
        PS::Log<LogLevel::Verbose>(TEXT("Equipment Shadowveil ({}): {} wearables; 5 native binding sites validated.\n"),
            server ? TEXT("server") : TEXT("client"), WearablePaths.size());
        return {WearablePaths.size(),true,server};
    }
    PS::Log<LogLevel::Error>(TEXT("Equipment Shadowveil: {}; feature disabled.\n"), failure);
    return {WearablePaths.size(),false,false};
}
void ShutdownEquipmentShadowveil() {
    ResetHooks();
    WearablePaths.clear();
    EnabledActions = 0;
    Observed.store(0, std::memory_order_relaxed);
}
}
