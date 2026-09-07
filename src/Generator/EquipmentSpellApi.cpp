#include "Generator/EquipmentSpellApi.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Helper/ActorHelper.h"
#include "Unreal/UObjectGlobals.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Utility/Logging.h"
#include "Runtime/HostServices.h"
#include <fstream>
#include <filesystem>
#include <unordered_set>

using namespace RC;
using namespace RC::Unreal;
using nlohmann::json;

namespace PS::EquipmentSpellApi {
    namespace {
        bool Relevant(const std::string& path) {
            return path.starts_with("/Script/Dominion.")
                && (path.contains("Spell") || path.contains("Magic")
                    || path.contains("Equip") || path.contains("Perk")
                    || path.contains("Progress") || path.contains("Unlock")
                    || path.contains("PlayerInventory"));
        }
        json Fields(UStruct* type) {
            json result = json::array();
            for (auto* field : TFieldRange<FProperty>(type, EFieldIterationFlags::None)) {
                json info = {
                    {"name", to_string(field->GetName())},
                    {"type", DragonWilds::PropertyHelper::GetPropertyTypeAsUTF8String(field)},
                    {"offset", field->GetOffset_Internal()},
                    {"elementSize", field->GetElementSize()},
                    {"arrayDim", field->GetArrayDim()},
                    {"flags", static_cast<uint64_t>(field->GetPropertyFlags())}
                };
                if (auto* nested = CastField<FStructProperty>(field))
                    info["struct"] = to_string(nested->GetStruct()->GetPathName());
                if (auto* array = CastField<FArrayProperty>(field)) {
                    info["innerType"] = DragonWilds::PropertyHelper::GetPropertyTypeAsUTF8String(array->GetInner());
                    if (auto* nested = CastField<FStructProperty>(array->GetInner()))
                        info["innerStruct"] = to_string(nested->GetStruct()->GetPathName());
                }
                result.push_back(std::move(info));
            }
            return result;
        }
    }
    void Export() {
        try {
            json report = {
                {"purpose", "Read-only equipment/utility-spell API discovery. No grants or equip changes are performed."},
                {"classes", json::array()}, {"functions", json::array()},
                {"structs", json::array()}
            };
            std::unordered_set<std::string> referenced;
            std::vector<UScriptStruct*> structs;
            UObjectGlobals::ForEachUObject([&](UObject* object, int32_t, int32_t) -> LoopAction {
                if (!object || !object->GetClassPrivate()) return LoopAction::Continue;
                const auto path = to_string(object->GetPathName());
                if (!path.starts_with("/Script/Dominion.")) return LoopAction::Continue;
                if (object->IsA(UScriptStruct::StaticClass()))
                    structs.push_back(static_cast<UScriptStruct*>(object));
                if (!Relevant(path)) return LoopAction::Continue;
                if (object->IsA(UFunction::StaticClass())) {
                    auto* function = static_cast<UFunction*>(object);
                    auto fields = Fields(function);
                    for (const auto& field : fields)
                        for (const auto* key : {"struct", "innerStruct"})
                            if (field.contains(key)) referenced.insert(field.at(key).get<std::string>());
                    report["functions"].push_back({{"path", path},
                        {"flags", function->GetFunctionFlags()},
                        {"parameterSize", function->GetParmsSize()},
                        {"fields", std::move(fields)}});
                } else if (object->IsA(UClass::StaticClass())) {
                    auto* type = static_cast<UClass*>(object);
                    auto fields = Fields(type);
                    for (const auto& field : fields) {
                        for (const auto* key : {"struct", "innerStruct"})
                            if (field.contains(key)) referenced.insert(field.at(key).get<std::string>());
                    }
                    report["classes"].push_back({{"path", path},
                        {"super", type->GetSuperStruct() ? to_string(type->GetSuperStruct()->GetPathName()) : ""},
                        {"fields", std::move(fields)}});
                }
                return LoopAction::Continue;
            });
            std::unordered_set<std::string> emitted;
            bool added;
            do {
              added = false;
              for (auto* type : structs) {
                const auto path = to_string(type->GetPathName());
                if (!emitted.contains(path) && (Relevant(path) || referenced.contains(path))) {
                    auto fields = Fields(type);
                    for (const auto& field : fields)
                        for (const auto* key : {"struct", "innerStruct"})
                            if (field.contains(key)) referenced.insert(field.at(key).get<std::string>());
                    report["structs"].push_back({{"path", path}, {"fields", std::move(fields)}});
                    emitted.insert(path);
                    added = true;
                }
              }
            } while (added);
            constexpr auto surge = TEXT("/Game/Gameplay/UtilityMagic/PerkSpells/Surge/USD_Surge.USD_Surge");
            auto* asset = DragonWilds::ActorHelper::ResolveObject(DragonWilds::ActorHelper::NormalizeObjectPath(surge));
            report["surge"] = {{"requested", to_string(StringType(surge))}, {"resolved", asset != nullptr}};
            if (asset && asset->GetClassPrivate()) {
                report["surge"]["class"] = to_string(asset->GetClassPrivate()->GetPathName());
                report["surge"]["fields"] = Fields(asset->GetClassPrivate());
            }
            const auto folder = std::filesystem::path(PS::HostServices::WorkingDirectory())
                / "Mods" / "RuneSchema" / "diagnostics";
            std::filesystem::create_directories(folder);
            std::ofstream file(folder / "EquipmentSpellAPI.json", std::ios::trunc);
            if (!file) throw std::runtime_error("Could not open EquipmentSpellAPI.json");
            file << report.dump(2);
            file.flush();
            if (!file) throw std::runtime_error("Could not finish EquipmentSpellAPI.json");
            Log<LogLevel::Normal>(STR("Equipment/spell API exported to Mods/RuneSchema/diagnostics/EquipmentSpellAPI.json ({} classes, {} functions).\n"),
                report["classes"].size(), report["functions"].size());
        } catch (const std::exception& error) {
            Log<LogLevel::Error>(STR("Equipment/spell API export failed: {}\n"), ToWideSafe(error.what()));
        }
    }
}
