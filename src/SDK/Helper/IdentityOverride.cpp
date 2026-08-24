#include "SDK/Helper/IdentityOverride.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <unordered_set>
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/UObject.hpp"
#include "nlohmann/json.hpp"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Structs/Custom/FManagedValue.h"
#include "SDK/Structs/Custom/FScriptMapHelper.h"
#include "Utility/Config.h"
#include "Utility/Logging.h"

using namespace RC;
using namespace RC::Unreal;

namespace {
    struct RegistryBinding {
        const TCHAR* DataClassPath;
        const TCHAR* SubsystemClassPath;
    };

    constexpr RegistryBinding RegistryBindings[] = {
        { TEXT("/Script/Dominion.ItemData"), TEXT("/Script/Dominion.ItemSubsystem") },
        { TEXT("/Script/Dominion.RecipeData"), TEXT("/Script/Dominion.RecipeSubsystem") },
        { TEXT("/Script/Dominion.JournalEntryData"), TEXT("/Script/Dominion.JournalSubsystem") },
    };

    struct RegistryContext {
        UClass* DataClass = nullptr;
        UObject* Subsystem = nullptr;
        FMapProperty* PersistenceMapProperty = nullptr;
        FMapProperty* InternalNameMapProperty = nullptr;
    };

    struct IdentityField {
        const char* JsonName = nullptr;
        const TCHAR* PropertyName = nullptr;
        FStrProperty* Property = nullptr;
        FString Original;
        FString Final;
        bool Requested = false;
        bool Changed = false;
    };

    bool IsBlank(const nlohmann::json& value)
    {
        if (value.is_null())
        {
            return true;
        }
        if (!value.is_string())
        {
            return false;
        }

        const auto& text = value.get_ref<const std::string&>();
        return std::all_of(text.begin(), text.end(), [](unsigned char character) {
            return std::isspace(character) != 0;
        });
    }

    const nlohmann::json* FindRequestedValue(
        const nlohmann::json& body,
        const char* name,
        bool allowPropertiesContainer)
    {
        if (body.contains(name))
        {
            return &body.at(name);
        }
        if (allowPropertiesContainer && body.contains("Properties")
            && body.at("Properties").is_object() && body.at("Properties").contains(name))
        {
            return &body.at("Properties").at(name);
        }
        return nullptr;
    }

