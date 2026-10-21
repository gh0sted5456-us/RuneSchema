#include "Core/PersistencePruner.h"
#include "Core/CharacterLoadAudit.h"

#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "Core/AppearanceDefaults.h"
#include "Core/ConfigFiles.h"
#include "Core/SaveCleanup.h"
#include "Core/SaveRegistrySnapshot.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Classes/TSoftObjectPtr.h"
#include "SDK/Structs/FSoftObjectPath.h"
#include "SDK/Helper/ReflectedContract.h"
#include "Utility/Logging.h"
#include "Utility/Config.h"
#include "Runtime/HostServices.h"
#include "Runtime/Storefront.h"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/Engine/UDataTable.hpp"
#include "Unreal/NameTypes.hpp"
#include "Unreal/UFunctionStructs.hpp"
#include "Unreal/UObject.hpp"

using namespace RC;
using namespace RC::Unreal;

namespace PS {
namespace {
bool ValidAppearanceReference(const std::string& tablePath,
    const std::string& rowName)
{
    if (tablePath.empty() || rowName.empty()) return false;
    const auto path = RC::to_generic_string(tablePath);
    auto* object = UECustom::UObjectGlobals::StaticFindObject<UObject*>(
        nullptr, nullptr, path.c_str(), false);
    if (!object) {
        UECustom::TSoftObjectPtr<UObject> soft{UECustom::FSoftObjectPath(path)};
        object = UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
    }
    if (!object || !object->IsA(UDataTable::StaticClass())) return false;
    const FName row(RC::to_generic_string(rowName), FNAME_Find);
    return row != NAME_None
        && static_cast<UDataTable*>(object)->FindRowUnchecked(row);
}

void ReportValidatedSavedItems(const nlohmann::json& source,
    const SaveCleanup::RegistrySnapshot& registry)
{
    if (!source.contains("GameProgress")
        || !source.at("GameProgress").is_object()) return;
    const auto& game = source.at("GameProgress");
    std::set<std::pair<std::string, std::string>> reported;
    for (const auto* section : {"Inventory", "PersonalInventory", "Loadout"}) {
        if (!game.contains(section) || !game.at(section).is_object()) continue;
        for (const auto& entry : game.at(section).items()) {
            const auto& row = entry.value();
            if (!row.is_object()) continue;
            const auto id = row.value("ItemData", std::string{});
            if (id.empty() || !registry.Items.contains(id)
                || !reported.emplace(section, id).second) continue;
            Log<LogLevel::Verbose>(STR(
                "[PERSISTENCE-PRUNER][RESOLVED] Retaining {} persistence ID '{}' because it resolves in the applicable live registry. Origin is irrelevant (native, loaded pak, or RuneSchema loader).\n"),
                ToWideSafe(section), ToWideSafe(id.c_str()));
        }
    }
}

std::optional<nlohmann::json> ReadExternalBaseline()
{
    constexpr std::size_t MaximumDefaultBytes = 8 * 1024 * 1024;
    const auto path = HostServices::SettingsDirectory()
        / "defaults" / "default.json";
    try {
        auto document = nlohmann::json::parse(
            ConfigFiles::Read(path, MaximumDefaultBytes), nullptr, true, true);
        if (SaveCleanup::ClassifyCharacterDocument(document)
            == SaveCleanup::CharacterDocumentKind::Unsupported)
            throw std::runtime_error(
                "expected a native gameplay save or profile-only character document");
        Log<LogLevel::Normal>(STR(
            "[DEFAULT-RECOVERY][LOADED] Read the exact baseline file settings/defaults/default.json. No directory scan was performed.\n"));
        return document;
    } catch (const std::exception& error) {
        Log<LogLevel::Warning>(STR(
            "[DEFAULT-RECOVERY][FALLBACK] settings/defaults/default.json was unavailable or invalid: {}. Only the baked appearance profile remains available.\n"),
            ToWideSafe(error.what()));
        return std::nullopt;
    }
}
}

void PersistencePruner::PrepareForStartup() noexcept
{
    m_cleanupDeferredReported = false;
}

bool PersistencePruner::PruneBeforeCharacterLoad(
    UObject* context, UFunction* function, void* parameters)
{
    if (!context || !function || !parameters) return false;

    // Optional cleanup must not inspect/copy a character payload while its
    // authority is unavailable. Leave the original native event untouched.
    const auto registry = SaveCleanup::ReadRegistry();
    if (!registry || !registry->Ready()) {
        if(!m_cleanupDeferredReported){m_cleanupDeferredReported=true;
            CodedLog<LogLevel::Warning>(Diagnostics::Id::PersistenceUncertified,STR(
                "[PERSISTENCE-PRUNER][DEFERRED] Awaiting certified registration; pending={} healthy={}. Non-blocking: native character loading continues; save unchanged.\n"),
                RegistryEnrollment::PendingCount(),RegistryEnrollment::Healthy());}
        return false;
    }
    m_cleanupDeferredReported=false;

    try {
        DragonWilds::ReflectedContract::Frame(function);
        FStrProperty* jsonProperty = nullptr;
        void* jsonAddress = nullptr;
        for (auto* field : TFieldRange<FProperty>(
            function, EFieldIterationFlags::IncludeDeprecated)) {
            // A FString passed by const/reference is still an input. Only
            // genuine output-only fields are excluded, not reference inputs.
            if (!DragonWilds::ReflectedContract::Input(field,function)) continue;
            auto* stringField = CastField<FStrProperty>(field);
            if (!stringField || stringField->GetElementSize()!=sizeof(FString)) continue;
            auto* address = stringField->ContainerPtrToValuePtr<void>(parameters);
            const auto value = stringField->GetPropertyValue(address);
            const auto& chars=value.GetCharArray();
            if(chars.Num()<=1)continue;
            if(chars.Num()>32*1024*1024||!chars.GetData()||chars.GetData()[chars.Num()-1]!=0)
                throw std::runtime_error("character JSON string bounds invalid");
            const auto utf8 = RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1));
            if (utf8.find("\"GameProgress\"") == std::string::npos) continue;
            auto parsed = nlohmann::json::parse(utf8, nullptr, true, true);
            if (SaveCleanup::ClassifyCharacterDocument(parsed)
                != SaveCleanup::CharacterDocumentKind::Gameplay) continue;
            if (jsonProperty) throw std::runtime_error(
                "character load exposed more than one gameplay JSON parameter");
            jsonProperty = stringField;
            jsonAddress = address;
        }
        if (!jsonProperty) return false;
        auto value = jsonProperty->GetPropertyValue(jsonAddress);
        const auto pruned=PruneCharacterJson(value);
        jsonProperty->SetPropertyValue(jsonAddress, value);
        CharacterLoadAudit::Pruned(pruned);
        if(pruned)Log<LogLevel::Normal>(STR("[PERSISTENCE-PRUNER][PRUNED-PAYLOAD] load={} registry_confirmed_orphan_records={} native argument writeback verified; persisted save removal is not yet verified.\n"),CharacterLoadAudit::Token(),pruned);
        return true;
    } catch (const std::exception& error) {
        Log<LogLevel::Error>(STR(
            "[PERSISTENCE-PRUNER][UNCHANGED] Character JSON was not modified: {}.\n"),
            ToWideSafe(error.what()));
    }
    return false;
}

