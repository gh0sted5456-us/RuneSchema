#include "Loader/DragonWildsEquipmentLoader.h"
#include "Loader/SurgeNativeContract.h"
#include "Loader/EquipmentShadowveil.h"
#include "SDK/Helper/PropertyHelper.h"
#include "Unreal/UObject.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Helpers/Casting.hpp"
#include "Utility/JsonHelpers.h"
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
std::array<safetyhook::MidHook, 8> Hooks;
std::vector<RC::StringType> LegPaths;
std::atomic<bool> Active{false};
std::atomic<unsigned> Observed{0};
uintptr_t ImageBase{};
const NativeHookContract::Profile<SurgeNative::Site>* SelectedProfile{};

UObject* ObjectRef(UObject* owner, const TCHAR* name) {
    if (!owner || !owner->GetClassPrivate()) return nullptr;
    auto* field = CastField<FObjectPropertyBase>(PropertyHelper::GetPropertyByName(owner->GetClassPrivate(), name));
    if (!field || field->GetElementSize() != sizeof(UObject*)) return nullptr;
    return *field->ContainerPtrToValuePtr<UObject*>(owner);
}

bool WearsSurgeLegs(uintptr_t address, unsigned site) noexcept {
    if (!Active.load(std::memory_order_acquire) || !address) return false;
    try {
        auto* component = reinterpret_cast<UObject*>(address);
        auto* player = ObjectRef(component, TEXT("PlayerCharacter"));
        if (!player) return false;
        auto* equipment = ObjectRef(player, TEXT("PlayerEquipmentComponent"));
        if (!equipment) return false;
        auto* legs = ObjectRef(equipment, TEXT("CurrentLegsWearable"));
        if (!legs) return false;
        const auto path = legs->GetPathName();
        for (const auto& allowed : LegPaths) {
            if (path != allowed) continue;
            const unsigned bit = 1u << site;
            if (!(Observed.fetch_or(bit, std::memory_order_relaxed) & bit))
                PS::Log<LogLevel::Verbose>(TEXT("Equipment Surge: native path {} observed for configured legs.\n"), site);
            return true;
        }
    } catch (...) {
        Active.store(false, std::memory_order_release);
        // Do not unwind through native game code.
    }
    return false;
}
void Select(safetyhook::Context& c) { if (WearsSurgeLegs(c.rbx, 0)) c.rcx = (c.rcx & ~uintptr_t{255}) | 1; }
void ValidateRequest(safetyhook::Context& c) { if (WearsSurgeLegs(c.rcx - 0xc0, 1)) c.rip = ImageBase + SelectedProfile->sites[1].resume; }
void StaminaGate(safetyhook::Context& c) { if (WearsSurgeLegs(c.rbx, 2)) c.rip = ImageBase + SelectedProfile->sites[2].resume; }
void StaminaQuery(safetyhook::Context& c) { if (WearsSurgeLegs(c.rbx, 3)) c.rip = ImageBase + SelectedProfile->sites[3].resume; }
void StaminaCost(safetyhook::Context& c) { if (WearsSurgeLegs(c.rdi, 4)) c.rip = ImageBase + SelectedProfile->sites[4].resume; }
void Animation(safetyhook::Context& c) { if (WearsSurgeLegs(c.rbx, 5)) c.rip = ImageBase + SelectedProfile->sites[5].resume; }
void ServerCharges(safetyhook::Context& c) { if (WearsSurgeLegs(c.rdi - 0xc0, 6)) c.rip = ImageBase + SelectedProfile->sites[6].resume; }
void ClientCharges(safetyhook::Context& c) { if (WearsSurgeLegs(c.rbx - 0xc0, 7)) c.rip = ImageBase + SelectedProfile->sites[7].resume; }
constexpr safetyhook::MidHookFn Callbacks[]{Select, ValidateRequest, StaminaGate, StaminaQuery, StaminaCost, Animation, ServerCharges, ClientCharges};

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
    SelectedProfile = NativeHookContract::Select(nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage, SurgeNative::Profiles);
    if (!SelectedProfile) return false;
    failure = TEXT("native hook or resume bytes differ");
    if (!NativeHookContract::Validate(std::span(reinterpret_cast<const unsigned char*>(ImageBase), SelectedProfile->imageSize), SelectedProfile->sites)) return false;
    failure = TEXT("native hook creation failed");
    for (size_t i = 0; i < Hooks.size(); ++i) {
        auto hook = safetyhook::MidHook::create(reinterpret_cast<void*>(ImageBase + SelectedProfile->sites[i].rva),
            Callbacks[i], safetyhook::MidHook::StartDisabled);
        if (!hook) { ResetHooks(); return false; }
        Hooks[i] = std::move(*hook);
    }
    failure = TEXT("native hook activation failed");
    for (auto& hook : Hooks) if (!hook.enable()) { ResetHooks(); return false; }
    Active.store(true, std::memory_order_release);
    return true;
}
}

