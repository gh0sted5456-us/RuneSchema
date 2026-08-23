#include <fstream>
#include <cctype>
#include <unordered_set>
#include "UE4SSProgram.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/Property/FEnumProperty.hpp"
#include "Unreal/Property/FTextProperty.hpp"
#include "Unreal/Engine/UDataTable.hpp"
#include "Unreal/UObjectGlobals.hpp"
#include "nlohmann/json.hpp"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "Utility/Logging.h"
#include "Generator/JsonSchemaGenerator.h"

using namespace RC;
using namespace RC::Unreal;

namespace fs = std::filesystem;

namespace PS::JsonSchemaGenerator {
    bool IsUnsafePropertyName(const RC::StringType& Name)
    {
        static const std::unordered_set<RC::StringType> UnsafeNames = {
            TEXT("PersistenceID"),
            TEXT("InternalName"),
            TEXT("RootComponent"),
            TEXT("UberGraphFrame"),
            TEXT("BlueprintCreatedComponents"),
            TEXT("InstanceComponents")
        };
        return UnsafeNames.contains(Name)
            || Name.ends_with(TEXT("Guid"))
            || Name.ends_with(TEXT("GUID"));
    }

    void ParsePropertyInfo(FProperty* Property, nlohmann::ordered_json& Json);

    void ParseEnumPropertyInfo(FEnumProperty* Property, nlohmann::ordered_json& Json)
    {
        Json["type"] = "string";
        Json["description"] = "EnumProperty";

        if (auto Enum = Property->GetEnum())
        {
            Json["$ref"] = RC::fmt("../enums.schema.json#/definitions/%S", Enum->GetName().c_str());
        }
    }

    void ParseNumericPropertyInfo(FNumericProperty* Property, nlohmann::ordered_json& Json)
    {
        if (Property->IsFloatingPoint())
        {
            Json["type"] = "number";
            Json["description"] = "FloatProperty";
        }
        else
        {
            Json["type"] = "integer";
            Json["description"] = "IntProperty";
        }
    }

    void ParseBoolPropertyInfo(nlohmann::ordered_json& Json)
    {
        Json["type"] = "boolean";
        Json["description"] = "BoolProperty";
    }

    void ParseStrPropertyInfo(FProperty* Property, nlohmann::ordered_json& Json)
    {
        Json["type"] = "string";

        if (CastField<FStrProperty>(Property))
        {
            Json["description"] = "FString";
        }
        else if (CastField<FTextProperty>(Property))
        {
            Json["description"] = "FText";
        }
        else
        {
            Json["description"] = "FName";
        }
    }

    void ParseStructPropertyInfo(FStructProperty* Property, nlohmann::ordered_json& Json)
    {
        auto Struct = Property->GetStruct();
        Json["type"] = "object";
        Json["description"] = "StructProperty";
        Json["properties"] = nlohmann::ordered_json::object();

        if (!Struct)
        {
            return;
        }

        for (FProperty* InnerProperty : TFieldRange<FProperty>(Struct.Get(), EFieldIterationFlags::None))
        {
            ParsePropertyInfo(InnerProperty, Json["properties"]);
        }
    }

    void ParseArrayPropertyInfo(FArrayProperty* Property, nlohmann::ordered_json& Json)
    {
        nlohmann::ordered_json InnerJson;
        ParsePropertyInfo(Property->GetInner(), InnerJson);

        nlohmann::ordered_json ArrayJson = {
            { "type", "array" },
            { "description", "ArrayProperty" },
            { "items", nlohmann::ordered_json::object() }
        };

        for (auto& [Key, Value] : InnerJson.front().items())
        {
            ArrayJson["items"][Key] = Value;
        }

        Json["oneOf"] = nlohmann::json::array();
        Json["oneOf"].push_back(ArrayJson);
        Json["oneOf"].push_back({
            { "type", "object" },
            { "properties", {
                { "Items", ArrayJson }
            }},
        });
    }

    void ParseMapPropertyInfo(FMapProperty* Property, nlohmann::ordered_json& Json)
    {
        Json["type"] = "array";
        Json["description"] = "MapProperty";
        Json["items"] = {
            { "type", "object" },
            { "properties", {
                { "Key", nlohmann::ordered_json::object() },
                { "Value", nlohmann::ordered_json::object() }
            }},
        };

        nlohmann::ordered_json KeyJson;
        nlohmann::ordered_json ValueJson;
        ParsePropertyInfo(Property->GetKeyProp(), KeyJson);
        ParsePropertyInfo(Property->GetValueProp(), ValueJson);

        for (auto& [Key, Value] : KeyJson.front().items())
        {
            Json["items"]["properties"]["Key"][Key] = Value;
        }
        for (auto& [Key, Value] : ValueJson.front().items())
        {
            Json["items"]["properties"]["Value"][Key] = Value;
        }
    }