std::size_t PersistencePruner::PruneCharacterJson(FString& characterJson)
{
    if (characterJson.GetCharArray().Num() <= 1) return 0;

    const auto registry = SaveCleanup::ReadRegistry();
    if (!registry || !registry->Ready()) {
        if (!m_cleanupDeferredReported) {
            m_cleanupDeferredReported = true;
            CodedLog<LogLevel::Warning>(Diagnostics::Id::PersistenceUncertified,STR(
                "[PERSISTENCE-PRUNER][DEFERRED] Cleanup skipped: complete applicable live registries are unavailable. Non-blocking: native character loading continues; no persistence ID or character field was changed.\n"));
        }
        return 0;
    }

    bool applied=false;
    std::size_t committedRemovals=0;
    try {
        const auto utf8 = RC::to_string(RC::StringType(*characterJson));
        if (utf8.find("\"GameProgress\"") == std::string::npos) return 0;
        auto source = nlohmann::json::parse(utf8, nullptr, true, true);
        if (SaveCleanup::ClassifyCharacterDocument(source)
            != SaveCleanup::CharacterDocumentKind::Gameplay) return 0;
        const auto characterId = source.contains("meta_data")
            && source.at("meta_data").is_object()
            ? source.at("meta_data").value("char_guid", std::string{})
            : std::string{};
        if (characterId.empty()) return 0;

        ReportValidatedSavedItems(source, *registry);
        // Every incoming gameplay payload is checked. One failed attempt or
        // a different character must not consume cleanup for the whole process.
        // Plan is idempotent and uses only the certified registry snapshot;
        // this does not rescan assets or mutate registries on world transitions.

        SaveCleanup::Preview cleaned{source};
        cleaned = SaveCleanup::Plan(
            cleaned.Save, {}, false, registry.get(), false, true, true);
        nlohmann::json restored = nlohmann::json::array();
        const auto recovery = PSConfig::Get()->GetSettings().defaultRecovery;
        std::optional<nlohmann::json> external;
        if (recovery.enabled && recovery.useExternalDefault)
            external = ReadExternalBaseline();

        if (recovery.enabled && recovery.appearance) {
            const auto repair = [&](const nlohmann::json& document) {
                return SaveCleanup::RepairInvalidAppearance(cleaned.Save,
                    SaveCleanup::AppearanceProfile(document),
                    ValidAppearanceReference);
            };
            SaveCleanup::Preview appearance{cleaned.Save};
            bool usedExternal = false;
            if (external) {
                try {
                    appearance = repair(*external);
                    usedExternal = true;
                } catch (const std::exception& error) {
                    Log<LogLevel::Warning>(STR(
                        "[DEFAULT-RECOVERY][APPEARANCE-FALLBACK] External appearance baseline was rejected: {}. Using the DLL's baked profile.\n"),
                        ToWideSafe(error.what()));
                }
            }
            if (!usedExternal) appearance = repair(AppearanceDefaults::BuiltIn());
            for (auto& row : appearance.Removed) {
                restored.push_back(row);
                cleaned.Removed.push_back(std::move(row));
            }
            cleaned.Save = std::move(appearance.Save);
        }

        if (recovery.enabled && external
            && SaveCleanup::ClassifyCharacterDocument(*external)
                == SaveCleanup::CharacterDocumentKind::Gameplay
            && (recovery.items || recovery.quests || recovery.progress)) {
            try {
                const auto merged = SaveCleanup::MergeBaseline(cleaned.Save,
                    *external, *registry, {
                        .Items = recovery.items,
                        .Quests = recovery.quests,
                        .Progress = recovery.progress,
                    });
                cleaned.Save = merged.Save;
                for (const auto& row : merged.Restored)
                    restored.push_back(row);
            } catch (const std::exception& error) {
                Log<LogLevel::Warning>(STR(
                    "[DEFAULT-RECOVERY][MERGE-SKIPPED] External baseline sections were not merged: {}. Mandatory orphan pruning remains active.\n"),
                    ToWideSafe(error.what()));
            }
        }

        if (cleaned.Removed.empty() && restored.empty()) {
            Log<LogLevel::Verbose>(STR(
                "[PERSISTENCE-PRUNER][CHECKED] No confirmed orphan IDs; pending declarations={} are protected individually, not a cleanup barrier. No persistence data was changed.\n"),RegistryEnrollment::PendingCount());
            return 0;
        }

        const auto serialized = cleaned.Save.dump();
        FString replacement(RC::to_generic_string(serialized).c_str());
        const auto verified = RC::to_string(RC::StringType(*replacement));
        if (verified != serialized) throw std::runtime_error(
            "clean character JSON did not survive native writeback");
        std::size_t removed=0;for(const auto& row:cleaned.Removed)if(row.value("Kind",std::string{})!="Appearance")++removed;
        characterJson=std::move(replacement);applied=true;committedRemovals=removed;

        std::map<std::string, std::size_t> counts;
        std::size_t orphanCount = 0;
        std::size_t appearanceCount = 0;
        for (const auto& row : cleaned.Removed) {
            const auto kind = row.value("Kind", std::string("Unknown"));
            if (kind == "Appearance") ++appearanceCount;
            else {
                ++orphanCount;
                ++counts[kind];
                const auto id = row.value("Id", std::string("<unknown>"));
                Log<LogLevel::Verbose>(STR(
                    "[PERSISTENCE-PRUNER][ORPHAN-REMOVED] {} persistence ID '{}' did not resolve in its complete applicable live registry.\n"),
                    ToWideSafe(kind.c_str()), ToWideSafe(id.c_str()));
            }
        }
        std::string summary;
        for (const auto& [kind, count] : counts) {
            if (!summary.empty()) summary += ", ";
            summary += kind + "=" + std::to_string(count);
        }
        PS::ConsolidatedLog<RC::LogLevel::Normal>(STR(
            "[RuneSchema] [summary] Save cleanup | missing character IDs removed={} ({}) | appearance repaired={} | baseline entries restored={} | valid data kept; scope=load-payload transform, not a persisted-save receipt.\n"),
            orphanCount, ToWideSafe(summary.c_str()), appearanceCount, restored.size());
    } catch (const std::exception& error) {
        if(applied)Log<LogLevel::Error>(STR("[PERSISTENCE-PRUNER][APPLIED] Cleanup committed, but its diagnostic report failed: {}.\n"),ToWideSafe(error.what()));
        else Log<LogLevel::Error>(STR(
            "[PERSISTENCE-PRUNER][UNCHANGED] Character JSON was not modified: {}.\n"),
            ToWideSafe(error.what()));
    }
    return committedRemovals;
}
}
