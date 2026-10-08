#include "Runtime/LogicPakBridge.h"

#include <cstdint>
#include <format>
#include <stdexcept>

#include "Runtime/Storefront.h"
#include "Runtime/BPModLoaderIntegration.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Helper/ActorHelper.h"
#include "Utility/Logging.h"
#include "Unreal/AActor.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/UObjectGlobals.hpp"
#include "Unreal/UnrealFlags.hpp"
#include "Unreal/World.hpp"

using namespace RC;
using namespace RC::Unreal;

namespace PS {
namespace {
std::filesystem::path LegacyLogicModsRoot() {
    auto root = Storefront::CurrentDetection().Executable.parent_path();
    if (!root.empty()) root = root.parent_path();
    if (!root.empty()) root = root.parent_path();
    return root / L"Content" / L"Paks" / L"LogicMods";
}

const TCHAR* WorldLabel(UWorld* world) {
    if (world && world->GetPathName().find(TEXT("FrontEnd")) != StringType::npos)
        return TEXT("FRONTEND");
    return TEXT("WORLD");
}
}

LogicPakBridge::~LogicPakBridge() { Stop(); }

void LogicPakBridge::Start(const std::vector<std::pair<std::filesystem::path, std::string>>& mods) {
    if (m_started || Storefront::IsDedicatedServer()) return;
    if (BPModLoaderIntegration::Active().load(std::memory_order_acquire)) {
        PS::Log<LogLevel::Normal>(STR(
            "[LOGIC-PAK][BP-LOADER] BPModLoaderMod owns enabled RuneSchema ModActors; native duplicate startup suppressed.\n"));
        return;
    }

    const auto legacyRoot = LegacyLogicModsRoot();
    for (const auto& [root, owner] : mods) {
        try {
            for (auto package : LogicPaks::Discover(root, owner, legacyRoot)) {
                if (package.LegacyOwned) {
                    PS::Log<LogLevel::Normal>(STR(
                        "[LOGIC-PAK][DELEGATED][MOD:{}][PACKAGE:{}] Existing Content/Paks/LogicMods install detected; BPModLoaderMod retains startup ownership.\n"),
                        PS::ToWideSafe(package.Owner), PS::ToWideSafe(package.Name));
                    continue;
                }
                m_candidates.push_back({std::move(package)});
            }
        } catch (const std::exception& error) {
            PS::Log<LogLevel::Warning>(STR(
                "[LOGIC-PAK][PARTIAL][MOD:{}] Package discovery skipped: {}. Ordinary PAK mounting remains active.\n"),
                PS::ToWideSafe(owner), PS::ToWideSafe(error.what()));
        }
    }
    if (m_candidates.empty()) return;

    Hook::FCallbackOptions options{};
    options.OwnerModName = TEXT("RuneSchema");
    options.HookName = TEXT("RuneSchemaLogicPakBeginPlay");
    m_beginPlay = Hook::RegisterBeginPlayPostCallback(
        [this](Hook::TCallbackIterationData<void>&, AActor* actor) {
            try { if (actor) Activate(actor->GetWorld()); }
            catch (const std::exception& error) {
                PS::Log<LogLevel::Warning>(STR("[LOGIC-PAK][PARTIAL] BeginPlay startup skipped: {}.\n"),
                    PS::ToWideSafe(error.what()));
            }
        }, options);

    options.HookName = TEXT("RuneSchemaLogicPakStartupFallback");
    m_tick = Hook::RegisterEngineTickPostCallback(
        [this](Hook::TCallbackIterationData<void>&, UEngine*, float, bool) { Tick(); }, options);
    m_started = m_beginPlay != Hook::ERROR_ID && m_tick != Hook::ERROR_ID;
    if (!m_started) {
        PS::Log<LogLevel::Warning>(STR(
            "[LOGIC-PAK][UNAVAILABLE] Client ModActor lifecycle hooks could not be installed; ordinary PAK mounting remains active.\n"));
        Stop();
    }
}

void LogicPakBridge::Stop() noexcept {
    if (m_beginPlay != Hook::ERROR_ID) Hook::UnregisterCallback(m_beginPlay);
    if (m_tick != Hook::ERROR_ID) Hook::UnregisterCallback(m_tick);
    m_beginPlay = m_tick = Hook::ERROR_ID;
    for (auto& candidate : m_candidates) {
        if (!candidate.Owned) continue;
        try {
            if (auto* actor = candidate.Actor.Get())
                DragonWilds::ActorHelper::DestroyActor(static_cast<AActor*>(actor));
        } catch (...) {}
    }
    m_candidates.clear();
    m_world.Reset();
    m_gate.Reset();
    m_started = m_activating = false;
}

bool LogicPakBridge::IsSupportedWorld(UWorld* world) {
    if (!world || world->HasAnyFlags(static_cast<EObjectFlags>(
            RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed | RF_FinishDestroyed)))
        return false;
    const auto path = world->GetPathName();
    return path.find(TEXT("/Game/Maps/")) != StringType::npos;
}

void LogicPakBridge::InvokeOptional(AActor* actor, const TCHAR* name) {
    if (!actor) return;
    auto* function = actor->GetFunctionByNameInChain(name);
    if (!function) return;
    if (function->GetParmsSize() != 0)
        throw std::runtime_error(std::format("{} has parameters", RC::to_string(StringType(name))));
    DragonWilds::ActorHelper::FunctionCall(actor, function).Invoke();
}

AActor* LogicPakBridge::FindExisting(UWorld* world, UClass* type) const {
    AActor* result = nullptr;
    UObjectGlobals::ForEachUObject([&](UObject* object, int32_t, int32_t) -> LoopAction {
        if (!object || !object->IsA(type) || object->HasAnyFlags(static_cast<EObjectFlags>(
                RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed | RF_FinishDestroyed)))
            return LoopAction::Continue;
        auto* actor = static_cast<AActor*>(object);
        if (actor->GetWorld() != world) return LoopAction::Continue;
        result = actor;
        return LoopAction::Break;
    });
    return result;
}

void LogicPakBridge::Activate(UWorld* world) {
    if (m_activating || !IsSupportedWorld(world)) return;
    m_activating = true;
    struct Reset { bool& Value; ~Reset() { Value = false; } } reset{m_activating};
    const auto* worldLabel = WorldLabel(world);

    if (m_world.Get() != world) {
        m_world.Assign(world);
        for (auto& candidate : m_candidates) {
            candidate.Actor.Reset();
            candidate.Owned = false;
            // The front end may appear before a cooked class can resolve.
            // Give each new world one fresh attempt without retrying on every BeginPlay.
            if (!candidate.Class) candidate.ResolutionAttempted = false;
        }
    }

    for (auto& candidate : m_candidates) {
        if (!candidate.ResolutionAttempted) {
            candidate.ResolutionAttempted = true;
            candidate.Class = DragonWilds::ActorHelper::ResolveClass(PS::ToWideSafe(candidate.Package.ActorPath));
            if (!candidate.Class || !DragonWilds::ActorHelper::IsActorClass(candidate.Class)) {
                candidate.Class = nullptr;
                PS::Log<LogLevel::Verbose>(STR(
                    "[LOGIC-PAK][CONTENT-ONLY][MOD:{}][PACKAGE:{}] No default ModActor class resolved; package remains available as ordinary cooked content.\n"),
                    PS::ToWideSafe(candidate.Package.Owner), PS::ToWideSafe(candidate.Package.Name));
            } else {
                PS::Log<LogLevel::Normal>(STR(
                    "[LOGIC-PAK][DISCOVERED][MOD:{}][PACKAGE:{}] ModActor class '{}'.\n"),
                    PS::ToWideSafe(candidate.Package.Owner), PS::ToWideSafe(candidate.Package.Name),
                    PS::ToWideSafe(candidate.Package.ActorPath));
            }
        }
        if (!candidate.Class) continue;

        const auto worldKey = reinterpret_cast<std::uintptr_t>(world);
        if (!m_gate.Claim(worldKey, candidate.Package.Name)) continue;
        try {
            if (auto* existing = FindExisting(world, candidate.Class)) {
                candidate.Actor.Assign(existing);
                PS::Log<LogLevel::Normal>(STR(
                    "[LOGIC-PAK][EXISTING][{}][MOD:{}][PACKAGE:{}] ModActor already exists; duplicate startup suppressed.\n"),
                    worldLabel, PS::ToWideSafe(candidate.Package.Owner), PS::ToWideSafe(candidate.Package.Name));
                continue;
            }
            auto* actor = DragonWilds::ActorHelper::SpawnActor(world, candidate.Class, FVector{}, FRotator{},
                [](AActor* pending) { InvokeOptional(pending, TEXT("PreBeginPlay")); });
            InvokeOptional(actor, TEXT("PostBeginPlay"));
            candidate.Actor.Assign(actor);
            candidate.Owned = true;
            PS::Log<LogLevel::Normal>(STR(
                "[LOGIC-PAK][STARTED][{}][MOD:{}][PACKAGE:{}] ModActor executed once for this world.\n"),
                worldLabel, PS::ToWideSafe(candidate.Package.Owner), PS::ToWideSafe(candidate.Package.Name));
        } catch (const std::exception& error) {
            m_gate.Release(worldKey, candidate.Package.Name);
            PS::Log<LogLevel::Warning>(STR(
                "[LOGIC-PAK][FAILED][{}][MOD:{}][PACKAGE:{}] ModActor startup failed: {}. Other packages continue.\n"),
                worldLabel, PS::ToWideSafe(candidate.Package.Owner), PS::ToWideSafe(candidate.Package.Name),
                PS::ToWideSafe(error.what()));
        }
    }
}

void LogicPakBridge::Tick() {
    if (++m_tickCadence % 30 != 0 || m_world.Get()) return;
    UWorld* world = nullptr;
    UObjectGlobals::ForEachUObject([&](UObject* object, int32_t, int32_t) -> LoopAction {
        if (!object || !object->IsA<AActor>() || object->HasAnyFlags(static_cast<EObjectFlags>(
                RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed | RF_FinishDestroyed)))
            return LoopAction::Continue;
        auto* candidate = static_cast<AActor*>(object)->GetWorld();
        if (!IsSupportedWorld(candidate)) return LoopAction::Continue;
        world = candidate;
        return LoopAction::Break;
    });
    if (world) Activate(world);
}
}
