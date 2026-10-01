#include "Core/PersistencePruner.h"

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
#include "Utility/Logging.h"
#include "Utility/Config.h"
#include "Runtime/HostServices.h"
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

void PersistencePruner::PruneBeforeCharacterLoad(
    UObject* context, UFunction* function, void* parameters)
{
    if (s_cleanupConsumedForProcess.load(std::memory_order_acquire)
        || !context || !function || !parameters) return;

    try {
        FStrProperty* jsonProperty = nullptr;
        void* jsonAddress = nullptr;
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
        }
        if (!jsonProperty) return;
        auto value = jsonProperty->GetPropertyValue(jsonAddress);
        PruneCharacterJson(value);
        jsonProperty->SetPropertyValue(jsonAddress, value);
    } catch (const std::exception& error) {
        Log<LogLevel::Error>(STR(
            "[PERSISTENCE-PRUNER][UNCHANGED] Character JSON was not modified: {}.\n"),
            ToWideSafe(error.what()));
    }
}

void PersistencePruner::PruneCharacterJson(FString& characterJson)
{
    if (s_cleanupConsumedForProcess.load(std::memory_order_acquire)
        || characterJson.GetCharArray().Num() <= 1) return;

    try {
        const auto utf8 = RC::to_string(RC::StringType(*characterJson));
        if (utf8.find("\"GameProgress\"") == std::string::npos) return;
        auto source = nlohmann::json::parse(utf8, nullptr, true, true);
        if (SaveCleanup::ClassifyCharacterDocument(source)
            != SaveCleanup::CharacterDocumentKind::Gameplay) return;
        const auto characterId = source.contains("meta_data")
            && source.at("meta_data").is_object()
            ? source.at("meta_data").value("char_guid", std::string{})
            : std::string{};
        if (characterId.empty()) return;

        const auto registry = SaveCleanup::ReadRegistry();
        if (!registry || !registry->Ready()) {
            if (!m_cleanupDeferredReported) {
                m_cleanupDeferredReported = true;
                Log<LogLevel::Warning>(STR(
                    "[PERSISTENCE-PRUNER][DEFERRED] Waiting for complete applicable live registries. No persistence ID or character field was changed.\n"));
            }
            return;
        }

        ReportValidatedSavedItems(source, *registry);
        if (s_cleanupConsumedForProcess.exchange(
                true, std::memory_order_acq_rel)) return;

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
                "[PERSISTENCE-PRUNER][CHECKED] Every applicable persistence ID resolved; no persistence data was changed.\n"));
            return;
        }

        const auto serialized = cleaned.Save.dump();
        characterJson = FString(RC::to_generic_string(serialized).c_str());
        const auto verified = RC::to_string(RC::StringType(*characterJson));
        if (verified != serialized) throw std::runtime_error(
            "clean character JSON did not survive native writeback");

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
            "[DEFAULT-RECOVERY][APPEARANCE-REPAIRED] Replaced {} missing or invalid appearance handle(s) from the selected validated baseline.\n"),
            appearanceCount);
        if (!restored.empty()) Log<LogLevel::Warning>(STR(
            "[DEFAULT-RECOVERY][MERGED] Added {} missing baseline record(s). Existing live values and progress were preserved.\n"),
            restored.size());
    } catch (const std::exception& error) {
        Log<LogLevel::Error>(STR(
            "[PERSISTENCE-PRUNER][UNCHANGED] Character JSON was not modified: {}.\n"),
            ToWideSafe(error.what()));
    }
}
}