    UObject* FindSubsystem(UClass* subsystemClass)
    {
        if (!subsystemClass)
        {
            return nullptr;
        }

        TArray<UObject*> objects;
        UECustom::UObjectGlobals::GetObjectsOfClass(subsystemClass, objects, true);
        for (auto* object : objects)
        {
            if (object && !object->HasAnyFlags(
                static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject)))
            {
                return object;
            }
        }
        return nullptr;
    }

    RegistryContext ResolveRegistry(UObject* object)
    {
        RegistryContext result{};
        if (!object)
        {
            return result;
        }

        for (const auto& binding : RegistryBindings)
        {
            auto* dataClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                nullptr, nullptr, binding.DataClassPath, false);
            if (!dataClass || !object->IsA(dataClass))
            {
                continue;
            }

            result.DataClass = dataClass;
            auto* subsystemClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                nullptr, nullptr, binding.SubsystemClassPath, false);
            result.Subsystem = FindSubsystem(subsystemClass);
            if (result.Subsystem)
            {
                auto* subsystemObjectClass = result.Subsystem->GetClassPrivate();
                result.PersistenceMapProperty = CastField<FMapProperty>(
                    DragonWilds::PropertyHelper::GetPropertyByName(
                        subsystemObjectClass, TEXT("PersistenceIDToDataMap")));
                result.InternalNameMapProperty = CastField<FMapProperty>(
                    DragonWilds::PropertyHelper::GetPropertyByName(
                        subsystemObjectClass, TEXT("InternalNameToDataMap")));
            }
            break;
        }
        return result;
    }

    FString ReadIdentity(UObject* object, FStrProperty* property)
    {
        if (!object || !property)
        {
            return {};
        }
        return property->GetPropertyValue(property->ContainerPtrToValuePtr<void>(object));
    }

    bool MapContainsOther(
        FMapProperty* mapProperty,
        UObject* subsystem,
        const FString& key,
        UObject* object)
    {
        if (!mapProperty || !subsystem || key.GetCharArray().Num() <= 1)
        {
            return false;
        }

        bool collision = false;
        UECustom::FScriptMapHelper map(
            mapProperty, mapProperty->ContainerPtrToValuePtr<void>(subsystem));
        map.ForEachPair([&](void* keyPtr, void* valuePtr) {
            if (collision || *static_cast<FString*>(keyPtr) != key)
            {
                return;
            }
            UObject* mapped = nullptr;
            std::memcpy(&mapped, valuePtr, sizeof(mapped));
            collision = mapped && mapped != object;
        });
        return collision;
    }

    bool HasIdentityCollision(
        const RegistryContext& registry,
        UObject* object,
        const FString& proposed)
    {
        if (!registry.DataClass || proposed.GetCharArray().Num() <= 1)
        {
            return false;
        }

        TArray<UObject*> candidates;
        UECustom::UObjectGlobals::GetObjectsOfClass(registry.DataClass, candidates, true);
        for (auto* candidate : candidates)
        {
            if (!candidate || candidate == object || candidate->HasAnyFlags(
                static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject)))
            {
                continue;
            }

            for (auto* name : { TEXT("PersistenceID"), TEXT("InternalName") })
            {
                auto* property = CastField<FStrProperty>(
                    DragonWilds::PropertyHelper::GetPropertyByName(
                        candidate->GetClassPrivate(), name));
                if (property && ReadIdentity(candidate, property) == proposed)
                {
                    return true;
                }
            }
        }

        return MapContainsOther(registry.PersistenceMapProperty, registry.Subsystem, proposed, object)
            || MapContainsOther(registry.InternalNameMapProperty, registry.Subsystem, proposed, object);
    }

    bool MapKeyTargetsObject(
        UECustom::FScriptMapHelper& map,
        const FString& key,
        UObject* object)
    {
        bool matches = false;
        map.ForEachPair([&](void* keyPtr, void* valuePtr) {
            if (matches || *static_cast<FString*>(keyPtr) != key)
            {
                return;
            }
            UObject* mapped = nullptr;
            std::memcpy(&mapped, valuePtr, sizeof(mapped));
            matches = mapped == object;
        });
        return matches;
    }

    void RemoveOwnedKey(
        UECustom::FScriptMapHelper& map,
        const FString& key,
        UObject* object)
    {
        if (key.GetCharArray().Num() > 1 && MapKeyTargetsObject(map, key, object))
        {
            map.Remove(const_cast<FString*>(&key));
        }
    }

    void AddKey(
        UECustom::FScriptMapHelper& map,
        const FString& key,
        UObject* object)
    {
        if (key.GetCharArray().Num() <= 1)
        {
            return;
        }

        UECustom::FManagedValue pair;
        map.InitializePair(pair);
        *static_cast<FString*>(map.GetKeyPtr(pair.GetData())) = key;
        std::memcpy(map.GetValuePtr(pair.GetData()), &object, sizeof(object));
        map.Add(pair);
    }

    void SynchronizeRegistry(
        const RegistryContext& registry,
        UObject* object,
        const IdentityField& persistence,
        const IdentityField& internalName)
    {
        if (!registry.Subsystem || !registry.PersistenceMapProperty
            || !registry.InternalNameMapProperty)
        {
            return;
        }

        UECustom::FScriptMapHelper persistenceMap(
            registry.PersistenceMapProperty,
            registry.PersistenceMapProperty->ContainerPtrToValuePtr<void>(registry.Subsystem));
        UECustom::FScriptMapHelper internalMap(
            registry.InternalNameMapProperty,
            registry.InternalNameMapProperty->ContainerPtrToValuePtr<void>(registry.Subsystem));

        RemoveOwnedKey(persistenceMap, persistence.Original, object);
        RemoveOwnedKey(internalMap, persistence.Original, object);
        RemoveOwnedKey(internalMap, internalName.Original, object);

        AddKey(persistenceMap, persistence.Final, object);
        AddKey(internalMap, persistence.Final, object);
        if (internalName.Final != persistence.Final)
        {
            AddKey(internalMap, internalName.Final, object);
        }

        persistenceMap.Rehash();
        internalMap.Rehash();
    }
}

namespace DragonWilds {
    bool IdentityOverride::IsIdentityProperty(const std::string& propertyName)
    {
        return propertyName == "PersistenceID" || propertyName == "InternalName";
    }

