#include "Loader/PlayerGhost.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <cctype>
#include <format>
#include <fstream>
#include <sstream>
#include <vector>
#include "Unreal/AActor.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/Property/FEnumProperty.hpp"
#include "Unreal/Property/FTextProperty.hpp"
#include "Helpers/Casting.hpp"
#include "Unreal/Engine/UDataTable.hpp"
#include "Unreal/Transform.hpp"
#include "Unreal/UFunctionStructs.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/UObjectGlobals.hpp"
#include "Unreal/World.hpp"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Classes/Custom/UWorldPartitionRuntimeLevelStreamingCell.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Classes/TSoftObjectPtr.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Structs/Custom/FScriptArrayHelper.h"
#include "SDK/Structs/FSoftObjectPath.h"
#include "Utility/JsonHelpers.h"
#include "Utility/Config.h"
#include "Utility/Logging.h"
#include "Loader/DragonWildsSpawnLoader.h"
#include "Loader/DragonWildsBlueprintModLoader.h"
#include "Loader/PlayerAttributeNames.h"
#include "Core/JsonPatchDirective.h"
#include "Core/JsonLoadOrderMerge.h"
#include "Runtime/HostServices.h"

using namespace RC;
using namespace RC::Unreal;

namespace fs = std::filesystem;

#include "Loader/Spawn/RuntimeSupport.h"
using namespace DragonWilds::SpawnRuntime;
namespace {
    const std::unordered_map<std::string, std::pair<std::string, std::string>>& AppearanceFields()
    {
        static const std::unordered_map<std::string, std::pair<std::string, std::string>> fields{
            {"BodyType", {"BodyType", "/Game/Gameplay/Character/Player/Customization/DT_Customization_BodyType.DT_Customization_BodyType"}},
            {"FaceType", {"FaceType", "/Game/Gameplay/Character/Player/Customization/DT_Customization_FaceType.DT_Customization_FaceType"}},
            {"Head", {"FaceType", "/Game/Gameplay/Character/Player/Customization/DT_Customization_FaceType.DT_Customization_FaceType"}},
            {"HairPreset", {"HairPreset", "/Game/Gameplay/Character/Player/Customization/DT_Customization_HairPresets.DT_Customization_HairPresets"}},
            {"HairStyle", {"HairPreset", "/Game/Gameplay/Character/Player/Customization/DT_Customization_HairPresets.DT_Customization_HairPresets"}},
            {"FacialHairPreset", {"FacialHairPreset", "/Game/Gameplay/Character/Player/Customization/DT_Customization_FacialHairPresets.DT_Customization_FacialHairPresets"}},
            {"BeardStyle", {"FacialHairPreset", "/Game/Gameplay/Character/Player/Customization/DT_Customization_FacialHairPresets.DT_Customization_FacialHairPresets"}},
            {"SkinTone", {"SkinTone", "/Game/Gameplay/Character/Player/Customization/DT_Customization_SkinTone.DT_Customization_SkinTone"}},
            {"SkinColor", {"SkinTone", "/Game/Gameplay/Character/Player/Customization/DT_Customization_SkinTone.DT_Customization_SkinTone"}},
            {"HairColor", {"HairColor", "/Game/Gameplay/Character/Player/Customization/DT_Customization_HairColor.DT_Customization_HairColor"}},
            {"EyeColor", {"EyeColor", "/Game/Gameplay/Character/Player/Customization/DT_Customization_EyeColor.DT_Customization_EyeColor"}},
            {"EyebrowColor", {"EyebrowColor", "/Game/Gameplay/Character/Player/Customization/DT_Customization_EyebrowColor.DT_Customization_EyebrowColor"}},
        };
        return fields;
    }

    const std::unordered_map<std::string, const TCHAR*>& AppearanceHandleFields()
    {
        static const std::unordered_map<std::string, const TCHAR*> fields{
            {"BodyType", TEXT("BodyTypeDataHandle")},
            {"FaceType", TEXT("FaceDataHandle")},
            {"HairPreset", TEXT("HairPresetDataHandle")},
            {"FacialHairPreset", TEXT("FacialHairPresetDataHandle")},
            {"HairColor", TEXT("HairColorPrimitiveDataHandle")},
            {"SkinTone", TEXT("SkinTonePrimitiveDataHandle")},
            {"EyeColor", TEXT("EyeColorPrimitiveDataHandle")},
            {"EyebrowColor", TEXT("EyebrowColorPrimitiveDataHandle")},
        };
        return fields;
    }

}
namespace DragonWilds {
    void DragonWildsSpawnLoader::LoadPlayerRules(
        const fs::path& loaderPath, const RC::StringType& modName, bool replaceExisting)
    {
        if (replaceExisting)
        {
            std::erase_if(m_playerRules, [&](const PlayerRule& rule) { return rule.ModName == modName; });
            m_reportedPlayerRuleFailures.clear();
            m_reportedPlayerRuleApplications.clear();
            m_reportedAppearanceNoOps.clear();
        }

        PS::JsonHelpers::ParseJsonFilesInPath(loaderPath, [&](const nlohmann::json& data) {
            m_playerDocuments.push_back({modName, data});
        });
    }

    void DragonWildsSpawnLoader::ParsePlayerRulesDocument(
        const nlohmann::json& data, const RC::StringType& modName)
    {
            if (!data.is_array()) throw std::runtime_error("players JSON root must be an array");
            for (const auto& value : data)
            {
                if (!value.is_object()) throw std::runtime_error("each players entry must be an object");
                PlayerRule rule;
                rule.ModName = modName;
                const auto addPlayerSelector = [&](std::string name, const char* field) {
                    if (name == "*")
                    {
                        rule.AllPlayers = true;
                        return;
                    }
                    if (name.size() > 1 && name.front() == '*')
                    {
                        const auto digits = name.substr(1);
                        if (!std::all_of(digits.begin(), digits.end(), [](unsigned char c) {
                            return std::isdigit(c) != 0;
                        }))
                            throw std::runtime_error(std::string(field)
                                + " wildcard must be *, *1, *2, and so on");
                        const auto slot = std::stoull(digits);
                        if (slot == 0 || slot > 9999)
                            throw std::runtime_error(std::string(field)
                                + " numbered wildcard must be between *1 and *9999");
                        rule.PlayerLoadSlots.push_back(static_cast<std::size_t>(slot));
                        return;
                    }
                    if (!name.empty()) rule.PlayerNames.push_back(std::move(name));
                };
                if (value.contains("PlayerName"))
                {
                    if (!value.at("PlayerName").is_string())
                        throw std::runtime_error("PlayerName must be a string");
                    auto name = value.at("PlayerName").get<std::string>();
                    addPlayerSelector(std::move(name), "PlayerName");
                }
                if (value.contains("PlayerNames"))
                {
                    if (!value.at("PlayerNames").is_array())
                        throw std::runtime_error("PlayerNames must be an array of strings");
                    for (const auto& nameValue : value.at("PlayerNames"))
                    {
                        if (!nameValue.is_string())
                            throw std::runtime_error("PlayerNames must contain only strings");
                        auto name = nameValue.get<std::string>();
                        addPlayerSelector(std::move(name), "PlayerNames");
                    }
                }
                if (value.contains("PlayerGuid"))
                {
                    if (!value.at("PlayerGuid").is_string())
                        throw std::runtime_error("PlayerGuid must be a string");
                    auto guid = value.at("PlayerGuid").get<std::string>();
                    if (!guid.empty()) rule.PlayerGuids.push_back(std::move(guid));
                }
                if (value.contains("PlayerGuids"))
                {
                    if (!value.at("PlayerGuids").is_array())
                        throw std::runtime_error("PlayerGuids must be an array of strings");
                    for (const auto& guidValue : value.at("PlayerGuids"))
                    {
                        if (!guidValue.is_string())
                            throw std::runtime_error("PlayerGuids must contain only strings");
                        auto guid = guidValue.get<std::string>();
                        if (!guid.empty()) rule.PlayerGuids.push_back(std::move(guid));
                    }
                }
                if (!rule.AllPlayers && rule.PlayerNames.empty() && rule.PlayerGuids.empty()
                    && rule.PlayerLoadSlots.empty())
                    throw std::runtime_error(
                        "a players entry requires PlayerName, PlayerNames, PlayerGuid, or PlayerGuids");

                const auto parse = [&](const char* field, bool& specified, double& target,
                    double minimum, double maximum = 100.0) {
                    if (!value.contains(field)) return;
                    if (!value.at(field).is_number())
                        throw std::runtime_error(std::string(field) + " must be a number");
                    target = value.at(field).get<double>();
                    if (!std::isfinite(target) || target < minimum || target > maximum)
                        throw std::runtime_error(std::string(field) + " is out of range");
                    specified = true;
                };
                parse("Scale", rule.SetScale, rule.ScaleMultiplier, 0.25, 3.0);
                parse("HealthMultiplier", rule.SetHealth, rule.HealthMultiplier, 0.1);
                parse("MaxHealth", rule.SetMaxHealth, rule.MaxHealth, 1.0, 1000000.0);
                parse("BaseHealth", rule.SetMaxHealth, rule.MaxHealth, 1.0, 1000000.0);
                parse("DefenseMultiplier", rule.SetDefense, rule.DefenseMultiplier, 0.1);
                parse("DamageMultiplier", rule.SetDamage, rule.DamageMultiplier, 0.1);
                parse("StaminaMultiplier", rule.SetStamina, rule.StaminaMultiplier, 0.1);
                parse("MaxStamina", rule.SetMaxStamina, rule.MaxStamina, 1.0, 1000000.0);
                parse("WalkSpeedMultiplier", rule.SetWalkSpeed, rule.WalkSpeedMultiplier, 0.1, 10.0);
                parse("RunSpeedMultiplier", rule.SetRunSpeed, rule.RunSpeedMultiplier, 0.1, 10.0);
                parse("CarryWeightMultiplier", rule.SetCarryWeight,
                    rule.CarryWeightMultiplier, 0.1, 100.0);
                parse("MaxCarryWeight", rule.SetMaxCarryWeight,
                    rule.MaxCarryWeight, 1.0, 1000000.0);
                parse("PoisonResistanceMultiplier", rule.SetPoisonResistance,
                    rule.PoisonResistanceMultiplier, 0.0, 100.0);
                parse("StaminaRecoveryMultiplier", rule.SetStaminaRecovery,
                    rule.StaminaRecoveryMultiplier, 0.0, 100.0);
                parse("PhysicalAttackMultiplier", rule.SetPhysicalAttack,
                    rule.PhysicalAttackMultiplier, 0.0);
                parse("MagicalAttackMultiplier", rule.SetMagicalAttack,
                    rule.MagicalAttackMultiplier, 0.0);
                parse("MagicAttackMultiplier", rule.SetMagicalAttack,
                    rule.MagicalAttackMultiplier, 0.0);
                parse("RangedAttackMultiplier", rule.SetRangedAttack,
                    rule.RangedAttackMultiplier, 0.0);
                parse("RangeAttackMultiplier", rule.SetRangedAttack,
                    rule.RangedAttackMultiplier, 0.0);
                parse("PhysicalDefenseMultiplier", rule.SetPhysicalDefense,
                    rule.PhysicalDefenseMultiplier, 0.0);
                parse("MagicalDefenseMultiplier", rule.SetMagicalDefense,
                    rule.MagicalDefenseMultiplier, 0.0);
                parse("MagicDefenseMultiplier", rule.SetMagicalDefense,
                    rule.MagicalDefenseMultiplier, 0.0);
                parse("RangedDefenseMultiplier", rule.SetRangedDefense,
                    rule.RangedDefenseMultiplier, 0.0);
                parse("RangeDefenseMultiplier", rule.SetRangedDefense,
                    rule.RangedDefenseMultiplier, 0.0);
                if (value.contains("AttributeMultipliers"))
                {
                    const auto& attributes = value.at("AttributeMultipliers");
                    if (!attributes.is_object())
                        throw std::runtime_error("AttributeMultipliers must be an object");
                    for (const auto& [identifier, multiplierValue] : attributes.items())
                    {
                        if (identifier.empty() || !multiplierValue.is_number())
                            throw std::runtime_error(
                                "AttributeMultipliers requires non-empty names and numeric values");
                        const double multiplier = multiplierValue.get<double>();
                        if (!std::isfinite(multiplier) || multiplier < 0.0 || multiplier > 100.0)
                            throw std::runtime_error(
                                "AttributeMultipliers values must be between 0 and 100");
                        rule.AttributeMultipliers.push_back({identifier, multiplier});
                    }
                }
                if (value.contains("Attributes"))
                {
                    const auto& attributes = value.at("Attributes");
                    if (!attributes.is_object())
                        throw std::runtime_error("Attributes must be an object");
                    for (const auto& [identifier, editValue] : attributes.items())
                    {
                        if (identifier.empty() || !editValue.is_object())
                            throw std::runtime_error(
                                "Attributes requires non-empty names and operation objects");
                        const std::array<std::pair<const char*, EPlayerAttributeEditOperation>, 3>
                            operations{{
                                {"Set", EPlayerAttributeEditOperation::Set},
                                {"Add", EPlayerAttributeEditOperation::Add},
                                {"Multiply", EPlayerAttributeEditOperation::Multiply},
                            }};
                        const char* selectedName = nullptr;
                        EPlayerAttributeEditOperation selectedOperation{};
                        double selectedValue = 0.0;
                        for (const auto& [operationName, operation] : operations)
                        {
                            if (!editValue.contains(operationName)) continue;
                            if (selectedName)
                                throw std::runtime_error(
                                    "Attributes entries must contain exactly one of Set, Add, or Multiply");
                            if (!editValue.at(operationName).is_number())
                                throw std::runtime_error(
                                    std::string("Attributes ") + operationName + " must be a number");
                            selectedName = operationName;
                            selectedOperation = operation;
                            selectedValue = editValue.at(operationName).get<double>();
                        }
                        if (!selectedName)
                            throw std::runtime_error(
                                "Attributes entries require Set, Add, or Multiply");
                        if (!std::isfinite(selectedValue)
                            || (selectedOperation == EPlayerAttributeEditOperation::Multiply
                                && (selectedValue < 0.0 || selectedValue > 100.0))
                            || (selectedOperation != EPlayerAttributeEditOperation::Multiply
                                && (selectedValue < -1000000.0 || selectedValue > 1000000.0)))
                            throw std::runtime_error(
                                "Attributes operation value is out of range");
                        rule.Attributes.push_back(
                            {identifier, selectedOperation, selectedValue});
                    }
                }
                if (value.contains("Appearance"))
                {
                    const auto& appearance = value.at("Appearance");
                    if (!appearance.is_object())
                        throw std::runtime_error("Appearance must be an object");

                    for (const auto& [inputField, selectionValue] : appearance.items())
                    {
                        const auto known = AppearanceFields().find(inputField);
                        if (known == AppearanceFields().end())
                            throw std::runtime_error("unsupported Appearance field: " + inputField);
                        std::string tablePath = known->second.second;
                        std::string rowName;
                        std::string sourceName;
                        std::string fallbackTablePath;
                        std::string fallbackRowName;
                        bool hasFallback = false;
                        if (selectionValue.is_string())
                        {
                            rowName = selectionValue.get<std::string>();
                        }
                        else if (selectionValue.is_object())
                        {
                            const char* rowKey = selectionValue.contains("Name") ? "Name"
                                : selectionValue.contains("Row") ? "Row" : "RowName";
                            if (!selectionValue.contains(rowKey)
                                || !selectionValue.at(rowKey).is_string())
                                throw std::runtime_error(inputField + " requires a string Name, Row, or RowName");
                            rowName = selectionValue.at(rowKey).get<std::string>();
                            if (selectionValue.contains("Source"))
                            {
                                if (!selectionValue.at("Source").is_string())
                                    throw std::runtime_error(inputField + " Source must be a string");
                                sourceName = selectionValue.at("Source").get<std::string>();
                                const auto source = m_appearanceSources.find(sourceName);
                                if (source == m_appearanceSources.end())
                                    throw std::runtime_error(inputField + " references an unavailable or disabled appearance source: " + sourceName);
                                const auto table = source->second.Tables.find(known->second.first);
                                if (table == source->second.Tables.end())
                                    throw std::runtime_error("appearance source " + sourceName + " has no table for " + known->second.first);
                                tablePath = table->second;
                                const auto fallback = source->second.FallbackRows.find(known->second.first);
                                if (fallback != source->second.FallbackRows.end())
                                {
                                    fallbackTablePath = known->second.second;
                                    fallbackRowName = fallback->second;
                                    hasFallback = true;
                                }
                            }
                            if (selectionValue.contains("DataTable"))
                            {
                                if (!selectionValue.at("DataTable").is_string())
                                    throw std::runtime_error(inputField + " DataTable must be a string");
                                tablePath = selectionValue.at("DataTable").get<std::string>();
                            }
                            if (selectionValue.contains("Fallback"))
                            {
                                const auto& fallback = selectionValue.at("Fallback");
                                fallbackTablePath = known->second.second;
                                if (fallback.is_string()) fallbackRowName = fallback.get<std::string>();
                                else if (fallback.is_object())
                                {
                                    if (!fallback.contains("RowName") || !fallback.at("RowName").is_string())
                                        throw std::runtime_error(inputField + " Fallback requires a string RowName");
                                    fallbackRowName = fallback.at("RowName").get<std::string>();
                                    if (fallback.contains("DataTable"))
                                    {
                                        if (!fallback.at("DataTable").is_string())
                                            throw std::runtime_error(inputField + " Fallback DataTable must be a string");
                                        fallbackTablePath = fallback.at("DataTable").get<std::string>();
                                    }
                                }
                                else throw std::runtime_error(inputField + " Fallback must be a row string or object");
                                hasFallback = true;
                            }
                        }
                        else
                        {
                            throw std::runtime_error(inputField
                                + " must be a row-name string or a DataTable/RowName object");
                        }
                        if (rowName.empty() || tablePath.empty())
                            throw std::runtime_error(inputField + " has an empty table or row name");
                        if (!sourceName.empty() && !hasFallback)
                            throw std::runtime_error(inputField + " uses Source '" + sourceName
                                + "' but has no safe vanilla Fallback in the selection or source manifest");
                        std::erase_if(rule.Appearance, [&](const PlayerAppearanceSelection& existing) {
                            return existing.Field == known->second.first;
                        });
                        rule.Appearance.push_back({known->second.first, std::move(tablePath),
                            std::move(rowName), std::move(sourceName),
                            std::move(fallbackTablePath), std::move(fallbackRowName), hasFallback});
                    }
                }
                if (value.contains("VisualEffect"))
                    rule.VisualEffect = ValidateVisualEffect(value.at("VisualEffect"));
                if (value.contains("Nameplate"))
                {
                    const auto& nameplate = value.at("Nameplate");
                    if (!nameplate.is_object())
                        throw std::runtime_error("Nameplate must be an object");
                    for (const auto& [field, ignored] : nameplate.items())
                        if (field != "Mode" && field != "Icon"
                            && field != "Scale" && field != "Distance"
                            && field != "ShowSelf" && field != "Client"
                            && field != "Server" && field != "States")
                            throw std::runtime_error("unsupported Nameplate field: " + field);
                    rule.Nameplate.Configured = true;
                    rule.Nameplate.Mode = nameplate.value("Mode", std::string("Name"));
                    if (rule.Nameplate.Mode != "Name" && rule.Nameplate.Mode != "Icon"
                        && rule.Nameplate.Mode != "Hidden")
                        throw std::runtime_error("Nameplate.Mode must be Name, Icon, or Hidden");
                    if (nameplate.contains("Icon"))
                    {
                        if (!nameplate.at("Icon").is_string())
                            throw std::runtime_error("Nameplate.Icon must be a cooked texture path");
                        rule.Nameplate.Icon = nameplate.at("Icon").get<std::string>();
                    }
                    if (rule.Nameplate.Mode == "Icon" && rule.Nameplate.Icon.empty())
                        throw std::runtime_error("Nameplate.Mode Icon requires Nameplate.Icon");
                    if (nameplate.contains("Scale"))
                    {
                        if (!nameplate.at("Scale").is_number())
                            throw std::runtime_error("Nameplate.Scale must be a number");
                        rule.Nameplate.Scale = nameplate.at("Scale").get<double>();
                        if (!std::isfinite(rule.Nameplate.Scale)
                            || rule.Nameplate.Scale < 0.1 || rule.Nameplate.Scale > 4.0)
                            throw std::runtime_error("Nameplate.Scale must be between 0.1 and 4");
                    }
                    if (nameplate.contains("Distance"))
                    {
                        if (!nameplate.at("Distance").is_number())
                            throw std::runtime_error("Nameplate.Distance must be a number");
                        rule.Nameplate.Distance = nameplate.at("Distance").get<double>();
                        if (!std::isfinite(rule.Nameplate.Distance)
                            || rule.Nameplate.Distance < 0.0 || rule.Nameplate.Distance > 100000.0)
                            throw std::runtime_error("Nameplate.Distance must be between 0 and 100000");
                    }
                    if (nameplate.contains("ShowSelf"))
                    {
                        if (!nameplate.at("ShowSelf").is_boolean())
                            throw std::runtime_error("Nameplate.ShowSelf must be true or false");
                        rule.Nameplate.ShowSelf = nameplate.at("ShowSelf").get<bool>();
                    }
                    const auto parseAudience = [&](const char* field, bool& target) {
                        if (!nameplate.contains(field)) return;
                        const auto& audience = nameplate.at(field);
                        if (audience.is_boolean())
                        {
                            target = audience.get<bool>();
                            return;
                        }
                        if (audience.is_string())
                        {
                            auto value = audience.get<std::string>();
                            std::transform(value.begin(), value.end(), value.begin(),
                                [](unsigned char character) {
                                    return static_cast<char>(std::tolower(character));
                                });
                            if (value == "yes") { target = true; return; }
                            if (value == "no") { target = false; return; }
                        }
                        throw std::runtime_error(std::string("Nameplate.") + field
                            + " must be Yes, No, true, or false");
                    };
                    parseAudience("Client", rule.Nameplate.ShowSelf);
                    parseAudience("Server", rule.Nameplate.ShowOthers);
                    if (nameplate.contains("States"))
                    {
                        const auto& states = nameplate.at("States");
                        if (!states.is_object())
                            throw std::runtime_error("Nameplate.States must be an object");
                        for (const auto& [stateName, stateValue] : states.items())
                        {
                            static const std::unordered_set<std::string> supportedStates{
                                "Dead", "Attack", "Ranged", "Magic",
                                "Woodcutting", "Mining"
                            };
                            if (!supportedStates.contains(stateName))
                                throw std::runtime_error(
                                    "unsupported Nameplate state: " + stateName);
                            if (!stateValue.is_object())
                                throw std::runtime_error(
                                    "Nameplate.States." + stateName + " must be an object");
                            for (const auto& [field, ignored] : stateValue.items())
                                if (field != "Icon" && field != "Scale"
                                    && field != "InactivitySeconds")
                                    throw std::runtime_error(
                                        "unsupported Nameplate.States." + stateName
                                        + " field: " + field);
                            if (!stateValue.contains("Icon")
                                || !stateValue.at("Icon").is_string()
                                || stateValue.at("Icon").get<std::string>().empty())
                                throw std::runtime_error(
                                    "Nameplate.States." + stateName
                                    + " requires a cooked Icon path");
                            auto& stateRule = rule.Nameplate.States[stateName];
                            stateRule.Configured = true;
                            stateRule.Icon = stateValue.at("Icon").get<std::string>();
                            if (stateName != "Dead")
                                stateRule.InactivitySeconds = 2.0;
                            if (stateValue.contains("Scale"))
                            {
                                if (!stateValue.at("Scale").is_number())
                                    throw std::runtime_error(
                                        "Nameplate.States." + stateName
                                        + ".Scale must be a number");
                                stateRule.Scale = stateValue.at("Scale").get<double>();
                                if (!std::isfinite(stateRule.Scale)
                                    || stateRule.Scale < 0.1
                                    || stateRule.Scale > 4.0)
                                    throw std::runtime_error(
                                        "Nameplate.States." + stateName
                                        + ".Scale must be between 0.1 and 4");
                            }
                            if (stateValue.contains("InactivitySeconds"))
                            {
                                if (!stateValue.at("InactivitySeconds").is_number())
                                    throw std::runtime_error(
                                        "Nameplate.States." + stateName
                                        + ".InactivitySeconds must be a number");
                                stateRule.InactivitySeconds =
                                    stateValue.at("InactivitySeconds").get<double>();
                                if (!std::isfinite(stateRule.InactivitySeconds)
                                    || stateRule.InactivitySeconds < 0.0
                                    || stateRule.InactivitySeconds > 3600.0)
                                    throw std::runtime_error(
                                        "Nameplate.States." + stateName
                                        + ".InactivitySeconds must be between 0 and 3600");
                            }
                        }
                    }
                }
                if (rule.SetHealth && rule.SetMaxHealth)
                    throw std::runtime_error("HealthMultiplier cannot be combined with MaxHealth or BaseHealth");
                if (rule.SetStamina && rule.SetMaxStamina)
                    throw std::runtime_error("StaminaMultiplier cannot be combined with MaxStamina");
                if (rule.SetCarryWeight && rule.SetMaxCarryWeight)
                    throw std::runtime_error(
                        "CarryWeightMultiplier cannot be combined with MaxCarryWeight");
                if (!rule.SetScale && !rule.SetHealth && !rule.SetMaxHealth && !rule.SetDefense
                    && !rule.SetDamage && !rule.SetStamina && !rule.SetMaxStamina
                    && !rule.SetWalkSpeed && !rule.SetRunSpeed
                    && !rule.SetCarryWeight && !rule.SetMaxCarryWeight
                    && !rule.SetPoisonResistance && !rule.SetStaminaRecovery
                    && !rule.SetPhysicalAttack && !rule.SetMagicalAttack
                    && !rule.SetRangedAttack && !rule.SetPhysicalDefense
                    && !rule.SetMagicalDefense && !rule.SetRangedDefense
                    && rule.AttributeMultipliers.empty() && rule.Attributes.empty()
                    && rule.Appearance.empty() && rule.VisualEffect.empty()
                    && !rule.Nameplate.Configured)
                    throw std::runtime_error("a players entry requires at least one adjustment field");
                m_playerRules.push_back(std::move(rule));
            }
    }

