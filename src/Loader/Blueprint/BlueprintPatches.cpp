#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "Loader/DragonWildsBlueprintModLoader.h"
#include "Loader/PlayerGhost.h"
#include "Core/JsonLoadOrderMerge.h"
#include "Loader/Spawn/RuntimeSupport.h"
#include "Utility/Logging.h"
#include "Unreal/AActor.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds {
    void DragonWildsBlueprintModLoader::OnFinalizeLoad(const EEngineLifecyclePhase& phase) {
        if (phase == EEngineLifecyclePhase::PostEngineInit) {
            for (const auto& document : m_pendingBlueprintPatches) {
                try {
                    for (const auto& [identity, changes] : document.items()) {
                        if (!identity.starts_with("/Game/")) {
                            if (identity.contains('/') || identity.contains('.') || !identity.ends_with("_C"))
                                throw std::runtime_error("Blueprint $Patch requires a class name ending _C or a /Game/ class path");
                            const auto key = FName(to_generic_string(identity), FNAME_Add);
                            m_blueprintPatches.emplace_back(key, changes);
                        } else {
                            auto path = identity;
                            if (path.find('.') == std::string::npos) {
                                const auto name = path.substr(path.find_last_of('/') + 1);
                                path += "." + name + "_C";
                            }
                            if (!path.ends_with("_C"))
                                throw std::runtime_error("Blueprint $Patch path must identify a generated class ending _C");
                            const auto key = FName(to_generic_string(path), FNAME_Add);
                            m_blueprintPatches.emplace_back(key, changes);
                            m_pathBlueprintPatches.push_back({{path, nlohmann::json::object()}});
                        }
                    }
                } catch (const std::exception& error) {
                    PS::Log<LogLevel::Error>(STR("Blueprint patch rejected: {}\n"), PS::ToWideSafe(error.what()));
                }
            }
            m_pendingBlueprintPatches.clear();
            size_t rules = 0;
            for (const auto& [key, mods] : m_modsMap) rules += mods.size();
            if (rules) PS::Log<LogLevel::Normal>(STR("Blueprints: {} rules registered.\n"), rules);
            if (!m_blueprintPatches.empty()) PS::RoutineLog("patches", STR("Blueprint $Patch: {} rules registered.\n"), m_blueprintPatches.size());
        } else if (phase == EEngineLifecyclePhase::GameInstanceInit) {
            for (const auto& document : m_pathBlueprintPatches) {
                try { LoadUnsafe(document); }
                catch (const std::exception& error) {
                    PS::Log<LogLevel::Error>(STR("Blueprint path patch failed: {}\n"), PS::ToWideSafe(error.what()));
                }
            }
        }
    }

    void DragonWildsBlueprintModLoader::ApplyDeferredPatches(UObject* object) {
        auto* type = object->GetClassPrivate();
        const auto name = type->GetNamePrivate();
        const auto path = FName(type->GetPathName(), FNAME_Find);
        for (const auto& mod : m_blueprintPatches) {
                const auto key = mod.GetBlueprintName();
                if (key != name && key != path) continue;
                try { ApplyMod(mod, object); }
                catch (const std::exception& error) {
                    PS::Log<LogLevel::Error>(STR("Blueprint $Patch {} failed: {}\n"), key.ToString(), PS::ToWideSafe(error.what()));
                }
        }
    }

    void DragonWildsBlueprintModLoader::ApplyBlueprintVisualEffect(AActor* actor) {
        try {
            auto* type = actor->GetClassPrivate();
            nlohmann::json effect = nlohmann::json::object();
            const auto mergeEffect = [&](const DragonWildsBlueprintMod& mod) {
                const auto field = mod.GetData().find("$VisualEffect");
                if (field == mod.GetData().end()) return;
                if (field->is_null()) effect = nlohmann::json::object();
                else JsonLoadOrderMerge::Apply(effect, *field, true);
            };
            const auto name = type->GetNamePrivate();
            const auto path = FName(type->GetPathName(), FNAME_Find);
            for (const auto key : {name, path}) {
                const auto found = m_modsMap.find(key);
                if (found == m_modsMap.end()) continue;
                for (const auto& mod : found->second) {
                    mergeEffect(mod);
                }
            }
            for (const auto& mod : m_blueprintPatches)
                if (mod.GetBlueprintName() == name || mod.GetBlueprintName() == path) mergeEffect(mod);
            if (effect.empty() || !GhostMaterials::CanRender(actor)) return;
            effect = SpawnRuntime::ValidateVisualEffect(effect);
            auto* playerClass=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.DominionPlayerCharacter"));
            if(playerClass && actor->IsA(playerClass)) { PlayerGhost::Apply(actor,effect);return; }
            const auto signature = effect.dump();
            auto found = m_ghostMaterials.find(signature);
            if (found == m_ghostMaterials.end()) {
                if(m_ghostMaterials.size()>=256)throw std::runtime_error("Blueprint visual style limit reached (256)");
                const auto before = m_ghostRoots.size();
                try {
                    // Use GameInstance ownership; streamed actors may disappear.
                    auto* owner = SpawnRuntime::CallWorldContextGetter(
                        TEXT("/Script/Engine.GameplayStatics:GetGameInstance"),
                        TEXT("/Script/Engine.Default__GameplayStatics"), actor);
                    if (!owner) throw std::runtime_error("Ghost material game instance was unavailable");
                    auto materials = GhostMaterials::Create(owner, effect, m_ghostRoots);
                    for (auto* material : {materials.Overlay, materials.Body}) {
                        if (material && !material->IsRootSet()) {
                            m_ghostRoots.push_back(material);
                            material->SetRootSet();
                        }
                    }
                    found = m_ghostMaterials.emplace(signature, materials).first;
                } catch (...) {
                    while (m_ghostRoots.size() > before) {
                        m_ghostRoots.back()->ClearRootSet();
                        m_ghostRoots.pop_back();
                    }
                    throw;
                }
            }
            GhostMaterials::Apply(actor, found->second, STR("Blueprint ") + type->GetName());
        } catch (const std::exception& error) {
            PS::Log<LogLevel::Warning>(STR("Blueprint Ghost effect failed: {}\n"), PS::ToWideSafe(error.what()));
        }
    }
}