    void ParsePropertyInfo(FProperty* Property, nlohmann::ordered_json& Json)
    {
        if (!Property || IsUnsafePropertyName(Property->GetName()))
        {
            return;
        }

        auto PropertyName = RC::to_string(Property->GetName());
        Json[PropertyName] = nlohmann::ordered_json::object();
        nlohmann::ordered_json& JsonProperty = Json[PropertyName];

        if (auto EnumProperty = CastField<FEnumProperty>(Property))
        {
            ParseEnumPropertyInfo(EnumProperty, JsonProperty);
        }
        else if (auto NumericProperty = CastField<FNumericProperty>(Property))
        {
            ParseNumericPropertyInfo(NumericProperty, JsonProperty);
        }
        else if (CastField<FBoolProperty>(Property))
        {
            ParseBoolPropertyInfo(JsonProperty);
        }
        else if (auto StructProperty = CastField<FStructProperty>(Property))
        {
            ParseStructPropertyInfo(StructProperty, JsonProperty);
        }
        else if (auto MapProperty = CastField<FMapProperty>(Property))
        {
            ParseMapPropertyInfo(MapProperty, JsonProperty);
        }
        else if (auto ArrayProperty = CastField<FArrayProperty>(Property))
        {
            ParseArrayPropertyInfo(ArrayProperty, JsonProperty);
        }
        else if (CastField<FStrProperty>(Property) || CastField<FTextProperty>(Property) || CastField<FNameProperty>(Property))
        {
            ParseStrPropertyInfo(Property, JsonProperty);
        }
        else if (CastField<FClassProperty>(Property) || CastField<FSoftClassProperty>(Property))
        {
            JsonProperty["$ref"] = "../utility.schema.json#/definitions/ClassReference";
        }
        else if (CastField<FObjectProperty>(Property) || CastField<FSoftObjectProperty>(Property))
        {
            JsonProperty["$ref"] = "../utility.schema.json#/definitions/ObjectReference";
        }
    }

    std::string SafeSchemaFileName(const RC::StringType& Name)
    {
        auto value = RC::to_string(Name);
        for (auto& character : value)
        {
            if (!std::isalnum(static_cast<unsigned char>(character)) && character != '-' && character != '_')
            {
                character = '_';
            }
        }
        return value;
    }

    nlohmann::ordered_json BuildObjectSchema(UClass* Class)
    {
        nlohmann::ordered_json schema = {
            { "$schema", "http://json-schema.org/draft-07/schema#" },
            { "type", "object" },
            { "properties", nlohmann::ordered_json::object() },
            { "additionalProperties", true }
        };
        if (!Class)
        {
            return schema;
        }

        for (FProperty* Property : TFieldRange<FProperty>(Class, EFieldIterationFlags::IncludeSuper))
        {
            ParsePropertyInfo(Property, schema["properties"]);
        }
        return schema;
    }