    void DragonWildsSpawnLoader::FinalizePlayerRules()
    {
        if (m_playerDocuments.empty()) return;
        struct Definition { RC::StringType Owner; std::string Reference; nlohmann::json Body; };
        struct Patch { RC::StringType Owner; JsonPatchDirective::Directive Directive; };
        std::vector<Definition> definitions;
        std::vector<Patch> patches;
        std::unordered_map<std::string, std::size_t> byReference;

        for (const auto& owned : m_playerDocuments)
        {
            if (!owned.Document.is_array())
            {
                PS::Log<LogLevel::Error>(STR("Players file for {} must be an array.\n"), owned.ModName);
                continue;
            }
            std::size_t ordinal = 0;
            for (const auto& incoming : owned.Document)
            {
                ++ordinal;
                try
                {
                    static constexpr std::array<std::string_view, 8> protectedIdentity{
                        "Id", "$Id", "PlayerName", "PlayerNames", "PlayerGuid",
                        "PlayerGuids", "PlayerSlot", "PlayerSlots"};
                    if (const auto patch = JsonPatchDirective::Parse(incoming, protectedIdentity, "player"))
                    {
                        patches.push_back({owned.ModName, *patch});
                        continue;
                    }
                    if (!incoming.is_object()) throw std::runtime_error("player rule must be an object");
                    std::string identity;
                    if (incoming.contains("$Id") && incoming.at("$Id").is_string()) identity = incoming.at("$Id").get<std::string>();
                    else if (incoming.contains("Id") && incoming.at("Id").is_string()) identity = incoming.at("Id").get<std::string>();
                    const auto reference = identity.empty()
                        ? RC::to_string(owned.ModName) + ":#" + std::to_string(ordinal)
                        : RC::to_string(owned.ModName) + ":" + identity;
                    if (identity.empty())
                    {
                        definitions.push_back({owned.ModName, reference, incoming});
                        continue;
                    }
                    if (const auto found = byReference.find(reference); found != byReference.end())
                        JsonLoadOrderMerge::Apply(definitions.at(found->second).Body, incoming, true);
                    else
                    {
                        byReference.emplace(reference, definitions.size());
                        definitions.push_back({owned.ModName, reference, incoming});
                    }
                }
                catch (const std::exception& error)
                {
                    PS::Log<LogLevel::Error>(STR("Player rule from {} was rejected: {}.\n"),
                        owned.ModName, PS::ToWideSafe(error.what()));
                }
            }
        }

        size_t patched = 0, patchErrors = 0;
        for (auto& patch : patches)
        {
            auto reference = patch.Directive.Reference;
            if (reference.find(':') == std::string::npos)
                reference = RC::to_string(patch.Owner) + ":" + reference;
            const auto found = byReference.find(reference);
            if (found == byReference.end())
            {
                PS::Log<LogLevel::Error>(STR("{}: player $Patch target '{}' was not loaded; no rule was created.\n"),
                    patch.Owner, RC::to_generic_string(reference));
                ++patchErrors;
                continue;
            }
            const auto stats = JsonPatchDirective::Apply(
                definitions.at(found->second).Body, patch.Directive, true);
            ++patched;
            PS::Log<LogLevel::Verbose>( STR("{} patched player rule '{}' ({} fields overwritten).\n"),
                patch.Owner, RC::to_generic_string(reference), stats.FieldsOverwritten);
        }

        if (patched || patchErrors) PS::RoutineLog("patches", STR("Players $Patch: {} updated, {} errors.\n"), patched, patchErrors);

        for (auto& definition : definitions)
        {
            definition.Body.erase("$Id");
            definition.Body.erase("Id");
            try { ParsePlayerRulesDocument(nlohmann::json::array({definition.Body}), definition.Owner); }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Error>(STR("Failed to register player rule '{}' from {}: {}.\n"),
                    RC::to_generic_string(definition.Reference), definition.Owner,
                    PS::ToWideSafe(error.what()));
            }
        }
        PS::RoutineLog("players", STR("Loaded {} deterministic player rule(s); applied {} deferred patch(es).\n"),
            definitions.size(), patches.size());
        m_playerDocuments.clear();
    }

    UObject* DragonWildsSpawnLoader::FindLocalPlayerController()
    {
        auto* controllerClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.DominionPlayerController"));
        if (!controllerClass) return nullptr;
        TArray<UObject*> controllers;
        UECustom::UObjectGlobals::GetObjectsOfClass(controllerClass, controllers, true);
        for (auto* controller : controllers)
        {
            if (!controller || controller->HasAnyFlags(static_cast<EObjectFlags>(
                RF_ClassDefaultObject | RF_ArchetypeObject))) continue;
            auto local = ActorHelper::FunctionCall(
                controller, STR("/Script/Engine.Controller:IsLocalController"));
            local.Invoke();
            if (local.Result<bool>()) return controller;
        }
        return nullptr;
    }

    UObject* DragonWildsSpawnLoader::FindPlayerControllerByName(
        const RC::StringType& playerName, bool& ambiguous)
    {
        ambiguous = false;
        auto* controllerClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.DominionPlayerController"));
        if (!controllerClass) return nullptr;

        const auto equalIgnoringCase = [](const RC::StringType& left, const RC::StringType& right) {
            if (left.size() != right.size()) return false;
            for (std::size_t index = 0; index < left.size(); ++index)
            {
                if (std::towlower(left[index]) != std::towlower(right[index])) return false;
            }
            return true;
        };

        UObject* match = nullptr;
        TArray<UObject*> controllers;
        UECustom::UObjectGlobals::GetObjectsOfClass(controllerClass, controllers, true);
        for (auto* controller : controllers)
        {
            if (!controller || controller->GetWorld() != m_readyWorld
                || controller->HasAnyFlags(
                    static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject)))
            {
                continue;
            }

            auto* statePointer = PropertyHelper::GetValuePtrByPropertyNameInChain<
                TObjectPtr<UObject>>(controller, TEXT("PlayerState"));
            auto* playerState = statePointer ? statePointer->Get() : nullptr;
            if (!playerState) continue;

            auto getName = ActorHelper::FunctionCall(
                playerState, STR("/Script/Engine.PlayerState:GetPlayerName"));
            getName.Invoke();
            const auto actualName = getName.Result<FString>();
            if (actualName.GetCharArray().Num() <= 1
                || !equalIgnoringCase(RC::StringType(*actualName), playerName))
            {
                continue;
            }
            if (match)
            {
                ambiguous = true;
                return nullptr;
            }
            match = controller;
        }
        return match;
    }

    std::string DragonWildsSpawnLoader::GetPlayerControllerName(UObject* controller)
    {
        if (!controller) return {};
        try
        {
            auto* statePointer = PropertyHelper::GetValuePtrByPropertyNameInChain<
                TObjectPtr<UObject>>(controller, TEXT("PlayerState"));
            auto* playerState = statePointer ? statePointer->Get() : nullptr;
            if (!playerState) return {};
            auto getName = ActorHelper::FunctionCall(
                playerState, STR("/Script/Engine.PlayerState:GetPlayerName"));
            getName.Invoke();
            const auto actualName = getName.Result<FString>();
            return actualName.GetCharArray().Num() > 1
                ? RC::to_string(RC::StringType(*actualName)) : std::string{};
        }
        catch (...) { return {}; }
    }

    std::string DragonWildsSpawnLoader::GetPlayerCharacterGuid(UObject* controller)
    {
        if (!controller) return {};
        try
        {
            auto getGuid = ActorHelper::FunctionCall(
                controller, STR("/Script/Dominion.DominionPlayerControllerBase:GetCharacterGuid"));
            getGuid.Invoke();
            FGuid guid{};
            getGuid.MoveResult(&guid, sizeof(guid));
            uint32 lanes[4]{};
            std::memcpy(lanes, &guid, sizeof(lanes));
            if (lanes[0] == 0 && lanes[1] == 0 && lanes[2] == 0 && lanes[3] == 0) return {};
            return std::format("{:08X}{:08X}{:08X}{:08X}",
                lanes[0], lanes[1], lanes[2], lanes[3]);
        }
        catch (...) { return {}; }
    }

    UObject* DragonWildsSpawnLoader::FindPlayerControllerByGuid(
        const std::string& playerGuid, bool& ambiguous)
    {
        ambiguous = false;
        const auto normalize = [](std::string_view value) {
            std::string normalized;
            normalized.reserve(32);
            for (const unsigned char character : value)
            {
                if (std::isxdigit(character))
                    normalized.push_back(static_cast<char>(std::toupper(character)));
            }
            return normalized;
        };
        const auto expected = normalize(playerGuid);
        if (expected.size() != 32) return nullptr;

        auto* controllerClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.DominionPlayerController"));
        if (!controllerClass) return nullptr;
        UObject* match = nullptr;
        TArray<UObject*> controllers;
        UECustom::UObjectGlobals::GetObjectsOfClass(controllerClass, controllers, true);
        for (auto* controller : controllers)
        {
            if (!controller || controller->GetWorld() != m_readyWorld
                || controller->HasAnyFlags(static_cast<EObjectFlags>(
                    RF_ClassDefaultObject | RF_ArchetypeObject))) continue;
            if (normalize(GetPlayerCharacterGuid(controller)) != expected) continue;
            if (match)
            {
                ambiguous = true;
                return nullptr;
            }
            match = controller;
        }
        return match;
    }

    std::vector<std::string> DragonWildsSpawnLoader::GetConnectedPlayerNames()
    {
        std::vector<std::string> names;
        auto* controllerClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.DominionPlayerController"));
        if (!controllerClass) return names;
        TArray<UObject*> controllers;
        UECustom::UObjectGlobals::GetObjectsOfClass(controllerClass, controllers, true);
        for (auto* controller : controllers)
        {
            if (!controller || controller->GetWorld() != m_readyWorld
                || controller->HasAnyFlags(static_cast<EObjectFlags>(
                    RF_ClassDefaultObject | RF_ArchetypeObject))) continue;
            auto* statePointer = PropertyHelper::GetValuePtrByPropertyNameInChain<
                TObjectPtr<UObject>>(controller, TEXT("PlayerState"));
            auto* playerState = statePointer ? statePointer->Get() : nullptr;
            if (!playerState) continue;
            try
            {
                auto getName = ActorHelper::FunctionCall(
                    playerState, STR("/Script/Engine.PlayerState:GetPlayerName"));
                getName.Invoke();
                const auto actualName = getName.Result<FString>();
                if (actualName.GetCharArray().Num() > 1)
                    names.push_back(RC::to_string(RC::StringType(*actualName)));
            }
            catch (...) {}
        }
        return names;
    }

    std::vector<DragonWildsSpawnLoader::PlayerLoadOrderEntry>
    DragonWildsSpawnLoader::GetConnectedPlayersInLoadOrder()
    {
        std::vector<PlayerLoadOrderEntry> connected;
        auto* controllerClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.DominionPlayerController"));
        if (!controllerClass) return connected;

        struct ConnectedController
        {
            int32_t PlayerId = -1;
            PlayerLoadOrderEntry Player;
        };
        std::vector<ConnectedController> orderedControllers;
        TArray<UObject*> controllers;
        UECustom::UObjectGlobals::GetObjectsOfClass(controllerClass, controllers, true);
        for (auto* controller : controllers)
        {
            if (!controller || controller->GetWorld() != m_readyWorld
                || controller->HasAnyFlags(static_cast<EObjectFlags>(
                    RF_ClassDefaultObject | RF_ArchetypeObject))) continue;
            if(PlayerGhost::HasItemRules())try {
                auto pawn=ActorHelper::FunctionCall(controller,TEXT("/Script/Engine.Controller:K2_GetPawn"));
                pawn.Invoke();PlayerGhost::TrackEquipment(pawn.Result<UObject*>());
            } catch(const std::exception& error) {
                PS::Log<LogLevel::Warning>(TEXT("Equipment visual unavailable: {}\n"),PS::ToWideSafe(error.what()));
            }
            const auto name = GetPlayerControllerName(controller);
            const auto guid = GetPlayerCharacterGuid(controller);
            if (name.empty() && guid.empty()) continue;
            const auto key = !guid.empty() ? "guid:" + guid : "name:" + name;
            int32_t playerId = -1;
            try
            {
                auto* statePointer = PropertyHelper::GetValuePtrByPropertyNameInChain<
                    TObjectPtr<UObject>>(controller, TEXT("PlayerState"));
                auto* playerState = statePointer ? statePointer->Get() : nullptr;
                if (playerState)
                {
                    auto getId = ActorHelper::FunctionCall(
                        playerState, STR("/Script/Engine.PlayerState:GetPlayerId"));
                    getId.Invoke();
                    playerId = getId.Result<int32_t>();
                }
            }
            catch (...) {}
            orderedControllers.push_back({playerId, {key, name, guid}});
        }
        std::stable_sort(orderedControllers.begin(), orderedControllers.end(),
            [](const auto& left, const auto& right) {
                if (left.PlayerId != right.PlayerId)
                    return left.PlayerId < right.PlayerId;
                return left.Player.Key < right.Player.Key;
            });
        for (auto& ordered : orderedControllers)
            connected.push_back(std::move(ordered.Player));
        return connected;
    }

    fs::path DragonWildsSpawnLoader::GetAppearanceProvenancePath()
    {
        return fs::path(PS::HostServices::WorkingDirectory())
            / "Mods" / "RuneSchema" / "player-data" / "appearance-fallbacks.json";
    }

    void DragonWildsSpawnLoader::LoadAppearanceProvenance()
    {
        if (m_appearanceProvenanceLoaded) return;
        m_appearanceProvenanceLoaded = true;
        const auto path = GetAppearanceProvenancePath();
        const auto temporary = fs::path(path.string() + ".tmp");
        std::error_code staleTemporaryError;
        if (fs::remove(temporary, staleTemporaryError))
        {
            PS::RoutineLog("players",
                STR("Removed stale RuneSchema appearance-state temporary file.\n"));
        }
        if (!fs::is_regular_file(path)) return;
        try
        {
            std::ifstream input(path, std::ios::binary);
            const auto document = nlohmann::json::parse(input, nullptr, true, true);
            if (!document.is_object() || document.value("SchemaVersion", 0) != 1
                || !document.contains("Fields") || !document.at("Fields").is_array())
                throw std::runtime_error("appearance fallback file has an invalid schema");
            for (const auto& value : document.at("Fields"))
            {
                AppearanceProvenance record;
                record.PlayerGuid = value.at("PlayerGuid").get<std::string>();
                record.Field = value.at("Field").get<std::string>();
                record.OwnerMod = value.at("OwnerMod").get<std::string>();
                record.Source = value.value("Source", std::string{});
                record.AppliedDataTablePath = value.at("AppliedDataTable").get<std::string>();
                record.AppliedRowName = value.at("AppliedRowName").get<std::string>();
                record.FallbackDataTablePath = value.at("FallbackDataTable").get<std::string>();
                record.FallbackRowName = value.at("FallbackRowName").get<std::string>();
                if (record.PlayerGuid.empty() || record.Field.empty()
                    || record.FallbackDataTablePath.empty() || record.FallbackRowName.empty())
                    throw std::runtime_error("appearance fallback file contains an incomplete field");
                m_appearanceProvenance.push_back(std::move(record));
            }
        }
        catch (const std::exception& error)
        {
            m_appearanceProvenance.clear();
            PS::Log<LogLevel::Error>(
                STR("Appearance fallback state was ignored safely: {}\n"),
                PS::ToWideSafe(error.what()));
        }
    }

    bool DragonWildsSpawnLoader::SaveAppearanceProvenance(std::string& error)
    {
        const auto path = GetAppearanceProvenancePath();
        const auto temporary = fs::path(path.string() + ".tmp");
        try
        {
            nlohmann::json fields = nlohmann::json::array();
            for (const auto& record : m_appearanceProvenance)
            {
                fields.push_back({
                    {"PlayerGuid", record.PlayerGuid}, {"Field", record.Field},
                    {"OwnerMod", record.OwnerMod}, {"Source", record.Source},
                    {"AppliedDataTable", record.AppliedDataTablePath},
                    {"AppliedRowName", record.AppliedRowName},
                    {"FallbackDataTable", record.FallbackDataTablePath},
                    {"FallbackRowName", record.FallbackRowName},
                });
            }
            fs::create_directories(path.parent_path());
            {
                std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
                if (!output) throw std::runtime_error("appearance fallback temporary file could not be opened");
                output << nlohmann::json{{"SchemaVersion", 1}, {"Fields", fields}}.dump(2) << '\n';
                if (!output.good()) throw std::runtime_error("appearance fallback write failed");
            }
            fs::copy_file(temporary, path, fs::copy_options::overwrite_existing);
            std::error_code ignored;
            fs::remove(temporary, ignored);
            return true;
        }
        catch (const std::exception& exception)
        {
            std::error_code ignored;
            fs::remove(temporary, ignored);
            error = exception.what();
            return false;
        }
    }

    bool DragonWildsSpawnLoader::ReadPlayerAppearance(
        UObject* pawn, const std::string& field, std::string& dataTablePath,
        std::string& rowName, UObject** customizationOut, std::string& error)
    {
        try
        {
            auto getCustomization = ActorHelper::FunctionCall(
                pawn, STR("/Script/Dominion.DominionPlayerCharacter:GetPlayerCustomizationComponent"));
            getCustomization.Invoke();
            auto* customization = getCustomization.Result<UObject*>();
            if (customizationOut) *customizationOut = customization;
            auto* saveProperty = customization ? CastField<FStructProperty>(
                PropertyHelper::GetPropertyByName(
                    customization->GetClassPrivate(), TEXT("CustomizationSaveData"))) : nullptr;
            auto* saveStruct = saveProperty ? saveProperty->GetStruct().Get() : nullptr;
            auto* saveData = saveProperty
                ? saveProperty->ContainerPtrToValuePtr<void>(customization) : nullptr;
            const auto handleName = AppearanceHandleFields().find(field);
            auto* handleProperty = handleName != AppearanceHandleFields().end() && saveStruct
                ? CastField<FStructProperty>(PropertyHelper::GetPropertyByName(
                    saveStruct, handleName->second)) : nullptr;
            auto* handleStruct = handleProperty ? handleProperty->GetStruct().Get() : nullptr;
            auto* tableProperty = handleStruct ? CastField<FObjectPropertyBase>(
                PropertyHelper::GetPropertyByName(handleStruct, TEXT("DataTable"))) : nullptr;
            auto* rowProperty = handleStruct ? CastField<FNameProperty>(
                PropertyHelper::GetPropertyByName(handleStruct, TEXT("RowName"))) : nullptr;
            if (!saveData || !handleProperty || !tableProperty || !rowProperty)
                throw std::runtime_error("customization field was unavailable");
            auto* handle = handleProperty->ContainerPtrToValuePtr<void>(saveData);
            UObject* table = nullptr;
            std::memcpy(&table, tableProperty->ContainerPtrToValuePtr<void>(handle), sizeof(table));
            const auto row = rowProperty->GetPropertyValue(
                rowProperty->ContainerPtrToValuePtr<void>(handle));
            if (!table || row == NAME_None)
                throw std::runtime_error("customization handle is empty");
            dataTablePath = RC::to_string(table->GetPathName());
            rowName = RC::to_string(row.ToString());
            return true;
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return false;
        }
        catch (...)
        {
            error = "unknown appearance read error";
            return false;
        }
    }

    bool DragonWildsSpawnLoader::WritePlayerAppearance(
        UObject* pawn, const std::string& field, const std::string& dataTablePath,
        const std::string& rowName, bool& changed, UObject** customizationOut,
        std::string& error)
    {
        changed = false;
        std::string currentTable;
        std::string currentRow;
        UObject* customization = nullptr;
        if (!ReadPlayerAppearance(pawn, field, currentTable, currentRow, &customization, error))
            return false;
        if (customizationOut) *customizationOut = customization;

        auto normalizedCurrent = ActorHelper::NormalizeObjectPath(RC::to_generic_string(currentTable));
        auto normalizedTarget = ActorHelper::NormalizeObjectPath(RC::to_generic_string(dataTablePath));
        if (normalizedCurrent == normalizedTarget && currentRow == rowName)
            return true;

        try
        {
            auto* saveProperty = CastField<FStructProperty>(PropertyHelper::GetPropertyByName(
                customization->GetClassPrivate(), TEXT("CustomizationSaveData")));
            auto* saveStruct = saveProperty ? saveProperty->GetStruct().Get() : nullptr;
            auto* saveData = saveProperty ? saveProperty->ContainerPtrToValuePtr<void>(customization) : nullptr;
            const auto handleName = AppearanceHandleFields().find(field);
            auto* handleProperty = handleName != AppearanceHandleFields().end() && saveStruct
                ? CastField<FStructProperty>(PropertyHelper::GetPropertyByName(saveStruct, handleName->second)) : nullptr;
            auto* handleStruct = handleProperty ? handleProperty->GetStruct().Get() : nullptr;
            auto* tableProperty = handleStruct ? CastField<FObjectPropertyBase>(
                PropertyHelper::GetPropertyByName(handleStruct, TEXT("DataTable"))) : nullptr;
            auto* rowProperty = handleStruct ? CastField<FNameProperty>(
                PropertyHelper::GetPropertyByName(handleStruct, TEXT("RowName"))) : nullptr;
            auto* resolved = ActorHelper::ResolveObject(normalizedTarget);
            auto* dataTableClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                nullptr, nullptr, TEXT("/Script/Engine.DataTable"));
            auto* table = resolved && dataTableClass && resolved->IsA(dataTableClass)
                ? static_cast<UDataTable*>(resolved) : nullptr;
            const FName targetRow(RC::to_generic_string(rowName), FNAME_Add);
            if (!saveData || !handleProperty || !tableProperty || !rowProperty
                || !table || !table->FindRowUnchecked(targetRow))
                throw std::runtime_error("table or row was unavailable");
            auto* handle = handleProperty->ContainerPtrToValuePtr<void>(saveData);
            std::memcpy(tableProperty->ContainerPtrToValuePtr<void>(handle), &table, sizeof(table));
            rowProperty->SetPropertyValue(
                rowProperty->ContainerPtrToValuePtr<void>(handle), targetRow);
            changed = true;
            return true;
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return false;
        }
        catch (...)
        {
            error = "unknown appearance write error";
            return false;
        }
    }

    void DragonWildsSpawnLoader::ClearAppearanceSources()
    {
        m_appearanceSources.clear();
    }

    void DragonWildsSpawnLoader::RegisterAppearanceSource(
        const fs::path& modPath, const RC::StringType& modName)
    {
        const auto manifestPath = modPath / "appearance" / "manifest.json";
        if (!fs::is_regular_file(manifestPath)) return;

        std::ifstream input(manifestPath, std::ios::binary);
        if (!input) throw std::runtime_error("appearance manifest could not be opened");
        const auto document = nlohmann::json::parse(input, nullptr, true, true);
        if (!document.is_object())
            throw std::runtime_error("appearance/manifest.json must contain an object");
        if (document.value("SchemaVersion", 1) != 1)
            throw std::runtime_error("appearance manifest SchemaVersion must be 1");
        if (!document.contains("Tables") || !document.at("Tables").is_object())
            throw std::runtime_error("appearance manifest requires a Tables object");

        AppearanceSource source;
        for (const auto& [inputField, path] : document.at("Tables").items())
        {
            const auto known = AppearanceFields().find(inputField);
            if (known == AppearanceFields().end() || !path.is_string() || path.get<std::string>().empty())
                throw std::runtime_error("appearance manifest contains an invalid table mapping for " + inputField);
            source.Tables[known->second.first] = path.get<std::string>();
        }
        if (document.contains("Fallbacks"))
        {
            if (!document.at("Fallbacks").is_object())
                throw std::runtime_error("appearance manifest Fallbacks must be an object");
            for (const auto& [inputField, row] : document.at("Fallbacks").items())
            {
                const auto known = AppearanceFields().find(inputField);
                if (known == AppearanceFields().end() || !row.is_string() || row.get<std::string>().empty())
                    throw std::runtime_error("appearance manifest contains an invalid fallback for " + inputField);
                source.FallbackRows[known->second.first] = row.get<std::string>();
            }
        }
        m_appearanceSources[RC::to_string(modName)] = std::move(source);
        PS::RoutineLog("players",
            STR("Registered appearance source '{}' from appearance/manifest.json.\n"), modName);
    }

    void DragonWildsSpawnLoader::ApplyClientPlayerVisualRules(
        UObject* pawn, bool nameplatesOnly)
    {
        if (!pawn || (m_playerRules.empty() && !PlayerGhost::HasItemRules())) return;
        auto* playerClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.DominionPlayerCharacter"));
        if (!playerClass || !pawn->IsA(playerClass)) return;
        if (nameplatesOnly && !GhostMaterials::CanRender(pawn)) return;
        if (!nameplatesOnly)
        {
            try { PlayerGhost::TrackEquipment(pawn); }
            catch(const std::exception& error) { PS::Log<LogLevel::Warning>(TEXT("Equipment visual unavailable: {}\n"),PS::ToWideSafe(error.what())); }
        }
        if(m_playerRules.empty())return;

        auto* statePointer = PropertyHelper::GetValuePtrByPropertyNameInChain<
            TObjectPtr<UObject>>(pawn, TEXT("PlayerState"));
        auto* playerState = statePointer ? statePointer->Get() : nullptr;
        if (!playerState) return;

        std::string playerName;
        int32_t playerId = -1;
        try
        {
            auto getName = ActorHelper::FunctionCall(
                playerState, STR("/Script/Engine.PlayerState:GetPlayerName"));
            getName.Invoke();
            const auto name = getName.Result<FString>();
            if (name.GetCharArray().Num() > 1)
                playerName = RC::to_string(RC::StringType(*name));

            auto getId = ActorHelper::FunctionCall(
                playerState, STR("/Script/Engine.PlayerState:GetPlayerId"));
            getId.Invoke();
            playerId = getId.Result<int32_t>();
        }
        catch (...) {}

        std::string playerGuid;
        auto* controllerClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.DominionPlayerController"));
        if (controllerClass)
        {
            TArray<UObject*> controllers;
            UECustom::UObjectGlobals::GetObjectsOfClass(
                controllerClass, controllers, true);
            for (auto* controller : controllers)
            {
                if (!controller || controller->GetWorld() != pawn->GetWorld()) continue;
                try
                {
                    auto getPawn = ActorHelper::FunctionCall(
                        controller, STR("/Script/Engine.Controller:K2_GetPawn"));
                    getPawn.Invoke();
                    if (getPawn.Result<UObject*>() == pawn)
                    {
                        playerGuid = GetPlayerCharacterGuid(controller);
                        break;
                    }
                }
                catch (...) {}
            }
        }

        struct ActivePlayerState { int32_t Id; UObject* State; };
        std::vector<ActivePlayerState> activeStates;
        auto* stateClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Engine.PlayerState"));
        if (stateClass)
        {
            TArray<UObject*> states;
            UECustom::UObjectGlobals::GetObjectsOfClass(stateClass, states, true);
            for (auto* state : states)
            {
                if (!state || state->GetWorld() != pawn->GetWorld()
                    || state->HasAnyFlags(static_cast<EObjectFlags>(
                        RF_ClassDefaultObject | RF_ArchetypeObject))) continue;
                try
                {
                    auto getPawn = ActorHelper::FunctionCall(
                        state, STR("/Script/Engine.PlayerState:GetPawn"));
                    getPawn.Invoke();
                    if (!getPawn.Result<UObject*>()) continue;
                    auto getId = ActorHelper::FunctionCall(
                        state, STR("/Script/Engine.PlayerState:GetPlayerId"));
                    getId.Invoke();
                    activeStates.push_back({getId.Result<int32_t>(), state});
                }
                catch (...) {}
            }
        }
        std::stable_sort(activeStates.begin(), activeStates.end(),
            [](const auto& left, const auto& right) { return left.Id < right.Id; });
        std::size_t playerSlot = 0;
        for (std::size_t index = 0; index < activeStates.size(); ++index)
        {
            if (activeStates[index].State == playerState)
            {
                playerSlot = index + 1;
                break;
            }
        }

        const PlayerNameplateRule* selectedNameplate = nullptr;
        RC::StringType selectedNameplateContext;
        for (const auto& rule : m_playerRules)
        {
            if (rule.Nameplate.Configured == false
                && (nameplatesOnly || rule.VisualEffect.empty())) continue;
            const bool nameMatches = !playerName.empty()
                && std::find(rule.PlayerNames.begin(), rule.PlayerNames.end(), playerName)
                    != rule.PlayerNames.end();
            const bool guidMatches = !playerGuid.empty()
                && std::find(rule.PlayerGuids.begin(), rule.PlayerGuids.end(), playerGuid)
                    != rule.PlayerGuids.end();
            const bool slotMatches = playerSlot != 0
                && std::find(rule.PlayerLoadSlots.begin(), rule.PlayerLoadSlots.end(),
                    playerSlot) != rule.PlayerLoadSlots.end();
            if (!rule.AllPlayers && !nameMatches && !guidMatches && !slotMatches)
                continue;
            const auto context = STR("Player visual rule from '")
                + rule.ModName + STR("'");
            if (!nameplatesOnly && !rule.VisualEffect.empty())
                ApplyVisualEffect(pawn, rule.VisualEffect, context);
            if (rule.Nameplate.Configured)
            {
                selectedNameplate = &rule.Nameplate;
                selectedNameplateContext = context;
            }
        }
        if (selectedNameplate)
        {
            bool isLocalPlayer = false;
            try
            {
                auto* controllerPointer = PropertyHelper::GetValuePtrByPropertyNameInChain<
                    TObjectPtr<UObject>>(pawn, TEXT("Controller"));
                auto* controller = controllerPointer ? controllerPointer->Get() : nullptr;
                if (controller)
                {
                    auto local = ActorHelper::FunctionCall(
                        controller, STR("/Script/Engine.Controller:IsLocalController"));
                    local.Invoke();
                    isLocalPlayer = local.Result<bool>();
                }
            }
            catch (...) {}
            if ((isLocalPlayer && selectedNameplate->ShowSelf)
                || (!isLocalPlayer && selectedNameplate->ShowOthers))
                ApplyPlayerNameplate(pawn, *selectedNameplate, selectedNameplateContext);
        }
    }

    bool DragonWildsSpawnLoader::ApplyPlayerNameplate(UObject* pawn,
        const PlayerNameplateRule& rule, const RC::StringType& context)
    {
        try
        {
            const auto stateRule = [&](const std::string& name)
                -> const PlayerNameplateStateRule* {
                const auto found = rule.States.find(name);
                return found != rule.States.end() && found->second.Configured
                    ? &found->second : nullptr;
            };
            const auto* deadRule = stateRule("Dead");
            bool dead = false;
            if (deadRule)
            {
                auto* damage = ActorHelper::GetObjectRef(
                    pawn, TEXT("BP_Components_PlayerDamage"));
                auto* fatalProperty = damage ? CastField<FStructProperty>(
                    PropertyHelper::GetPropertyByName(
                        damage->GetClassPrivate(), TEXT("FatalDamageInfo"))) : nullptr;
                auto* fatalStruct = fatalProperty
                    ? fatalProperty->GetStruct().Get() : nullptr;
                auto* isSetProperty = fatalStruct ? CastField<FBoolProperty>(
                    PropertyHelper::GetPropertyByName(
                        fatalStruct, TEXT("bIsSet"))) : nullptr;
                if (fatalProperty && isSetProperty)
                {
                    auto* fatalData = fatalProperty->ContainerPtrToValuePtr<void>(damage);
                    dead = isSetProperty->GetPropertyValue(
                        isSetProperty->ContainerPtrToValuePtr<void>(fatalData));
                }
            }

            auto active = m_activeNameplateStates.find(pawn);
            if (dead)
            {
                if (deadRule->InactivitySeconds > 0.0)
                    m_activeNameplateStates.insert_or_assign(pawn,
                        ActiveNameplateState{FWeakObjectPtr(pawn), "Dead",
                            deadRule->InactivitySeconds});
            }
            else if (active != m_activeNameplateStates.end()
                && active->second.Actor.Get() == pawn
                && active->second.State == "Dead"
                && active->second.RemainingSeconds > 0.0)
            {
                dead = true;
            }
            else if (active != m_activeNameplateStates.end()
                && active->second.State == "Dead")
            {
                m_activeNameplateStates.erase(active);
            }

            const PlayerNameplateStateRule* effectiveState = dead ? deadRule : nullptr;
            std::string effectiveStateName = dead ? "Dead" : std::string{};
            active = m_activeNameplateStates.find(pawn);
            if (!dead && active != m_activeNameplateStates.end()
                && active->second.Actor.Get() == pawn)
            {
                if (const auto* activityRule = stateRule(active->second.State))
                {
                    if (active->second.RemainingSeconds < 0.0)
                        active->second.RemainingSeconds = activityRule->InactivitySeconds;
                    if (active->second.RemainingSeconds > 0.0)
                    {
                        effectiveState = activityRule;
                        effectiveStateName = active->second.State;
                    }
                    else
                    {
                        m_activeNameplateStates.erase(active);
                    }
                }
                else
                {
                    m_activeNameplateStates.erase(active);
                }
            }

            const std::string effectiveMode = effectiveState ? "Icon" : rule.Mode;
            const std::string& effectiveIcon = effectiveState
                ? effectiveState->Icon : rule.Icon;
            const double effectiveScale = effectiveState
                ? effectiveState->Scale : rule.Scale;
            auto* nameplate = ActorHelper::GetObjectRef(
                pawn, TEXT("BP_Player_Nameplate"));
            if (!nameplate)
                throw std::runtime_error("BP_Player_Nameplate was unavailable");

            const bool hidden = effectiveMode == "Hidden";
            if (auto* property = PropertyHelper::GetPropertyByName(
                    nameplate->GetClassPrivate(), TEXT("bHiddenInGame")))
                PropertyHelper::CopyJsonValueToContainer(nameplate, property, hidden);
            if (auto* property = PropertyHelper::GetPropertyByName(
                    nameplate->GetClassPrivate(), TEXT("DistanceFromPlayerToShow")))
                PropertyHelper::CopyJsonValueToContainer(
                    nameplate, property, rule.Distance);
            if (rule.ShowSelf)
            {
                if (auto* property = PropertyHelper::GetPropertyByName(
                        nameplate->GetClassPrivate(), TEXT("bOwnerNoSee")))
                    PropertyHelper::CopyJsonValueToContainer(nameplate, property, false);
            }

            try
            {
                ActorHelper::FunctionCall(nameplate,
                    STR("/Script/Engine.SceneComponent:SetVisibility"))
                    .Arg(TEXT("bNewVisibility"), !hidden)
                    .Arg(TEXT("bPropagateToChildren"), true).Invoke();
            }
            catch (...) {}

            auto getWidget = ActorHelper::FunctionCall(nameplate,
                STR("/Script/UMG.WidgetComponent:GetUserWidgetObject"));
            getWidget.Invoke();
            auto* widget = getWidget.Result<UObject*>();
            if (!widget)
                throw std::runtime_error("the player nameplate widget was not initialized");
            if (rule.ShowSelf)
                ActorHelper::FunctionCall(widget,
                    STR("/Script/UMG.Widget:SetVisibility"))
                    .Arg(TEXT("InVisibility"), static_cast<uint8>(4)).Invoke();
            const auto signature = effectiveMode + "|" + effectiveIcon + "|"
                + std::to_string(effectiveScale) + "|" + std::to_string(rule.Distance)
                + "|" + (rule.ShowSelf ? "self" : "no-self")
                + "|" + (rule.ShowOthers ? "others" : "no-others")
                + "|centered-square-v3";
            if (const auto applied = m_nameplateAppliedActors.find(pawn);
                applied != m_nameplateAppliedActors.end()
                && applied->second.Actor.Get() == widget
                && applied->second.Signature == signature)
                return true;
            auto* text = ActorHelper::GetObjectRef(
                widget, TEXT("PlayerNameTextBlock"));
            if (!text)
                throw std::runtime_error("PlayerNameTextBlock was unavailable");

            const auto setVisibility = [](UObject* target, uint8 visibility) {
                if (!target) return;
                ActorHelper::FunctionCall(target,
                    STR("/Script/UMG.Widget:SetVisibility"))
                    .Arg(TEXT("InVisibility"), visibility).Invoke();
            };

            auto* widgetTree = ActorHelper::GetObjectRef(widget, TEXT("WidgetTree"));
            UObject* icon = nullptr;
            UObject* iconSlot = nullptr;
            if (widgetTree)
            {
                const auto iconPath = widgetTree->GetPathName()
                    + TEXT(".RuneSchemaNameplateIcon");
                icon = UECustom::UObjectGlobals::StaticFindObject(
                    nullptr, nullptr, iconPath.c_str(), false);
                if (icon)
                    iconSlot = ActorHelper::GetObjectRef(icon, TEXT("Slot"));
            }

            if (effectiveMode != "Icon")
            {
                setVisibility(text, hidden ? 1 : 4);
                if (icon) setVisibility(icon, 1);
                m_nameplateAppliedActors.insert_or_assign(pawn,
                    AppliedVisual{FWeakObjectPtr(widget), signature});
                return true;
            }

            auto* texture = ActorHelper::ResolveObject(
                RC::to_generic_string(effectiveIcon));
            if (!texture)
                throw std::runtime_error("the cooked Nameplate.Icon texture could not be loaded");
            if (!widgetTree)
                throw std::runtime_error("the player nameplate WidgetTree was unavailable");

            if (!icon)
            {
                auto* imageClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                    nullptr, nullptr, TEXT("/Script/UMG.Image"), false);
                auto* root = ActorHelper::GetObjectRef(widgetTree, TEXT("RootWidget"));
                if (!imageClass || !root)
                    throw std::runtime_error("UMG Image or the nameplate root panel was unavailable");

                FStaticConstructObjectParameters params(imageClass, widgetTree);
                params.Name = FName(TEXT("RuneSchemaNameplateIcon"), FNAME_Add);
                params.SetFlags = static_cast<EObjectFlags>(RF_Transactional);
                icon = UObjectGlobals::StaticConstructObject<UObject*>(params);
                if (!icon)
                    throw std::runtime_error("failed to construct the nameplate icon widget");

                auto addChild = ActorHelper::FunctionCall(root,
                    STR("/Script/UMG.PanelWidget:AddChild"));
                addChild.Arg(TEXT("Content"), icon).Invoke();
                iconSlot = addChild.Result<UObject*>();
                if (!iconSlot)
                    throw std::runtime_error("failed to add the nameplate icon to its canvas");
            }

            if (!iconSlot)
                throw std::runtime_error("the nameplate icon canvas slot was unavailable");
            if (auto* layout = PropertyHelper::GetPropertyByName(
                    iconSlot->GetClassPrivate(), TEXT("LayoutData")))
            {
                PropertyHelper::CopyJsonValueToContainer(iconSlot, layout,
                    nlohmann::json{
                        {"Offsets", {{"Left", 250.0}, {"Top", 25.0},
                            {"Right", 64.0}, {"Bottom", 64.0}}},
                        {"Anchors", {
                            {"Minimum", {{"X", 0.0}, {"Y", 0.0}}},
                            {"Maximum", {{"X", 0.0}, {"Y", 0.0}}}}},
                        {"Alignment", {{"X", 0.5}, {"Y", 0.5}}}
                    });
            }
            if (auto* autoSize = PropertyHelper::GetPropertyByName(
                    iconSlot->GetClassPrivate(), TEXT("bAutoSize")))
                PropertyHelper::CopyJsonValueToContainer(iconSlot, autoSize, false);

            // Canvas slot field writes alone do not always invalidate Slate's cached
            // layout. Use the public UMG setters as well so the live slot is centered.
            ActorHelper::FunctionCall(iconSlot,
                STR("/Script/UMG.CanvasPanelSlot:SetPosition"))
                .Arg(TEXT("Position"), FVector2D(250.0, 25.0)).Invoke();
            ActorHelper::FunctionCall(iconSlot,
                STR("/Script/UMG.CanvasPanelSlot:SetSize"))
                .Arg(TEXT("Size"), FVector2D(64.0, 64.0)).Invoke();
            ActorHelper::FunctionCall(iconSlot,
                STR("/Script/UMG.CanvasPanelSlot:SetAlignment"))
                .Arg(TEXT("InAlignment"), FVector2D(0.5, 0.5)).Invoke();
            ActorHelper::FunctionCall(iconSlot,
                STR("/Script/UMG.CanvasPanelSlot:SetAutoSize"))
                .Arg(TEXT("InbAutoSize"), false).Invoke();

            auto setBrush = ActorHelper::FunctionCall(icon,
                STR("/Script/UMG.Image:SetBrushFromTexture"));
            setBrush.Arg(TEXT("Texture"), texture)
                .Arg(TEXT("bMatchSize"), false).Invoke();
            auto* renderTransform = PropertyHelper::GetPropertyByName(
                icon->GetClassPrivate(), TEXT("RenderTransform"));
            if (!renderTransform)
                throw std::runtime_error("the nameplate icon RenderTransform was unavailable");
            PropertyHelper::CopyJsonValueToContainer(icon, renderTransform,
                nlohmann::json{
                    {"Translation", {{"X", 0.0}, {"Y", 0.0}}},
                    {"Scale", {{"X", effectiveScale}, {"Y", effectiveScale}}},
                    {"Shear", {{"X", 0.0}, {"Y", 0.0}}},
                    {"Angle", 0.0}
                });
            setVisibility(text, 1);
            setVisibility(icon, 4);
            m_nameplateAppliedActors.insert_or_assign(pawn,
                AppliedVisual{FWeakObjectPtr(widget), signature});
            if (m_nameplateAppliedActors.size() > 128)
                std::erase_if(m_nameplateAppliedActors,
                    [](const auto& value) { return !value.second.Actor.Get(); });
            const auto reportKey = "nameplate-applied\n" + RC::to_string(context)
                + "\n" + (effectiveState ? effectiveStateName + "\n" : "default\n")
                + effectiveIcon;
            if (m_reportedPlayerRuleApplications.insert(reportKey).second)
                PS::RoutineLog("players", STR("{} applied {} icon nameplate '{}'.\n"),
                    context, effectiveState
                        ? RC::to_generic_string(effectiveStateName) : TEXT("default"),
                    RC::to_generic_string(effectiveIcon));
            return true;
        }
        catch (const std::exception& error)
        {
            const auto diagnosticKey = "nameplate\n" + RC::to_string(context)
                + "\n" + error.what();
            if (m_reportedPlayerRuleFailures.insert(diagnosticKey).second)
                PS::Log<LogLevel::Warning>(STR("{} nameplate skipped safely: {}.\n"),
                    context, PS::ToWideSafe(error.what()));
            return false;
        }
    }

    UObject* DragonWildsSpawnLoader::ResolvePlayerPawnFromActivity(UObject* source)
    {
        if (!source) return nullptr;
        auto* playerClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Dominion.DominionPlayerCharacter"));
        if (!playerClass) return nullptr;
        for (auto* candidate = source; candidate; candidate = candidate->GetOuterPrivate())
        {
            if (candidate->IsA(playerClass)) return candidate;
        }
        return nullptr;
    }

    std::string DragonWildsSpawnLoader::ClassifyPlayerAttackActivity(UObject* source)
    {
        if (!source) return {};
        auto sourcePath = RC::to_string(source->GetPathName());
        std::transform(sourcePath.begin(), sourcePath.end(), sourcePath.begin(),
            [](unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
        if (sourcePath.contains("rangedattack")) return "Ranged";

        std::string actionPath;
        auto* currentAttack = CastField<FStructProperty>(
            PropertyHelper::GetPropertyByName(
                source->GetClassPrivate(), TEXT("CurrentAttack")));
        auto* currentStruct = currentAttack ? currentAttack->GetStruct().Get() : nullptr;
        auto* actionInstance = currentStruct ? CastField<FStructProperty>(
            PropertyHelper::GetPropertyByName(
                currentStruct, TEXT("ActionInstance"))) : nullptr;
        auto* actionStruct = actionInstance ? actionInstance->GetStruct().Get() : nullptr;
        auto* dataProperty = actionStruct ? CastField<FObjectPropertyBase>(
            PropertyHelper::GetPropertyByName(actionStruct, TEXT("Data"))) : nullptr;
        if (currentAttack && actionInstance && dataProperty)
        {
            auto* currentData = currentAttack->ContainerPtrToValuePtr<void>(source);
            auto* actionData = actionInstance->ContainerPtrToValuePtr<void>(currentData);
            if (auto* data = dataProperty->GetObjectPropertyValue(
                    dataProperty->ContainerPtrToValuePtr<void>(actionData)))
                actionPath = RC::to_string(data->GetPathName());
        }
        std::transform(actionPath.begin(), actionPath.end(), actionPath.begin(),
            [](unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
        if (actionPath.contains("pickaxe") || actionPath.contains("_mine"))
            return "Mining";
        if (actionPath.contains("hatchet") || actionPath.contains("woodcut"))
            return "Woodcutting";
        return "Attack";
    }

    void DragonWildsSpawnLoader::SetPlayerNameplateActivity(UObject* source,
        const std::string& state)
    {
        auto* pawn = ResolvePlayerPawnFromActivity(source);
        if (!pawn || pawn->GetWorld() != m_readyWorld) return;
        m_activeNameplateStates.insert_or_assign(pawn,
            ActiveNameplateState{FWeakObjectPtr(pawn), state, -1.0});
        ApplyClientPlayerVisualRules(pawn, true);
    }

    void DragonWildsSpawnLoader::RefreshPlayerNameplates(double deltaSeconds)
    {
        const bool hasNameplates = std::any_of(m_playerRules.begin(), m_playerRules.end(),
            [](const PlayerRule& rule) { return rule.Nameplate.Configured; });
        if (!hasNameplates) return;

        std::vector<UObject*> expired;
        for (auto iterator = m_activeNameplateStates.begin();
            iterator != m_activeNameplateStates.end();)
        {
            if (!iterator->second.Actor.Get())
            {
                iterator = m_activeNameplateStates.erase(iterator);
                continue;
            }
            if (iterator->second.RemainingSeconds < 0.0)
            {
                ++iterator;
                continue;
            }
            iterator->second.RemainingSeconds -= std::max(0.0, deltaSeconds);
            if (iterator->second.RemainingSeconds <= 0.0)
            {
                expired.push_back(iterator->second.Actor.Get());
                iterator = m_activeNameplateStates.erase(iterator);
                continue;
            }
            ++iterator;
        }
        for (auto* player : expired)
            if (player && player->GetWorld() == m_readyWorld)
                ApplyClientPlayerVisualRules(player, true);
    }

    void DragonWildsSpawnLoader::ApplyPlayerRules()
    {
        if ((m_playerRules.empty() && !PlayerGhost::HasItemRules()) || !IsWorldStillLoaded(m_readyWorld)) return;
        LoadAppearanceProvenance();
        std::unordered_map<std::string, std::string> activeAppearanceOwners;
        const auto connected = GetConnectedPlayerNames();
        const auto connectedInLoadOrder = GetConnectedPlayersInLoadOrder();
        for (const auto& rule : m_playerRules)
        {
            struct PlayerRuleTarget {
                std::string Name;
                std::string Guid;
            };
            std::vector<PlayerRuleTarget> targets;
            if (rule.AllPlayers)
            {
                for (const auto& name : connected) targets.push_back({name, {}});
            }
            else if (!rule.PlayerGuids.empty())
            {
                // GUID selectors take precedence over names.
                for (const auto& guid : rule.PlayerGuids)
                {
                    bool ambiguous = false;
                    if (auto* controller = FindPlayerControllerByGuid(guid, ambiguous))
                    {
                        auto name = GetPlayerControllerName(controller);
                        if (std::none_of(targets.begin(), targets.end(), [&](const auto& target) {
                            return target.Guid == guid;
                        })) targets.push_back({std::move(name), guid});
                    }
                }
            }
            else if (!rule.PlayerLoadSlots.empty())
            {
                for (const auto slot : rule.PlayerLoadSlots)
                {
                    if (slot > connectedInLoadOrder.size())
                    {
                        const auto key = RC::to_string(rule.ModName) + "\n*"
                            + std::to_string(slot);
                        if (m_reportedPlayerRuleFailures.insert(key).second)
                            PS::Log<LogLevel::Warning>(
                                STR("Player rule from '{}' targets *{}, but that player has not loaded in this world yet.\n"),
                                rule.ModName, slot);
                        continue;
                    }
                    const auto& player = connectedInLoadOrder.at(slot - 1);
                    if (std::none_of(targets.begin(), targets.end(), [&](const auto& target) {
                        return (!player.Guid.empty() && target.Guid == player.Guid)
                            || (player.Guid.empty() && target.Name == player.Name);
                    })) targets.push_back({player.Name, player.Guid});
                }
            }
            else
            {
                for (const auto& name : rule.PlayerNames) targets.push_back({name, {}});
            }
            for (const auto& target : targets)
            {
                bool targetAmbiguous = false;
                auto* targetController = !target.Guid.empty()
                    ? FindPlayerControllerByGuid(target.Guid, targetAmbiguous)
                    : FindPlayerControllerByName(RC::to_generic_string(target.Name), targetAmbiguous);
                const auto actualGuid = targetController
                    ? GetPlayerCharacterGuid(targetController) : target.Guid;
                if (!actualGuid.empty())
                {
                    for (const auto& appearance : rule.Appearance)
                        activeAppearanceOwners[actualGuid + "\n" + appearance.Field]
                            = RC::to_string(rule.ModName);
                }
                std::string ignored;
                const bool applied = AdjustRuntimePlayerRule(
                    target.Name, rule, ignored, target.Guid);
                if (!applied)
                {
                    const auto label = target.Name.empty() ? target.Guid : target.Name;
                    const auto key = RC::to_string(rule.ModName) + "\n" + label
                        + "\n" + target.Guid + "\n" + ignored;
                    if (m_reportedPlayerRuleFailures.insert(key).second)
                    {
                        PS::Log<LogLevel::Error>(
                            STR("Player rule from '{}' for '{}' failed safely: {}\n"),
                            rule.ModName, PS::ToWideSafe(label.c_str()), PS::ToWideSafe(ignored.c_str()));
                    }
                }
                else
                {
                    const auto label = target.Name.empty() ? target.Guid : target.Name;
                    const auto auditGuid = actualGuid.empty() ? target.Guid : actualGuid;
                    const auto key = RC::to_string(rule.ModName) + "\n" + label
                        + "\n" + auditGuid + "\n" + ignored;
                    if (m_reportedPlayerRuleApplications.insert(key).second)
                    {
                        PS::RoutineLog("players",
                            STR("Applied player rule from '{}' to '{}' (GUID {}): {}\n"),
                            rule.ModName, PS::ToWideSafe(label.c_str()),
                            PS::ToWideSafe(auditGuid.c_str()), PS::ToWideSafe(ignored.c_str()));
                    }
                }
            }
        }
        ReconcileAppearanceFallbacks(activeAppearanceOwners);
    }

    void DragonWildsSpawnLoader::ReconcileAppearanceFallbacks(
        const std::unordered_map<std::string, std::string>& activeOwners)
    {
        bool stateChanged = false;
        for (auto record = m_appearanceProvenance.begin();
            record != m_appearanceProvenance.end();)
        {
            const auto active = activeOwners.find(record->PlayerGuid + "\n" + record->Field);
            if (active != activeOwners.end() && active->second == record->OwnerMod)
            {
                ++record;
                continue;
            }

            bool ambiguous = false;
            auto* controller = FindPlayerControllerByGuid(record->PlayerGuid, ambiguous);
            if (!controller)
            {
                ++record;
                continue;
            }
            try
            {
                auto pawnCall = ActorHelper::FunctionCall(
                    controller, STR("/Script/Engine.Controller:K2_GetPawn"));
                pawnCall.Invoke();
                auto* pawn = pawnCall.Result<UObject*>();
                bool changed = false;
                UObject* customization = nullptr;
                std::string error;
                if (!WritePlayerAppearance(pawn, record->Field,
                    record->FallbackDataTablePath, record->FallbackRowName,
                    changed, &customization, error))
                {
                    PS::Log<LogLevel::Error>(
                        STR("Appearance fallback for player {} field {} failed safely: {}\n"),
                        PS::ToWideSafe(record->PlayerGuid.c_str()),
                        PS::ToWideSafe(record->Field.c_str()), PS::ToWideSafe(error.c_str()));
                    ++record;
                    continue;
                }
                if (changed && customization)
                {
                    auto refresh = ActorHelper::FunctionCall(customization,
                        STR("/Script/Dominion.PlayerCustomizationComponent:OnRep_PlayerCustomization"));
                    refresh.Invoke();
                }
                PS::RoutineLog("players",
                    STR("Restored safe appearance fallback for player {} field {} after mod '{}' became inactive.\n"),
                    PS::ToWideSafe(record->PlayerGuid.c_str()),
                    PS::ToWideSafe(record->Field.c_str()), PS::ToWideSafe(record->OwnerMod.c_str()));
                record = m_appearanceProvenance.erase(record);
                stateChanged = true;
            }
            catch (const std::exception& error)
            {
                PS::Log<LogLevel::Error>(
                    STR("Appearance fallback failed safely: {}\n"), PS::ToWideSafe(error.what()));
                ++record;
            }
            catch (...)
            {
                ++record;
            }
        }
        if (stateChanged)
        {
            std::string error;
            if (!SaveAppearanceProvenance(error))
                PS::Log<LogLevel::Error>(STR("Could not save appearance fallback state: {}\n"),
                    PS::ToWideSafe(error.c_str()));
        }
    }

    bool DragonWildsSpawnLoader::AdjustRuntimePlayerRule(
        const std::string& targetPlayerNameUtf8,
        const PlayerRule& rule,
        std::string& result,
        const std::string& targetPlayerGuid)
    {
        try
        {
            const bool setScale = rule.SetScale;
            const double scaleMultiplier = rule.ScaleMultiplier;
            const bool setHealth = rule.SetHealth;
            const double healthMultiplier = rule.HealthMultiplier;
            const bool setMaxHealth = rule.SetMaxHealth;
            const double maxHealth = rule.MaxHealth;
            const bool setDefense = rule.SetDefense;
            const double defenseMultiplier = rule.DefenseMultiplier;
            const bool setDamage = rule.SetDamage;
            const double damageMultiplier = rule.DamageMultiplier;
            const bool setStamina = rule.SetStamina;
            const double staminaMultiplier = rule.StaminaMultiplier;
            const bool setMaxStamina = rule.SetMaxStamina;
            const double maxStamina = rule.MaxStamina;
            if (!setScale && !setHealth && !setMaxHealth && !setDefense && !setDamage
                && !setStamina && !setMaxStamina
                && !rule.SetWalkSpeed && !rule.SetRunSpeed
                && !rule.SetCarryWeight && !rule.SetMaxCarryWeight
                && !rule.SetPoisonResistance && !rule.SetStaminaRecovery
                && !rule.SetPhysicalAttack && !rule.SetMagicalAttack
                && !rule.SetRangedAttack && !rule.SetPhysicalDefense
                && !rule.SetMagicalDefense && !rule.SetRangedDefense
                && rule.AttributeMultipliers.empty() && rule.Attributes.empty()
                && rule.Appearance.empty() && rule.VisualEffect.empty()
                && !rule.Nameplate.Configured)
            {
                result = "no player adjustment was requested";
                return false;
            }
            if ((setScale && (!std::isfinite(scaleMultiplier) || scaleMultiplier < 0.25 || scaleMultiplier > 3.0))
                || (setHealth && (!std::isfinite(healthMultiplier) || healthMultiplier < 0.1 || healthMultiplier > 100.0))
                || (setMaxHealth && (!std::isfinite(maxHealth) || maxHealth < 1.0 || maxHealth > 1000000.0))
                || (setDefense && (!std::isfinite(defenseMultiplier) || defenseMultiplier < 0.1 || defenseMultiplier > 100.0))
                || (setDamage && (!std::isfinite(damageMultiplier) || damageMultiplier < 0.1 || damageMultiplier > 100.0))
                || (setStamina && (!std::isfinite(staminaMultiplier) || staminaMultiplier < 0.1 || staminaMultiplier > 100.0))
                || (setMaxStamina && (!std::isfinite(maxStamina)
                    || maxStamina < 1.0 || maxStamina > 1000000.0))
                || (rule.SetWalkSpeed && (!std::isfinite(rule.WalkSpeedMultiplier)
                    || rule.WalkSpeedMultiplier < 0.1 || rule.WalkSpeedMultiplier > 10.0))
                || (rule.SetRunSpeed && (!std::isfinite(rule.RunSpeedMultiplier)
                    || rule.RunSpeedMultiplier < 0.1 || rule.RunSpeedMultiplier > 10.0))
                || (rule.SetCarryWeight && (!std::isfinite(rule.CarryWeightMultiplier)
                    || rule.CarryWeightMultiplier < 0.1
                    || rule.CarryWeightMultiplier > 100.0))
                || (rule.SetMaxCarryWeight && (!std::isfinite(rule.MaxCarryWeight)
                    || rule.MaxCarryWeight < 1.0 || rule.MaxCarryWeight > 1000000.0)))
            {
                result = "scale must be 0.25-3, health/defense/damage/stamina multipliers must be 0.1-100, and absolute health/stamina must be 1-1000000";
                return false;
            }
            if (setHealth && setMaxHealth)
            {
                result = "health multiplier and absolute max health cannot be combined";
                return false;
            }
            if (setStamina && setMaxStamina)
            {
                result = "stamina multiplier and absolute max stamina cannot be combined";
                return false;
            }
            if (rule.SetCarryWeight && rule.SetMaxCarryWeight)
            {
                result = "carry-weight multiplier and absolute max carry weight cannot be combined";
                return false;
            }
            if (!IsWorldStillLoaded(m_readyWorld) || !GetGameMode(m_readyWorld))
            {
                result = "an authoritative game world is not ready";
                return false;
            }

            bool ambiguous = false;
            auto* controller = !targetPlayerGuid.empty()
                ? FindPlayerControllerByGuid(targetPlayerGuid, ambiguous)
                : targetPlayerNameUtf8.empty()
                ? FindLocalPlayerController()
                : FindPlayerControllerByName(RC::to_generic_string(targetPlayerNameUtf8), ambiguous);
            if (!controller)
            {
                result = ambiguous ? "player name matched more than one connected player"
                    : targetPlayerNameUtf8.empty() && targetPlayerGuid.empty()
                    ? "no local player exists; a headless server must specify 'player <name>'"
                    : !targetPlayerGuid.empty()
                    ? "no connected player matched that character GUID"
                    : "no connected player matched that name";
                return false;
            }
            const auto resolvedPlayerGuid = GetPlayerCharacterGuid(controller);

            auto pawnCall = ActorHelper::FunctionCall(
                controller, STR("/Script/Engine.Controller:K2_GetPawn"));
            pawnCall.Invoke();
            auto* pawn = pawnCall.Result<UObject*>();
            if (!pawn || pawn->GetWorld() != m_readyWorld)
            {
                result = "the selected player has no active pawn";
                return false;
            }
            const bool visualEffectApplied = rule.VisualEffect.empty()
                || ApplyVisualEffect(pawn, rule.VisualEffect,
                    STR("Player rule from '") + rule.ModName + STR("'"));
            bool nameplateApplied = !rule.Nameplate.Configured;
            bool nameplateDeferred = false;
            if (rule.Nameplate.Configured)
            {
                if (GhostMaterials::CanRender(pawn))
                    nameplateApplied = ApplyPlayerNameplate(pawn, rule.Nameplate,
                        STR("Player rule from '") + rule.ModName + STR("'"));
                else
                {
                    // Dedicated servers retain the rule; each client applies it
                    // from ClientRestart or OnRep_PlayerState when its widget exists.
                    nameplateApplied = true;
                    nameplateDeferred = true;
                }
            }

            auto state = std::find_if(m_playerAdjustments.begin(), m_playerAdjustments.end(),
                [&](const PlayerAdjustmentState& value) { return value.Pawn == pawn; });
            if (state == m_playerAdjustments.end())
            {
                PlayerAdjustmentState initial;
                initial.Pawn = pawn;
                m_playerAdjustments.push_back(std::move(initial));
                state = std::prev(m_playerAdjustments.end());
            }

            UObject* playerAttributes = nullptr;
            try
            {
                auto getAttributes = ActorHelper::FunctionCall(
                    pawn, STR("/Script/Dominion.DominionPlayerCharacter:GetPlayerAttributesComponent"));
                getAttributes.Invoke();
                playerAttributes = getAttributes.Result<UObject*>();
            }
            catch (...)
            {
                auto* address = PropertyHelper::GetValuePtrByPropertyNameInChain<
                    TObjectPtr<UObject>>(pawn, TEXT("AttributesComponent"));
                playerAttributes = address ? address->Get() : nullptr;
            }

            const auto captureAttribute = [&](PlayerAttributeBaseline& baseline,
                UObject* attributes,
                const std::function<bool(const std::string&)>& matches) {
                if (baseline.Valid || !attributes) return;
                const auto captureFromPair = [&](const TCHAR* attributesName,
                    const TCHAR* valuesName) {
                    auto* attributesProperty = CastField<FArrayProperty>(
                        PropertyHelper::GetPropertyByName(
                            attributes->GetClassPrivate(), attributesName));
                    auto* valuesProperty = CastField<FArrayProperty>(
                        PropertyHelper::GetPropertyByName(
                            attributes->GetClassPrivate(), valuesName));
                    auto* valueInner = valuesProperty
                        ? CastField<FNumericProperty>(valuesProperty->GetInner()) : nullptr;
                    auto* attributeArray = attributesProperty
                        ? attributesProperty->ContainerPtrToValuePtr<FScriptArray>(attributes) : nullptr;
                    auto* valueArray = valuesProperty
                        ? valuesProperty->ContainerPtrToValuePtr<FScriptArray>(attributes) : nullptr;
                    if (!attributesProperty || !valuesProperty || !valueInner
                        || !valueInner->IsFloatingPoint() || !attributeArray || !valueArray
                        || attributeArray->Num() != valueArray->Num()) return;
                    const int32 attributeSize = attributesProperty->GetInner()->GetElementSize();
                    const int32 valueSize = valueInner->GetElementSize();
                    auto* attributeData = static_cast<uint8*>(attributeArray->GetData());
                    auto* valueData = static_cast<uint8*>(valueArray->GetData());
                    for (int32 index = 0; index < attributeArray->Num(); ++index)
                    {
                        UObject* attribute = nullptr;
                        std::memcpy(&attribute,
                            attributeData + index * attributeSize, sizeof(attribute));
                        if (!attribute) continue;
                        std::array<std::string, 2> names{
                            RC::to_string(attribute->GetName()),
                            attribute->GetClassPrivate()
                                ? RC::to_string(attribute->GetClassPrivate()->GetName())
                                : std::string{},
                        };
                        const bool matched = std::any_of(names.begin(), names.end(),
                            [&](std::string name) {
                                std::transform(name.begin(), name.end(), name.begin(),
                                    [](unsigned char character) {
                                        return static_cast<char>(std::tolower(character));
                                    });
                                return matches(name);
                            });
                        if (!matched) continue;
                        baseline.Attributes = attributes;
                        baseline.ValuesProperty = valuesProperty;
                        baseline.Attribute = attribute;
                        baseline.Index = index;
                        baseline.Value = valueInner->GetFloatingPointPropertyValue(
                            valueData + index * valueSize);
                        baseline.Valid = std::isfinite(baseline.Value);
                        return;
                    }
                };

                captureFromPair(TEXT("FloatAttributes"), TEXT("AttributeValues"));
                if (!baseline.Valid)
                    captureFromPair(TEXT("SharedFloatAttributes"),
                        TEXT("SharedAttributeValues"));
            };
            const auto applyAttributeValue = [&](PlayerAttributeBaseline& baseline, double value) {
                if (!baseline.Valid || !baseline.Attributes || !baseline.ValuesProperty
                    || !baseline.Attribute || baseline.Index < 0 || !std::isfinite(value))
                    return false;
                auto* numeric = CastField<FNumericProperty>(baseline.ValuesProperty->GetInner());
                auto* values = baseline.ValuesProperty->ContainerPtrToValuePtr<FScriptArray>(
                    baseline.Attributes);
                if (!numeric || !numeric->IsFloatingPoint() || !values
                    || !values->IsValidIndex(baseline.Index)) return false;
                auto* address = static_cast<uint8*>(values->GetData())
                    + baseline.Index * numeric->GetElementSize();
                numeric->SetFloatingPointPropertyValue(address, value);
                auto changed = ActorHelper::FunctionCall(
                    baseline.Attributes,
                    STR("/Script/Dominion.DominionAttributesComponent:OnAttributeChanged"));
                changed.Arg(STR("Attribute"), baseline.Attribute).Invoke();
                return true;
            };
            const auto applyAttribute = [&](PlayerAttributeBaseline& baseline, double multiplier) {
                return applyAttributeValue(baseline, baseline.Value * multiplier);
            };

            const auto normalizeAttributeIdentifier = [](std::string identifier) {
                return NormalizePlayerAttributeIdentifier(std::move(identifier));
            };

            bool scaleApplied = !setScale;
            bool healthApplied = !setHealth && !setMaxHealth;
            bool defenseApplied = !setDefense;
            bool damageApplied = !setDamage;
            bool staminaApplied = !setStamina && !setMaxStamina;
            bool walkSpeedApplied = !rule.SetWalkSpeed;
            bool runSpeedApplied = !rule.SetRunSpeed;
            bool carryWeightApplied = !rule.SetCarryWeight && !rule.SetMaxCarryWeight;
            bool poisonResistanceApplied = !rule.SetPoisonResistance;
            bool staminaRecoveryApplied = !rule.SetStaminaRecovery;
            bool physicalAttackApplied = !rule.SetPhysicalAttack;
            bool magicalAttackApplied = !rule.SetMagicalAttack;
            bool rangedAttackApplied = !rule.SetRangedAttack;
            bool physicalDefenseApplied = !rule.SetPhysicalDefense;
            bool magicalDefenseApplied = !rule.SetMagicalDefense;
            bool rangedDefenseApplied = !rule.SetRangedDefense;
            bool namedAttributesApplied = true;
            std::vector<std::string> unsupportedNamedAttributes;
            bool appearanceApplied = true;
            std::vector<std::string> unsupportedAppearance;

            if (setScale)
            {
                static_cast<AActor*>(pawn)->SetActorScale3D(FVector(
                    state->BaseScale.X() * scaleMultiplier,
                    state->BaseScale.Y() * scaleMultiplier,
                    state->BaseScale.Z() * scaleMultiplier));
                scaleApplied = true;
            }

            if (setHealth || setMaxHealth)
            {
                UObject* health = nullptr;
                std::vector<UObject*> healthCandidates;
                const auto addHealthCandidate = [&](UObject* candidate) {
                    if (candidate && std::find(healthCandidates.begin(),
                        healthCandidates.end(), candidate) == healthCandidates.end())
                        healthCandidates.push_back(candidate);
                };
                for (const auto* propertyName : {
                    TEXT("HealthComponent"), TEXT("BP_Components_Health")})
                {
                    auto* address = PropertyHelper::GetValuePtrByPropertyNameInChain<
                        TObjectPtr<UObject>>(pawn, propertyName);
                    addHealthCandidate(address ? address->Get() : nullptr);
                }
                try
                {
                    auto getPlayerHealth = ActorHelper::FunctionCall(
                        pawn,
                        STR("/Script/Dominion.DominionPlayerCharacter:GetHealthComponent"));
                    getPlayerHealth.Invoke();
                    addHealthCandidate(getPlayerHealth.Result<UObject*>());
                }
                catch (...)
                {
                }
                try
                {
                    auto getComponent = ActorHelper::FunctionCall(
                        pawn, STR("/Script/Dominion.HealthInterface:GetBaseHealthComponent"));
                    getComponent.Invoke();
                    addHealthCandidate(getComponent.Result<UObject*>());
                }
                catch (...)
                {
                }
                // Select a live health component with a positive maximum.
                for (auto* candidate : healthCandidates)
                {
                    try
                    {
                        auto getMax = ActorHelper::FunctionCall(
                            candidate, STR("/Script/Dominion.HealthComponent:GetMaxHealth"));
                        getMax.Invoke();
                        const double candidateMaximum = getMax.NumericResult();
                        if (!std::isfinite(candidateMaximum) || candidateMaximum <= 0.0)
                            continue;
                        health = candidate;
                        if (!state->HasBaseHealth)
                        {
                            state->BaseMaxHealth = candidateMaximum;
                            state->HasBaseHealth = true;
                        }
                        break;
                    }
                    catch (...)
                    {
                    }
                }
                if (!health)
                    PS::Log<LogLevel::Warning>(
                        STR("No authoritative positive player health component was available.\n"));
                // Use the public attribute API; private health delegates can crash.
                captureAttribute(state->HealthAttribute, playerAttributes,
                    [](const std::string& name) {
                        return name == "da_attribute_maxhealth"
                            || name == "maxhealthattribute"
                            || (name.find("health") != std::string::npos
                                && name.find("max") != std::string::npos);
                    });
                const double desired = setMaxHealth ? maxHealth
                    : state->HealthAttribute.Valid
                    ? state->HealthAttribute.Value * healthMultiplier
                    : state->HasBaseHealth ? state->BaseMaxHealth * healthMultiplier : 0.0;
                if (health)
                {
                    if (state->HasBaseHealth)
                    {
                        const double desiredMaximum = desired;
                        auto verifyMax = ActorHelper::FunctionCall(
                            health, STR("/Script/Dominion.HealthComponent:GetMaxHealth"));
                        verifyMax.Invoke();
                        double currentMaximum = verifyMax.NumericResult();
                        healthApplied = std::isfinite(currentMaximum)
                            && std::abs(currentMaximum - desiredMaximum) < 0.01;
                        if (!healthApplied && std::isfinite(currentMaximum) && currentMaximum > 0.0)
                        {
                            // ModifyMaxHealth takes an absolute maximum, not a delta.
                            auto modifyMax = ActorHelper::FunctionCall(
                                health, STR("/Script/Dominion.HealthComponent:ModifyMaxHealth"));
                            modifyMax.FirstNumericArg(desiredMaximum).Invoke();
                            auto verifyFallback = ActorHelper::FunctionCall(
                                health, STR("/Script/Dominion.HealthComponent:GetMaxHealth"));
                            verifyFallback.Invoke();
                            currentMaximum = verifyFallback.NumericResult();
                            healthApplied = std::isfinite(currentMaximum)
                                && std::abs(currentMaximum - desiredMaximum) < 0.01;
                            if (!healthApplied && state->HealthAttribute.Valid
                                && state->HealthAttribute.Value > 0.0)
                            {
                                // Update the live MaxHealthAttribute and notify its change.
                                const double absoluteMultiplier =
                                    desiredMaximum / state->HealthAttribute.Value;
                                if (std::isfinite(absoluteMultiplier)
                                    && applyAttribute(state->HealthAttribute,
                                        absoluteMultiplier))
                                {
                                    auto verifyAttribute = ActorHelper::FunctionCall(
                                        health,
                                        STR("/Script/Dominion.HealthComponent:GetMaxHealth"));
                                    verifyAttribute.Invoke();
                                    currentMaximum = verifyAttribute.NumericResult();
                                    healthApplied = std::isfinite(currentMaximum)
                                        && std::abs(currentMaximum - desiredMaximum) < 0.01;
                                }
                            }
                            if (!healthApplied)
                            {
                                try
                                {
                                    auto* effectClass = ActorHelper::ResolveClass(STR(
                                        "/Game/Gameplay/GameplayEffects/Effects/GE_ModifyMaxHealth.GE_ModifyMaxHealth_C"));
                                    auto effectObject = effectClass
                                        ? effectClass->GetClassDefaultObject() : TObjectPtr<UObject>{};
                                    auto* effect = effectObject.Get();
                                    auto* dataProperty = effect ? CastField<FStructProperty>(
                                        PropertyHelper::GetPropertyByName(
                                            effect->GetClassPrivate(), TEXT("Data"))) : nullptr;
                                    auto* dataStruct = dataProperty ? dataProperty->GetStruct().Get() : nullptr;
                                    auto* amountProperty = dataStruct ? CastField<FStructProperty>(
                                        PropertyHelper::GetPropertyByName(
                                            dataStruct, TEXT("AmountToModify"))) : nullptr;
                                    auto* amountStruct = amountProperty
                                        ? amountProperty->GetStruct().Get() : nullptr;
                                    auto* valueProperty = amountStruct ? CastField<FNumericProperty>(
                                        PropertyHelper::GetPropertyByName(
                                            amountStruct, TEXT("Value"))) : nullptr;
                                    if (!effect || !dataProperty || !amountProperty
                                        || !valueProperty || !valueProperty->IsFloatingPoint())
                                        throw std::runtime_error(
                                            "GE_ModifyMaxHealth magnitude was unavailable");
                                    auto* data = dataProperty->ContainerPtrToValuePtr<void>(effect);
                                    auto* amount = amountProperty->ContainerPtrToValuePtr<void>(data);
                                    auto* magnitude = valueProperty->ContainerPtrToValuePtr<void>(amount);
                                    const double originalMagnitude =
                                        valueProperty->GetFloatingPointPropertyValue(magnitude);
                                    const double delta = desiredMaximum - currentMaximum;
                                    valueProperty->SetFloatingPointPropertyValue(magnitude, delta);
                                    try
                                    {
                                        auto applyEffect = ActorHelper::FunctionCall(pawn,
                                            STR("/Script/Dominion.DominionPlayerCharacter:AddGameplayEffect"));
                                        applyEffect.Arg(STR("GameplayEffectData"), effect)
                                            .Arg(STR("EffectInstigator"), static_cast<AActor*>(pawn))
                                            .Arg(STR("EffectSource"), pawn)
                                            .Invoke();
                                    }
                                    catch (...)
                                    {
                                        valueProperty->SetFloatingPointPropertyValue(
                                            magnitude, originalMagnitude);
                                        throw;
                                    }
                                    valueProperty->SetFloatingPointPropertyValue(
                                        magnitude, originalMagnitude);
                                    auto verifyEffect = ActorHelper::FunctionCall(
                                        health, STR("/Script/Dominion.HealthComponent:GetMaxHealth"));
                                    verifyEffect.Invoke();
                                    currentMaximum = verifyEffect.NumericResult();
                                    healthApplied = std::isfinite(currentMaximum)
                                        && std::abs(currentMaximum - desiredMaximum) < 0.01;
                                    if (!healthApplied)
                                    {
                                        // Fallback preserves the native 24-byte effect handle.
                                        UObject* gameplayEffects = nullptr;
                                        try
                                        {
                                            auto getEffects = ActorHelper::FunctionCall(pawn,
                                                STR("/Script/Dominion.DominionPlayerCharacter:GetPlayerGameplayEffectsComponent"));
                                            getEffects.Invoke();
                                            gameplayEffects = getEffects.Result<UObject*>();
                                        }
                                        catch (...)
                                        {
                                            auto* address = PropertyHelper::GetValuePtrByPropertyNameInChain<
                                                TObjectPtr<UObject>>(pawn, TEXT("GameplayEffectsComponent"));
                                            gameplayEffects = address ? address->Get() : nullptr;
                                        }
                                        if (!gameplayEffects)
                                            throw std::runtime_error(
                                                "player gameplay-effects component was unavailable");

                                        valueProperty->SetFloatingPointPropertyValue(magnitude, delta);
                                        std::array<uint8_t, 24> effectHandle{};
                                        try
                                        {
                                            auto instantiate = ActorHelper::FunctionCall(gameplayEffects,
                                                STR("/Script/Dominion.DominionGameplayEffectsComponent:InstantiateGameplayEffect"));
                                            instantiate.Arg(STR("DataClass"), effectClass)
                                                .Arg(STR("InstigatingGE"), static_cast<UObject*>(nullptr))
                                                .Arg(STR("bForceNewInstance"), true)
                                                .Invoke();
                                            instantiate.MoveResult(effectHandle.data(), effectHandle.size());
                                        }
                                        catch (...)
                                        {
                                            valueProperty->SetFloatingPointPropertyValue(
                                                magnitude, originalMagnitude);
                                            throw;
                                        }
                                        valueProperty->SetFloatingPointPropertyValue(
                                            magnitude, originalMagnitude);
                                        auto apply = ActorHelper::FunctionCall(gameplayEffects,
                                            STR("/Script/Dominion.DominionGameplayEffectsComponent:ApplyGameplayEffect"));
                                        apply.Arg(STR("Instigator"), static_cast<AActor*>(pawn))
                                            .Arg(STR("Source"), pawn)
                                            .Arg(STR("Handle"), effectHandle)
                                            .Arg(STR("bIgnoreChanceToApply"), true)
                                            .Invoke();
                                        auto verifyNative = ActorHelper::FunctionCall(
                                            health, STR("/Script/Dominion.HealthComponent:GetMaxHealth"));
                                        verifyNative.Invoke();
                                        currentMaximum = verifyNative.NumericResult();
                                        healthApplied = std::isfinite(currentMaximum)
                                            && std::abs(currentMaximum - desiredMaximum) < 0.01;
                                    }
                                    if (!healthApplied)
                                    {
                                        const auto diagnosticKey = std::format(
                                            "max-health-effect\n{}\n{}\n{}",
                                            resolvedPlayerGuid, currentMaximum, desiredMaximum);
                                        if (m_reportedPlayerRuleFailures.insert(diagnosticKey).second)
                                            PS::Log<LogLevel::Warning>(
                                                STR("GE_ModifyMaxHealth dispatched but verification returned {} (requested {}).\n"),
                                                currentMaximum, desiredMaximum);
                                    }
                                }
                                catch (const std::exception& error)
                                {
                                    PS::Log<LogLevel::Warning>(
                                        STR("Max-health gameplay effect failed safely: {}\n"),
                                        PS::ToWideSafe(error.what()));
                                }
                            }
                        }
                        if (healthApplied)
                        {
                            auto setHealthCall = ActorHelper::FunctionCall(
                                health, STR("/Script/Dominion.HealthComponent:SetHealth"));
                            setHealthCall.FirstNumericArg(desiredMaximum).Invoke();
                        }
                    }
                }
            }

            if (setStamina || setMaxStamina)
            {
                auto getStamina = ActorHelper::FunctionCall(
                    pawn, STR("/Script/Dominion.DominionPlayerCharacter:GetStaminaComponent"));
                getStamina.Invoke();
                auto* stamina = getStamina.Result<UObject*>();
                if (stamina && !state->HasBaseStamina)
                {
                    auto getMaximum = ActorHelper::FunctionCall(
                        stamina, STR("/Script/Dominion.StaminaComponent:GetMaxStamina"));
                    getMaximum.Invoke();
                    const double baseMaximum = getMaximum.NumericResult();
                    auto* attributesAddress = PropertyHelper::GetValuePtrByPropertyNameInChain<
                        TObjectPtr<UObject>>(stamina, TEXT("AttributesComponent"));
                    auto* attributes = attributesAddress ? attributesAddress->Get() : nullptr;
                    auto* attributesProperty = attributes ? CastField<FArrayProperty>(
                        PropertyHelper::GetPropertyByName(
                            attributes->GetClassPrivate(), TEXT("FloatAttributes"))) : nullptr;
                    auto* valuesProperty = attributes ? CastField<FArrayProperty>(
                        PropertyHelper::GetPropertyByName(
                            attributes->GetClassPrivate(), TEXT("AttributeValues"))) : nullptr;
                    auto* attributeInner = attributesProperty
                        ? CastField<FObjectPropertyBase>(attributesProperty->GetInner()) : nullptr;
                    auto* valueInner = valuesProperty
                        ? CastField<FNumericProperty>(valuesProperty->GetInner()) : nullptr;
                    auto* attributeArray = attributesProperty
                        ? attributesProperty->ContainerPtrToValuePtr<FScriptArray>(attributes) : nullptr;
                    auto* valueArray = valuesProperty
                        ? valuesProperty->ContainerPtrToValuePtr<FScriptArray>(attributes) : nullptr;
                    if (attributes && attributesProperty && valuesProperty && attributeInner && valueInner
                        && valueInner->IsFloatingPoint() && attributeArray && valueArray
                        && attributeArray->Num() == valueArray->Num()
                        && std::isfinite(baseMaximum) && baseMaximum > 0.0)
                    {
                        const int32 attributeSize = attributesProperty->GetInner()->GetElementSize();
                        auto* attributeData = static_cast<uint8*>(attributeArray->GetData());
                        for (int32 index = 0; index < attributeArray->Num(); ++index)
                        {
                            UObject* attribute = nullptr;
                            std::memcpy(&attribute, attributeData + index * attributeSize, sizeof(attribute));
                            if (!attribute) continue;
                            std::array<std::string, 2> names{
                                RC::to_string(attribute->GetName()),
                                attribute->GetClassPrivate()
                                    ? RC::to_string(attribute->GetClassPrivate()->GetName())
                                    : std::string{},
                            };
                            const bool isMaximumStamina = std::any_of(
                                names.begin(), names.end(), [](std::string name) {
                                    std::transform(name.begin(), name.end(), name.begin(),
                                        [](unsigned char character) {
                                            return static_cast<char>(std::tolower(character));
                                        });
                                    return name.find("stamina") != std::string::npos
                                        && name.find("max") != std::string::npos;
                                });
                            if (!isMaximumStamina) continue;
                            state->StaminaAttributes = attributes;
                            state->StaminaValuesProperty = valuesProperty;
                            state->MaxStaminaAttribute = attribute;
                            state->MaxStaminaIndex = index;
                            state->BaseMaxStamina = baseMaximum;
                            state->HasBaseStamina = true;
                            break;
                        }
                    }
                }
                if (stamina && state->HasBaseStamina && state->StaminaAttributes
                    && state->StaminaValuesProperty && state->MaxStaminaAttribute
                    && state->MaxStaminaIndex >= 0)
                {
                    auto* valueInner = CastField<FNumericProperty>(
                        state->StaminaValuesProperty->GetInner());
                    auto* values = state->StaminaValuesProperty->ContainerPtrToValuePtr<FScriptArray>(
                        state->StaminaAttributes);
                    if (valueInner && valueInner->IsFloatingPoint() && values
                        && values->IsValidIndex(state->MaxStaminaIndex))
                    {
                        auto* valueAddress = static_cast<uint8*>(values->GetData())
                            + state->MaxStaminaIndex * valueInner->GetElementSize();
                        const double desired = setMaxStamina
                            ? maxStamina : state->BaseMaxStamina * staminaMultiplier;
                        valueInner->SetFloatingPointPropertyValue(valueAddress, desired);
                        auto changed = ActorHelper::FunctionCall(
                            state->StaminaAttributes,
                            STR("/Script/Dominion.DominionAttributesComponent:OnAttributeChanged"));
                        changed.Arg(STR("Attribute"), state->MaxStaminaAttribute).Invoke();
                        auto maximumChanged = ActorHelper::FunctionCall(
                            stamina, STR("/Script/Dominion.StaminaComponent:MaxStaminaChanged"));
                        maximumChanged.FirstNumericArg(desired).Invoke();
                        staminaApplied = true;
                    }
                }
            }

            const auto captureFields = [&](std::vector<PlayerNumericBaseline>& fields,
                const std::vector<const TCHAR*>& directNames,
                const std::vector<const TCHAR*>& inverseNames) {
                if (!fields.empty()) return;
                std::vector<UObject*> objects{pawn};
                for (auto* property = pawn->GetClassPrivate()->GetPropertyLink(); property;
                    property = property->GetPropertyLinkNext())
                {
                    auto* objectProperty = CastField<FObjectProperty>(property);
                    if (!objectProperty) continue;
                    auto* address = objectProperty->ContainerPtrToValuePtr<void>(pawn);
                    auto* object = address ? *reinterpret_cast<UObject**>(address) : nullptr;
                    if (object) objects.push_back(object);
                }
                for (auto* object : objects)
                {
                    const auto add = [&](const TCHAR* name, bool inverse) {
                        auto* numeric = CastField<FNumericProperty>(
                            PropertyHelper::GetPropertyByName(object->GetClassPrivate(), name));
                        if (!numeric || !numeric->IsFloatingPoint()) return;
                        auto* address = numeric->ContainerPtrToValuePtr<void>(object);
                        fields.push_back({object, numeric,
                            numeric->GetFloatingPointPropertyValue(address), inverse});
                    };
                    for (auto* name : directNames) add(name, false);
                    for (auto* name : inverseNames) add(name, true);
                }
            };
            const auto applyFields = [](std::vector<PlayerNumericBaseline>& fields, double multiplier) {
                for (auto& field : fields)
                {
                    auto* address = field.Property->ContainerPtrToValuePtr<void>(field.Object);
                    field.Property->SetFloatingPointPropertyValue(
                        address, field.Value * (field.Inverse ? 1.0 / multiplier : multiplier));
                }
                return !fields.empty();
            };

            if (setDamage)
            {
                captureAttribute(state->DamageAttribute, playerAttributes, [](const std::string& name) {
                    return name.find("damage") != std::string::npos
                        && name.find("taken") == std::string::npos
                        && (name.find("mult") != std::string::npos
                            || name.find("output") != std::string::npos
                            || name.find("attack") != std::string::npos);
                });
                captureFields(state->DamageFields,
                    {TEXT("DamageMultiplier"), TEXT("DamageScale"),
                     TEXT("OutgoingDamageMultiplier"), TEXT("AttackDamageMultiplier")}, {});
                damageApplied = applyAttribute(state->DamageAttribute, damageMultiplier)
                    || applyFields(state->DamageFields, damageMultiplier);
            }
            if (setDefense)
            {
                captureAttribute(state->DefenseAttribute, playerAttributes, [](const std::string& name) {
                    return name.find("defense") != std::string::npos
                        || name.find("defence") != std::string::npos
                        || name.find("armor") != std::string::npos
                        || name.find("armour") != std::string::npos;
                });
                captureFields(state->DefenseFields,
                    {TEXT("DefenseMultiplier"), TEXT("DefenceMultiplier"),
                     TEXT("ArmorMultiplier"), TEXT("ArmourMultiplier")},
                    {TEXT("DamageTakenMultiplier"), TEXT("IncomingDamageMultiplier")});
                defenseApplied = applyAttribute(state->DefenseAttribute, defenseMultiplier)
                    || applyFields(state->DefenseFields, defenseMultiplier);
            }

            if (rule.SetWalkSpeed)
            {
                captureFields(state->WalkSpeedFields,
                    {TEXT("MaxWalkSpeed")}, {});
                walkSpeedApplied = applyFields(
                    state->WalkSpeedFields, rule.WalkSpeedMultiplier);
            }
            if (rule.SetRunSpeed)
            {
                captureAttribute(state->RunSpeedAttribute, playerAttributes,
                    [](const std::string& name) {
                        return (name.find("run") != std::string::npos
                                || name.find("sprint") != std::string::npos)
                            && name.find("speed") != std::string::npos;
                    });
                captureFields(state->RunSpeedFields,
                    {TEXT("RunSpeed"), TEXT("SprintSpeed"),
                     TEXT("RunSpeedMultiplier"), TEXT("SprintSpeedMultiplier")}, {});
                runSpeedApplied = applyAttribute(
                    state->RunSpeedAttribute, rule.RunSpeedMultiplier)
                    || applyFields(state->RunSpeedFields, rule.RunSpeedMultiplier);
            }
            if (rule.SetCarryWeight || rule.SetMaxCarryWeight)
            {
                captureAttribute(state->CarryWeightAttribute, playerAttributes,
                    [&](const std::string& name) {
                        return NormalizePlayerAttributeIdentifier(name) == "carryweightmax";
                    });
                carryWeightApplied = rule.SetMaxCarryWeight
                    ? applyAttributeValue(
                        state->CarryWeightAttribute, rule.MaxCarryWeight)
                    : applyAttribute(
                        state->CarryWeightAttribute, rule.CarryWeightMultiplier);
            }
            if (rule.SetPoisonResistance)
            {
                captureAttribute(state->PoisonResistanceAttribute, playerAttributes,
                    [](const std::string& name) {
                        return name.find("poison") != std::string::npos
                            && (name.find("resist") != std::string::npos
                                || name.find("defense") != std::string::npos
                                || name.find("defence") != std::string::npos);
                    });
                poisonResistanceApplied = applyAttribute(
                    state->PoisonResistanceAttribute,
                    rule.PoisonResistanceMultiplier);
            }
            if (rule.SetStaminaRecovery)
            {
                auto getStamina = ActorHelper::FunctionCall(
                    pawn, STR("/Script/Dominion.DominionPlayerCharacter:GetStaminaComponent"));
                getStamina.Invoke();
                auto* stamina = getStamina.Result<UObject*>();
                auto* attributesAddress = stamina
                    ? PropertyHelper::GetValuePtrByPropertyNameInChain<TObjectPtr<UObject>>(
                        stamina, TEXT("AttributesComponent"))
                    : nullptr;
                auto* staminaAttributes = attributesAddress ? attributesAddress->Get() : nullptr;
                captureAttribute(state->StaminaRecoveryAttribute, staminaAttributes,
                    [](const std::string& name) {
                        return name.find("stamina") != std::string::npos
                            && (name.find("regen") != std::string::npos
                                || name.find("recovery") != std::string::npos
                                || name.find("recover") != std::string::npos);
                    });
                staminaRecoveryApplied = applyAttribute(
                    state->StaminaRecoveryAttribute,
                    rule.StaminaRecoveryMultiplier);
                if (staminaRecoveryApplied && stamina)
                {
                    auto resetRegen = ActorHelper::FunctionCall(
                        stamina, STR("/Script/Dominion.StaminaComponent:ResetRegen"));
                    resetRegen.Invoke();
                }
            }

            const auto attackMatcher = [](const std::string& name, const char* category) {
                const bool categoryMatch = std::string_view(category) == "physical"
                    ? name.find("physical") != std::string::npos
                        || name.find("melee") != std::string::npos
                    : std::string_view(category) == "magical"
                    ? name.find("magic") != std::string::npos
                    : name.find("range") != std::string::npos;
                return categoryMatch
                    && (name.find("attack") != std::string::npos
                        || name.find("damage") != std::string::npos)
                    && name.find("taken") == std::string::npos
                    && name.find("resist") == std::string::npos;
            };
            const auto defenseMatcher = [](const std::string& name, const char* category) {
                const bool categoryMatch = std::string_view(category) == "physical"
                    ? name.find("physical") != std::string::npos
                        || name.find("melee") != std::string::npos
                    : std::string_view(category) == "magical"
                    ? name.find("magic") != std::string::npos
                    : name.find("range") != std::string::npos;
                return categoryMatch
                    && (name.find("resist") != std::string::npos
                        || name.find("defense") != std::string::npos
                        || name.find("defence") != std::string::npos
                        || name.find("armor") != std::string::npos
                        || name.find("armour") != std::string::npos
                        || name.find("taken") != std::string::npos);
            };
            if (rule.SetPhysicalAttack)
            {
                captureAttribute(state->PhysicalAttackAttribute, playerAttributes,
                    [&](const std::string& name) { return attackMatcher(name, "physical"); });
                physicalAttackApplied = applyAttribute(
                    state->PhysicalAttackAttribute,
                    rule.PhysicalAttackMultiplier * (setDamage ? damageMultiplier : 1.0));
            }
            if (rule.SetMagicalAttack)
            {
                captureAttribute(state->MagicalAttackAttribute, playerAttributes,
                    [&](const std::string& name) { return attackMatcher(name, "magical"); });
                magicalAttackApplied = applyAttribute(
                    state->MagicalAttackAttribute,
                    rule.MagicalAttackMultiplier * (setDamage ? damageMultiplier : 1.0));
            }
            if (rule.SetRangedAttack)
            {
                captureAttribute(state->RangedAttackAttribute, playerAttributes,
                    [&](const std::string& name) { return attackMatcher(name, "ranged"); });
                rangedAttackApplied = applyAttribute(
                    state->RangedAttackAttribute,
                    rule.RangedAttackMultiplier * (setDamage ? damageMultiplier : 1.0));
            }
            if (rule.SetPhysicalDefense)
            {
                captureAttribute(state->PhysicalDefenseAttribute, playerAttributes,
                    [&](const std::string& name) { return defenseMatcher(name, "physical"); });
                physicalDefenseApplied = applyAttribute(
                    state->PhysicalDefenseAttribute,
                    rule.PhysicalDefenseMultiplier * (setDefense ? defenseMultiplier : 1.0));
            }
            if (rule.SetMagicalDefense)
            {
                captureAttribute(state->MagicalDefenseAttribute, playerAttributes,
                    [&](const std::string& name) { return defenseMatcher(name, "magical"); });
                magicalDefenseApplied = applyAttribute(
                    state->MagicalDefenseAttribute,
                    rule.MagicalDefenseMultiplier * (setDefense ? defenseMultiplier : 1.0));
            }
            if (rule.SetRangedDefense)
            {
                captureAttribute(state->RangedDefenseAttribute, playerAttributes,
                    [&](const std::string& name) { return defenseMatcher(name, "ranged"); });
                rangedDefenseApplied = applyAttribute(
                    state->RangedDefenseAttribute,
                    rule.RangedDefenseMultiplier * (setDefense ? defenseMultiplier : 1.0));
            }

            for (const auto& requested : rule.AttributeMultipliers)
            {
                const auto normalized = normalizeAttributeIdentifier(requested.Identifier);
                // Vitals require dedicated component notifications.
                if (normalized == "health" || normalized == "maxhealth"
                    || normalized == "stamina" || normalized == "maxstamina")
                {
                    namedAttributesApplied = false;
                    unsupportedNamedAttributes.push_back(requested.Identifier + " (reserved vital)");
                    continue;
                }
                auto named = std::find_if(state->NamedAttributes.begin(), state->NamedAttributes.end(),
                    [&](const NamedPlayerAttributeBaseline& entry) {
                        return entry.Identifier == normalized;
                    });
                if (named == state->NamedAttributes.end())
                {
                    state->NamedAttributes.push_back({normalized, {}});
                    named = std::prev(state->NamedAttributes.end());
                }
                captureAttribute(named->Baseline, playerAttributes,
                    [&](const std::string& loadedName) {
                        return normalizeAttributeIdentifier(loadedName) == normalized;
                    });
                if (!applyAttribute(named->Baseline, requested.Multiplier))
                {
                    namedAttributesApplied = false;
                    unsupportedNamedAttributes.push_back(requested.Identifier);
                }
            }

            for (const auto& requested : rule.Attributes)
            {
                const auto normalized = normalizeAttributeIdentifier(requested.Identifier);
                if (normalized == "health" || normalized == "maxhealth"
                    || normalized == "stamina" || normalized == "maxstamina")
                {
                    namedAttributesApplied = false;
                    unsupportedNamedAttributes.push_back(
                        requested.Identifier + " (use a dedicated vital field)");
                    continue;
                }
                auto named = std::find_if(state->NamedAttributes.begin(),
                    state->NamedAttributes.end(),
                    [&](const NamedPlayerAttributeBaseline& entry) {
                        return entry.Identifier == normalized;
                    });
                if (named == state->NamedAttributes.end())
                {
                    state->NamedAttributes.push_back({normalized, {}});
                    named = std::prev(state->NamedAttributes.end());
                }
                captureAttribute(named->Baseline, playerAttributes,
                    [&](const std::string& loadedName) {
                        return normalizeAttributeIdentifier(loadedName) == normalized;
                    });
                double desired = requested.Value;
                if (requested.Operation == EPlayerAttributeEditOperation::Add)
                    desired = named->Baseline.Value + requested.Value;
                else if (requested.Operation == EPlayerAttributeEditOperation::Multiply)
                    desired = named->Baseline.Value * requested.Value;
                if (!applyAttributeValue(named->Baseline, desired))
                {
                    namedAttributesApplied = false;
                    unsupportedNamedAttributes.push_back(requested.Identifier);
                }
            }

            std::string namedAttributeStatus = "applied";
            if (!namedAttributesApplied)
            {
                namedAttributeStatus = "unsupported [";
                for (size_t index = 0; index < unsupportedNamedAttributes.size(); ++index)
                {
                    if (index) namedAttributeStatus += ", ";
                    namedAttributeStatus += unsupportedNamedAttributes[index];
                }
                namedAttributeStatus += "]";
            }

            if (!rule.Appearance.empty())
            {
                bool changed = false;
                bool provenanceChanged = false;
                UObject* customization = nullptr;
                LoadAppearanceProvenance();
                for (const auto& selection : rule.Appearance)
                {
                    try
                    {
                        std::string currentTable;
                        std::string currentRow;
                        std::string error;
                        if (!ReadPlayerAppearance(pawn, selection.Field, currentTable,
                            currentRow, &customization, error))
                            throw std::runtime_error(error);

                        auto provenance = std::find_if(
                            m_appearanceProvenance.begin(), m_appearanceProvenance.end(),
                            [&](const AppearanceProvenance& value) {
                                return value.PlayerGuid == resolvedPlayerGuid
                                    && value.Field == selection.Field;
                            });
                        const auto normalizedCurrent = ActorHelper::NormalizeObjectPath(
                            RC::to_generic_string(currentTable));
                        const auto normalizedTarget = ActorHelper::NormalizeObjectPath(
                            RC::to_generic_string(selection.DataTablePath));
                        const bool alreadyApplied = normalizedCurrent == normalizedTarget
                            && currentRow == selection.RowName;
                        if (alreadyApplied)
                        {
                            const auto noOpKey = resolvedPlayerGuid + "\n"
                                + selection.Field + "\n" + selection.DataTablePath
                                + "\n" + selection.RowName;
                            if (m_reportedAppearanceNoOps.insert(noOpKey).second)
                            {
                                PS::RoutineLog("players",
                                    STR("Player appearance already matches the native save; skipped rewrite for player {} field {}.\n"),
                                    PS::ToWideSafe(resolvedPlayerGuid.c_str()),
                                    PS::ToWideSafe(selection.Field.c_str()));
                            }
                        }

                        // Keep the first observed fallback across overriding mods.
                        if (provenance == m_appearanceProvenance.end()
                            && (!alreadyApplied || selection.HasFallback)
                            && !resolvedPlayerGuid.empty())
                        {
                            AppearanceProvenance record;
                            record.PlayerGuid = resolvedPlayerGuid;
                            record.Field = selection.Field;
                            record.OwnerMod = RC::to_string(rule.ModName);
                            record.Source = selection.Source;
                            record.AppliedDataTablePath = selection.DataTablePath;
                            record.AppliedRowName = selection.RowName;
                            // Observed save values take precedence over authored fallbacks.
                            const bool useExplicitFallback = alreadyApplied
                                && selection.HasFallback;
                            record.FallbackDataTablePath = useExplicitFallback
                                ? selection.FallbackDataTablePath : currentTable;
                            record.FallbackRowName = useExplicitFallback
                                ? selection.FallbackRowName : currentRow;
                            m_appearanceProvenance.push_back(std::move(record));
                            provenance = std::prev(m_appearanceProvenance.end());
                            provenanceChanged = true;
                        }
                        else if (provenance != m_appearanceProvenance.end()
                            && (provenance->OwnerMod != RC::to_string(rule.ModName)
                                || provenance->AppliedDataTablePath != selection.DataTablePath
                                || provenance->AppliedRowName != selection.RowName))
                        {
                            provenance->OwnerMod = RC::to_string(rule.ModName);
                            provenance->Source = selection.Source;
                            provenance->AppliedDataTablePath = selection.DataTablePath;
                            provenance->AppliedRowName = selection.RowName;
                            provenanceChanged = true;
                        }

                        bool fieldChanged = false;
                        if (!WritePlayerAppearance(pawn, selection.Field,
                            selection.DataTablePath, selection.RowName,
                            fieldChanged, &customization, error))
                            throw std::runtime_error(error);
                        changed = changed || fieldChanged;
                    }
                    catch (const std::exception& error)
                    {
                        appearanceApplied = false;
                        unsupportedAppearance.push_back(selection.Field + "="
                            + selection.RowName + " (" + error.what() + ")");
                    }
                    catch (...)
                    {
                        appearanceApplied = false;
                        unsupportedAppearance.push_back(
                            selection.Field + "=" + selection.RowName);
                    }
                }
                if (provenanceChanged)
                {
                    std::string error;
                    if (!SaveAppearanceProvenance(error))
                    {
                        appearanceApplied = false;
                        unsupportedAppearance.push_back("fallback state (" + error + ")");
                    }
                }
                if (changed && customization)
                {
                    try
                    {
                        auto refresh = ActorHelper::FunctionCall(
                            customization,
                            STR("/Script/Dominion.PlayerCustomizationComponent:OnRep_PlayerCustomization"));
                        refresh.Invoke();
                    }
                    catch (...)
                    {
                        appearanceApplied = false;
                        unsupportedAppearance.push_back("visual refresh");
                    }
                }
            }

            std::string appearanceStatus = "applied";
            if (!appearanceApplied)
            {
                appearanceStatus = "unsupported [";
                for (size_t index = 0; index < unsupportedAppearance.size(); ++index)
                {
                    if (index) appearanceStatus += ", ";
                    appearanceStatus += unsupportedAppearance[index];
                }
                appearanceStatus += "]";
            }

            std::vector<std::string> requestedStatuses;
            const auto appendStatus = [&](const char* label, bool requested,
                bool applied, const std::string& detail = {}) {
                if (!requested) return;
                requestedStatuses.push_back(std::format("{}: {}", label,
                    detail.empty() ? (applied ? "applied" : "unsupported") : detail));
            };
            appendStatus("scale", setScale, scaleApplied);
            appendStatus("health", setHealth || setMaxHealth, healthApplied);
            appendStatus("defense", setDefense, defenseApplied);
            appendStatus("damage", setDamage, damageApplied);
            appendStatus("stamina", setStamina || setMaxStamina, staminaApplied);
            appendStatus("walk speed", rule.SetWalkSpeed, walkSpeedApplied);
            appendStatus("run speed", rule.SetRunSpeed, runSpeedApplied);
            appendStatus("carry weight", rule.SetCarryWeight || rule.SetMaxCarryWeight,
                carryWeightApplied);
            appendStatus("poison resistance", rule.SetPoisonResistance,
                poisonResistanceApplied);
            appendStatus("stamina recovery", rule.SetStaminaRecovery,
                staminaRecoveryApplied);
            appendStatus("physical attack", rule.SetPhysicalAttack, physicalAttackApplied);
            appendStatus("magical attack", rule.SetMagicalAttack, magicalAttackApplied);
            appendStatus("ranged attack", rule.SetRangedAttack, rangedAttackApplied);
            appendStatus("physical defense", rule.SetPhysicalDefense, physicalDefenseApplied);
            appendStatus("magical defense", rule.SetMagicalDefense, magicalDefenseApplied);
            appendStatus("ranged defense", rule.SetRangedDefense, rangedDefenseApplied);
            appendStatus("named attributes",
                !rule.AttributeMultipliers.empty() || !rule.Attributes.empty(),
                namedAttributesApplied, namedAttributeStatus);
            appendStatus("appearance", !rule.Appearance.empty(),
                appearanceApplied, appearanceStatus);
            appendStatus("visual effect", !rule.VisualEffect.empty(),
                visualEffectApplied);
            appendStatus("nameplate", rule.Nameplate.Configured, nameplateApplied,
                nameplateDeferred ? "client-side rule queued" : std::string{});

            std::string statusList;
            for (size_t index = 0; index < requestedStatuses.size(); ++index)
            {
                if (index) statusList += ", ";
                statusList += requestedStatuses[index];
            }
            result = "player adjustment completed (" + statusList + ")";
            return scaleApplied && healthApplied && defenseApplied && damageApplied && staminaApplied
                && walkSpeedApplied && runSpeedApplied && carryWeightApplied && poisonResistanceApplied
                && staminaRecoveryApplied && physicalAttackApplied && magicalAttackApplied
                && rangedAttackApplied && physicalDefenseApplied && magicalDefenseApplied
                && rangedDefenseApplied && namedAttributesApplied && appearanceApplied
                && visualEffectApplied && nameplateApplied;
        }
        catch (const std::exception& error)
        {
            result = std::string("player adjustment failed safely: ") + error.what();
            return false;
        }
        catch (...)
        {
            result = "player adjustment failed safely with an unknown exception";
            return false;
        }
    }

}