    IdentityOverrideResult IdentityOverride::Apply(
        UObject* object,
        const nlohmann::json& body,
        const RC::StringType& context,
        IdentityOverrideTarget target,
        bool allowPropertiesContainer)
    {
        IdentityOverrideResult result{};
        if (!object || !body.is_object())
        {
            return result;
        }

        const auto& settings = PS::PSConfig::Get()->GetIdentityOverrideSettings();
        const bool targetEnabled = target == IdentityOverrideTarget::Asset ? settings.assets
            : target == IdentityOverrideTarget::Recipe ? settings.recipes
            : settings.journals;
        if (!settings.enabled || !targetEnabled)
        {
            return result;
        }

        const bool identityDeclared = FindRequestedValue(
            body, "PersistenceID", allowPropertiesContainer)
            || FindRequestedValue(body, "InternalName", allowPropertiesContainer);
        if (!identityDeclared)
        {
            return result;
        }

        auto registry = ResolveRegistry(object);
        std::array<IdentityField, 2> fields = {{
            { "PersistenceID", TEXT("PersistenceID") },
            { "InternalName", TEXT("InternalName") },
        }};

        bool anyRequested = false;
        for (auto& field : fields)
        {
            field.Property = CastField<FStrProperty>(
                PropertyHelper::GetPropertyByName(
                    object->GetClassPrivate(), field.PropertyName));
            if (field.Property)
            {
                field.Original = ReadIdentity(object, field.Property);
                field.Final = field.Original;
            }

            const auto* requested = FindRequestedValue(
                body, field.JsonName, allowPropertiesContainer);
            if (!requested)
            {
                continue;
            }

            anyRequested = true;
            field.Requested = true;
            if (!field.Property)
            {
                PS::Log<LogLevel::Warning>(STR("{}: {} is not an FString property; override rejected.\n"),
                    context, field.PropertyName);
                result.Rejected++;
                field.Requested = false;
                continue;
            }

            if (IsBlank(*requested))
            {
                PS::Log<LogLevel::Verbose>(STR("{}: blank {} preserves the default value.\n"),
                    context, field.PropertyName);
                field.Requested = false;
                continue;
            }
            if (!requested->is_string())
            {
                PS::Log<LogLevel::Warning>(STR("{}: {} must be a string or null; override rejected.\n"),
                    context, field.PropertyName);
                result.Rejected++;
                field.Requested = false;
                continue;
            }

            field.Final = FString(RC::to_generic_string(requested->get<std::string>()).c_str());
            field.Changed = field.Final != field.Original;
        }

        if (!anyRequested)
        {
            return result;
        }
        if (!registry.DataClass)
        {
            PS::Log<LogLevel::Warning>(STR("{}: identity overrides are only supported for ItemData, RecipeData, and JournalEntryData assets.\n"),
                context);
            result.Rejected += static_cast<int>(std::count_if(fields.begin(), fields.end(),
                [](const IdentityField& field) { return field.Requested; }));
            return result;
        }
        if (registry.Subsystem
            && (!registry.PersistenceMapProperty || !registry.InternalNameMapProperty))
        {
            PS::Log<LogLevel::Warning>(STR("{}: the live registry maps were incomplete; identity override rejected.\n"),
                context);
            result.Rejected += static_cast<int>(std::count_if(fields.begin(), fields.end(),
                [](const IdentityField& field) { return field.Requested; }));
            return result;
        }

        for (auto& field : fields)
        {
            if (!field.Requested || !field.Changed)
            {
                continue;
            }
            if (HasIdentityCollision(registry, object, field.Final))
            {
                PS::Log<LogLevel::Warning>(STR("{}: {} '{}' is already owned by another registered object; override rejected.\n"),
                    context, field.PropertyName, *field.Final);
                field.Final = field.Original;
                field.Changed = false;
                field.Requested = false;
                result.Rejected++;
            }
        }

        if (settings.dryRun)
        {
            for (const auto& field : fields)
            {
                if (!field.Requested || !field.Changed)
                {
                    continue;
                }
                PS::Log<LogLevel::Normal>(
                    STR("{}: dry run would change {} from '{}' to '{}'.\n"),
                    context, field.PropertyName, *field.Original, *field.Final);
                result.Previewed++;
            }
            return result;
        }

        bool changed = false;
        for (auto& field : fields)
        {
            if (!field.Requested || !field.Changed)
            {
                continue;
            }
            field.Property->SetPropertyValue(
                field.Property->ContainerPtrToValuePtr<void>(object), field.Final);
            if (settings.logChanges)
            {
                PS::Log<LogLevel::Normal>(STR("{}: changed {} from '{}' to '{}'.\n"),
                    context, field.PropertyName, *field.Original, *field.Final);
            }
            changed = true;
            result.Written++;
        }

        if (changed)
        {
            SynchronizeRegistry(registry, object, fields[0], fields[1]);
            if (settings.logChanges)
            {
                PS::Log<LogLevel::Normal>(STR("{}: applied {} registry-aware identity override{}.\n"),
                    context, result.Written, result.Written == 1 ? STR("") : STR("s"));
            }
        }
        return result;
    }
}