DragonWildsEquipmentLoader::DragonWildsEquipmentLoader() : DragonWildsModLoaderBase("equipment") {
    SetDisplayName(TEXT("Equipment Behaviors"));
}
DragonWildsEquipmentLoader::~DragonWildsEquipmentLoader() { ShutdownEquipmentShadowveil(); ResetHooks(); LegPaths.clear(); Observed.store(0, std::memory_order_relaxed); }
bool DragonWildsEquipmentLoader::CanInitialize(const EEngineLifecyclePhase& phase) {
    return phase == EEngineLifecyclePhase::PostEngineInit;
}
void DragonWildsEquipmentLoader::OnLoad(const std::filesystem::path& path, const RC::StringType&, const EEngineLifecyclePhase& phase) {
    if (phase != EEngineLifecyclePhase::PostEngineInit || m_finalized) return;
    PS::JsonHelpers::ParseJsonFilesInPath(path, [&](const nlohmann::json& data) { EquipmentRules::Merge(m_rules, data); });
}
void DragonWildsEquipmentLoader::OnFinalizeLoad(const EEngineLifecyclePhase& phase) {
    if (phase != EEngineLifecyclePhase::PostEngineInit || m_finalized) return;
    m_finalized = true;
    const auto shadowveil=InitializeEquipmentShadowveil(m_rules.shadowveil);
    m_rules.shadowveil.clear();
    const auto surgeCount=m_rules.surge.size();
    for (const auto& [path, enabled] : m_rules.surge) LegPaths.push_back(RC::to_generic_string(path));
    m_rules.surge.clear();
    bool surgeEnabled=false;
    bool surgeServer=false;
    if (!LegPaths.empty()) {
        const TCHAR* failure{};
        if (Install(failure)) {
            surgeEnabled=true;
            surgeServer=SelectedProfile == &SurgeNative::Profiles[1];
            PS::Log<LogLevel::Verbose>(TEXT("Equipment Surge ({}): {} leg items; 8 native sites validated.\n"),
                surgeServer ? TEXT("server") : TEXT("client"), LegPaths.size());
        } else PS::Log<LogLevel::Error>(TEXT("Equipment Surge: {}; feature disabled.\n"), failure);
    }
    if (shadowveil.enabled || surgeEnabled)
        PS::Log<LogLevel::Normal>(TEXT("Equipment ({}): {} Shadowveil wearables, {} Surge leg items enabled.\n"),
            (shadowveil.enabled ? shadowveil.server : surgeServer) ? TEXT("server") : TEXT("client"),
            shadowveil.enabled ? shadowveil.wearables : 0, surgeEnabled ? surgeCount : 0);
}
void DragonWildsEquipmentLoader::OnAutoReload(const RC::StringType&, const std::filesystem::path&) {
    PS::Log<LogLevel::Warning>(TEXT("Equipment behavior rule changes require restarting the game.\n"));
}
}