    void GenerateAssetSchemas(const fs::path& DestinationPath)
    {
        auto assetSchemaPath = DestinationPath / "assets";
        fs::create_directories(assetSchemaPath);
        nlohmann::ordered_json index = {
            { "$schema", "http://json-schema.org/draft-07/schema#" },
            { "type", "object" },
            { "properties", nlohmann::ordered_json::object() },
            { "additionalProperties", { { "type", "object" } } }
        };

        auto* dataAssetClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Engine.DataAsset"), false);
        auto* curveClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Engine.CurveBase"), false);
        if (!dataAssetClass || !curveClass)
        {
            return;
        }

        std::unordered_set<RC::StringType> generatedClasses;
        std::size_t targetCount = 0;
        UObjectGlobals::ForEachUObject([&](UObject* Object, int32_t, int32_t) -> LoopAction {
            if (!Object || Object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject))
                || (!Object->IsA(dataAssetClass) && !Object->IsA(curveClass)))
            {
                return LoopAction::Continue;
            }

            auto* objectClass = Object->GetClassPrivate();
            if (!objectClass)
            {
                return LoopAction::Continue;
            }

            auto className = objectClass->GetName();
            auto fileName = SafeSchemaFileName(className) + ".schema.json";
            if (generatedClasses.insert(className).second)
            {
                std::ofstream output(assetSchemaPath / fileName);
                output << BuildObjectSchema(objectClass).dump(2);
            }

            index["properties"][RC::to_string(Object->GetPathName())] = {
                { "$ref", std::format("assets/{}", fileName) }
            };
            ++targetCount;
            return LoopAction::Continue;
        });

        std::ofstream output(DestinationPath / "assets.schema.json");
        output << index.dump(2);
        PS::Log<LogLevel::Normal>(STR("Finished generating asset schemas ({} targets, {} classes).\n"),
            targetCount, generatedClasses.size());
    }

    void GenerateBlueprintSchemas(const fs::path& DestinationPath)
    {
        auto blueprintSchemaPath = DestinationPath / "blueprints";
        fs::create_directories(blueprintSchemaPath);
        nlohmann::ordered_json index = {
            { "$schema", "http://json-schema.org/draft-07/schema#" },
            { "type", "object" },
            { "properties", nlohmann::ordered_json::object() },
            { "additionalProperties", { { "type", "object" } } }
        };

        auto* blueprintClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Engine.BlueprintGeneratedClass"), false);
        if (!blueprintClass)
        {
            return;
        }

        std::size_t generated = 0;
        UObjectGlobals::ForEachUObject([&](UObject* Object, int32_t, int32_t) -> LoopAction {
            if (!Object || !Object->IsA(blueprintClass))
            {
                return LoopAction::Continue;
            }

            auto* generatedClass = static_cast<UClass*>(Object);
            auto className = generatedClass->GetName();
            auto fileName = SafeSchemaFileName(generatedClass->GetPathName()) + ".schema.json";
            std::ofstream output(blueprintSchemaPath / fileName);
            output << BuildObjectSchema(generatedClass).dump(2);

            auto reference = nlohmann::ordered_json{
                { "$ref", std::format("blueprints/{}", fileName) }
            };
            index["properties"][RC::to_string(className)] = reference;
            auto path = generatedClass->GetPathName();
            auto dot = path.find_last_of(TEXT('.'));
            if (dot != RC::StringType::npos)
            {
                index["properties"][RC::to_string(path.substr(0, dot))] = reference;
            }
            ++generated;
            return LoopAction::Continue;
        });

        std::ofstream output(DestinationPath / "blueprints.schema.json");
        output << index.dump(2);
        PS::Log<LogLevel::Normal>(STR("Finished generating blueprint schemas ({} classes).\n"), generated);
    }

    void GenerateEnumSchema(const fs::path& DestinationPath)
    {
        nlohmann::ordered_json EnumJson;
        EnumJson["$schema"] = "http://json-schema.org/draft-07/schema#";
        EnumJson["definitions"] = nlohmann::ordered_json::object();

        std::vector<UObject*> EnumObjects;
        UObjectGlobals::FindAllOf(TEXT("Enum"), EnumObjects);

        for (UObject* EnumObject : EnumObjects)
        {
            auto* Enum = static_cast<UEnum*>(EnumObject);
            auto EnumName = RC::to_string(Enum->GetName());

            nlohmann::ordered_json Definition;
            Definition["type"] = "string";
            Definition["enum"] = nlohmann::json::array();

            auto Names = Enum->GetEnumNames();
            for (int32 Index = 0; Index < Names.Num(); ++Index)
            {
                if (Index == Names.Num() - 1)
                {
                    continue;
                }

                auto FullName = Names[Index].Key.ToString();
                Definition["enum"].push_back(RC::to_string(FullName));

                auto ScopeIndex = FullName.find(TEXT("::"));
                if (ScopeIndex != RC::StringType::npos)
                {
                    Definition["enum"].push_back(RC::to_string(FullName.substr(ScopeIndex + 2)));
                }
            }

            EnumJson["definitions"][EnumName] = Definition;
        }

        std::ofstream OutputFile(DestinationPath / "enums.schema.json");
        OutputFile << EnumJson.dump(2);

        PS::Log<LogLevel::Normal>(STR("Finished generating enums.schema.json ({} enums).\n"), EnumObjects.size());
    }

    void GenerateRawSchemas(const fs::path& DestinationPath)
    {
        auto RawSchemaPath = DestinationPath / "raw";
        std::filesystem::create_directories(RawSchemaPath);

        nlohmann::ordered_json RawSchemaJson = {
            { "$schema", "http://json-schema.org/draft-07/schema#" },
            { "type", "object" },
            { "properties", nlohmann::ordered_json::object() }
        };

        auto* datatableClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, TEXT("/Script/Engine.DataTable"));
        if (!datatableClass)
        {
            return;
        }

        TArray<UObject*> datatables;
        UECustom::UObjectGlobals::GetObjectsOfClass(datatableClass, datatables, true);

        int generated = 0;
        for (auto* object : datatables)
        {
            if (!object || object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject | RF_ArchetypeObject)))
            {
                continue;
            }

            auto* DataTable = static_cast<UDataTable*>(object);
            auto RowStruct = DataTable->GetRowStruct();
            if (!RowStruct)
            {
                continue;
            }

            auto DataTableName = RC::to_string(DataTable->GetName());

            nlohmann::ordered_json DataTableSchemaJson = {
                { "$schema", "http://json-schema.org/draft-07/schema#" },
                { "type", "object" },
                { "additionalProperties", {
                    { "type", "object" },
                    { "properties", nlohmann::ordered_json::object() }
                }},
            };

            RawSchemaJson["properties"][DataTableName] = {
                { "$ref", std::format("raw/{}.schema.json", DataTableName) }
            };

            for (FProperty* InnerProperty : TFieldRange<FProperty>(RowStruct.Get(), EFieldIterationFlags::None))
            {
                ParsePropertyInfo(InnerProperty, DataTableSchemaJson["additionalProperties"]["properties"]);
            }

            std::ofstream OutputFile(RawSchemaPath / std::format("{}.schema.json", DataTableName));
            OutputFile << DataTableSchemaJson.dump(2);
            generated++;
        }

        std::ofstream OutputFile(DestinationPath / "raw.schema.json");
        OutputFile << RawSchemaJson.dump(2);

        PS::Log<LogLevel::Normal>(STR("Finished generating raw schema files ({} data tables).\n"), generated);
    }

    void GenerateUtilitySchema(const fs::path& DestinationPath)
    {
        nlohmann::ordered_json UtilityJson = {
            { "$schema", "http://json-schema.org/draft-07/schema#" },
            { "definitions", {
                { "ObjectReference", {
                    { "oneOf", {
                        { { "type", "string" }, { "pattern", "^/" } },
                        { { "type", "object" }, { "properties", {
                            { "ObjectName", { { "type", "string" } } },
                            { "ObjectPath", { { "type", "string" }, { "pattern", "^/" } } }
                        }}},
                        { { "type", "object" }, { "properties", {
                            { "AssetPathName", { { "type", "string" }, { "pattern", "^/" } } },
                            { "SubPathString", { { "type", "string" } } }
                        }}}
                    }}
                }},
                { "ClassReference", {
                    { "oneOf", {
                        { { "type", "string" }, { "pattern", "^/" } },
                        { { "type", "object" }, { "properties", {
                            { "ObjectName", { { "type", "string" } } },
                            { "ObjectPath", { { "type", "string" }, { "pattern", "^/" } } }
                        }}},
                        { { "type", "object" }, { "properties", {
                            { "AssetPathName", { { "type", "string" }, { "pattern", "^/" } } },
                            { "SubPathString", { { "type", "string" } } }
                        }}}
                    }}
                }}
            }}
        };

        std::ofstream OutputFile(DestinationPath / "utility.schema.json");
        OutputFile << UtilityJson.dump(2);

        PS::Log<LogLevel::Normal>(STR("Finished generating utility.schema.json.\n"));
    }

    void GenerateSchemaFiles()
    {
        PS::Log<LogLevel::Normal>(STR("Beginning generation of schema files, please wait a moment...\n"));

        auto SchemaPath = fs::path(UE4SSProgram::get_program().get_working_directory()) / "Mods" / "RuneSchema" / "schemas";
        std::filesystem::create_directories(SchemaPath);

        GenerateUtilitySchema(SchemaPath);
        GenerateEnumSchema(SchemaPath);
        GenerateRawSchemas(SchemaPath);
        GenerateAssetSchemas(SchemaPath);
        GenerateBlueprintSchemas(SchemaPath);

        PS::Log<LogLevel::Normal>(STR("Finished generating all schema files. All done!\n"));
    }
}
