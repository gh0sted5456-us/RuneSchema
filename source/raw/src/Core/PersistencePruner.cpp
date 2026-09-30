#include "Core/PersistencePruner.h"

#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "Core/AppearanceDefaults.h"
#include "Core/SaveCleanup.h"
#include "Core/SaveRegistrySnapshot.h"
#include "Core/SaveSnapshotRestore.h"
#include "Utility/Config.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Classes/TSoftObjectPtr.h"
#include "SDK/Structs/FSoftObjectPath.h"
#include "Utility/Logging.h"
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
}

void PersistencePruner::PrepareForStartup() noexcept
{
    m_cleanupDeferredReported = false;
}

void PersistencePruner::PruneBeforeCharacterLoad(
    UObject* context, UFunction* function, void* parameters)
{
    if (s_cleanupConsumedForProcess.load(std::memory_order_acquire)
        || !context || !function || !parameters) return;
    const auto registry = SaveCleanup::ReadRegistry();
    if (!registry || !registry->Ready()) {
        if (!m_cleanupDeferredReported) {
            m_cleanupDeferredReported = true;
            Log<LogLevel::Warning>(STR(
                "[PERSISTENCE-PRUNER][DEFERRED] Waiting for complete applicable live registries. No persistence ID or character field was changed.\n"));
        }
        return;
    }

    try {
        FStrProperty* jsonProperty = nullptr;
        void* jsonAddress = nullptr;
        nlohmann::json source;
        for (auto* field : TFieldRange<FProperty>(
            function, EFieldIterationFlags::Default)) {
            if (!field->HasAnyPropertyFlags(CPF_Parm)
                || field->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm)
                || field->GetArrayDim() != 1 || field->GetOffset_Internal() < 0
                || field->GetOffset_Internal() + field->GetElementSize()
                    > function->GetParmsSize()) continue;
            auto* stringField = CastField<FStrProperty>(field);
            if (!stringField) continue;
            auto* address = stringField->ContainerPtrToValuePtr<void>(parameters);
            const auto value = stringField->GetPropertyValue(address);
            if (value.GetCharArray().Num() <= 1) continue;
            const auto utf8 = RC::to_string(RC::StringType(*value));
            if (utf8.find("\"GameProgress\"") == std::string::npos) continue;
            auto parsed = nlohmann::json::parse(utf8, nullptr, true, true);
            if (SaveCleanup::ClassifyCharacterDocument(parsed)
                != SaveCleanup::CharacterDocumentKind::Gameplay) continue;
            if (jsonProperty) throw std::runtime_error(
                "character load exposed more than one gameplay JSON parameter");
            jsonProperty = stringField;
            jsonAddress = address;
            source = std::move(parsed);
        }
        if (!jsonProperty) return;
        const auto characterId = source.contains("meta_data")
            && source.at("meta_data").is_object()
            ? source.at("meta_data").value("char_guid", std::string{})
            : std::string{};
        if (characterId.empty()) return;

        auto* config = PSConfig::Get();
        const bool resetOnce = config->ConsumeDefaultResetOnce();
        const auto& defaultSettings = config->GetSettings().defaults;
        std::vector<std::string> restoredSections;
        std::optional<nlohmann::json> restorationAppearanceDefaults;
        if (defaultSettings.restoration.enabled || resetOnce) {
            const auto snapshot = AppearanceDefaults::Load(true);
            if (!snapshot.External) {
                Log<LogLevel::Warning>(STR(
                    "[SNAPSHOT-RESTORE][SKIPPED] {}. Mandatory orphan pruning will continue without restoring save sections.\n"),
                    ToWideSafe(snapshot.Notice.c_str()));
            } else {
                try {
                    auto restored = SaveSnapshotRestore::Apply(
                        source, snapshot.Document, defaultSettings.restoration,
                        resetOnce ? SaveSnapshotRestore::Mode::ReplaceSelected
                            : SaveSnapshotRestore::Mode::MergeBaseline);
                    source = std::move(restored.Save);
                    restoredSections = std::move(restored.Sections);
                    if (defaultSettings.restoration.appearance)
                        restorationAppearanceDefaults = snapshot.Document;
                } catch (const std::exception& error) {
                    Log<LogLevel::Error>(STR(
                        "[SNAPSHOT-RESTORE][SKIPPED] No save section was restored: {}. Mandatory orphan pruning will continue.\n"),
                        ToWideSafe(error.what()));
                }
            }
        }

        ReportValidatedSavedItems(source, *registry);
        if (s_cleanupConsumedForProcess.exchange(
                true, std::memory_order_acq_rel)) return;

        SaveCleanup::Preview cleaned{source};
        cleaned = SaveCleanup::Plan(
            cleaned.Save, {}, false, registry.get(), false, true, true);
        auto defaults = AppearanceDefaults::Load(
            defaultSettings.appearanceOverrideEnabled);
        if (restorationAppearanceDefaults) {
            defaults.Document = *restorationAppearanceDefaults;
            defaults.External = true;
            defaults.Notice.clear();
        }
        if (!defaults.Notice.empty()) Log<LogLevel::Warning>(STR(
            "[PERSISTENCE-PRUNER][DEFAULT-OVERRIDE-IGNORED] {}. Using the DLL's built-in male/A defaults.\n"),
            ToWideSafe(defaults.Notice.c_str()));
        const auto repairWith = [&](const nlohmann::json& document) {
            return SaveCleanup::RepairInvalidAppearance(
                cleaned.Save, document, ValidAppearanceReference);
        };
        SaveCleanup::Preview appearance{cleaned.Save};
        bool usedExternalDefaults = defaults.External;
        try {
            appearance = repairWith(defaults.Document);
        } catch (const std::exception& error) {
            if (!defaults.External) throw;
            usedExternalDefaults = false;
            Log<LogLevel::Warning>(STR(
                "[PERSISTENCE-PRUNER][DEFAULT-OVERRIDE-IGNORED] settings/defaults/Default.json contains an unavailable live table or row: {}. Using the DLL's built-in male/A defaults.\n"),
                ToWideSafe(error.what()));
            appearance = repairWith(AppearanceDefaults::BuiltIn());
        }
        for (auto& row : appearance.Removed)
            cleaned.Removed.push_back(std::move(row));
        cleaned.Save = std::move(appearance.Save);
        if (cleaned.Removed.empty() && restoredSections.empty()) {
            Log<LogLevel::Verbose>(STR(
                "[PERSISTENCE-PRUNER][CHECKED] Every applicable persistence ID resolved; no persistence data was changed.\n"));
            return;
        }

        const auto serialized = cleaned.Save.dump();
        const FString replacement(RC::to_generic_string(serialized).c_str());
        jsonProperty->SetPropertyValue(jsonAddress, replacement);
        const auto verified = RC::to_string(RC::StringType(
            *jsonProperty->GetPropertyValue(jsonAddress)));
        if (verified != serialized) throw std::runtime_error(
            "clean character JSON did not survive reflected writeback");

        if (!restoredSections.empty()) {
            std::string restoredSummary;
            for (const auto& section : restoredSections) {
                if (!restoredSummary.empty()) restoredSummary += ", ";
                restoredSummary += section;
            }
            Log<LogLevel::Warning>(STR(
                "[SNAPSHOT-RESTORE][APPLIED] {} selected section(s) from settings/defaults/Default.json: {}. Active character identity metadata was preserved; supported item, recipe, quest, and journal persistence IDs were checked against live registries.\n"),
                resetOnce ? STR("Reset") : STR("Merged baseline into"),
                ToWideSafe(restoredSummary.c_str()));
        }

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
                Log<LogLevel::Warning>(STR(
                    "[PERSISTENCE-PRUNER][ORPHAN-REMOVED] {} persistence ID '{}' did not resolve in its complete applicable live registry.\n"),
                    ToWideSafe(kind.c_str()), ToWideSafe(id.c_str()));
            }
        }
        std::string summary;
        for (const auto& [kind, count] : counts) {
            if (!summary.empty()) summary += ", ";
            summary += kind + "=" + std::to_string(count);
        }
        if (orphanCount) Log<LogLevel::Warning>(STR(
            "[PERSISTENCE-PRUNER][ORPHANS-REMOVED] Removed {} unresolved persistence reference(s) ({}). Resolved IDs were retained. Pruning will not run again until game restart.\n"),
            orphanCount, ToWideSafe(summary.c_str()));
        if (appearanceCount) Log<LogLevel::Warning>(STR(
            "[PERSISTENCE-PRUNER][APPEARANCE-REPAIRED] Replaced {} invalid appearance handle(s) using {} defaults.\n"),
            appearanceCount, usedExternalDefaults
                ? STR("settings/defaults/Default.json") : STR("built-in male/A"));
    } catch (const std::exception& error) {
        Log<LogLevel::Error>(STR(
            "[PERSISTENCE-PRUNER][UNCHANGED] Character JSON was not modified: {}.\n"),
            ToWideSafe(error.what()));
    }
}
}
