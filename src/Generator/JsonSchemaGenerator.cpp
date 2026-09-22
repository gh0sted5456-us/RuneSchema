#include <fstream>
#include <iomanip>
#include <chrono>
#include <stdexcept>
#include "Generator/LoaderSchemas.h"
#include "Generator/DiagnosticExport.h"
#include "Runtime/HostServices.h"
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
    namespace {
        thread_local unsigned propertyDepth=0,propertyBudget=4096;
        template<class Json> void WriteSchema(const fs::path& path,const Json& data) {
            std::ofstream file(path);
            file << std::setw(2) << data;file.flush();
            if(!file)throw std::runtime_error("Failed writing schema: "+path.string());
        }
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
            { "not", {{"required", {"$Patch"}}} },
            { "properties", {
                { "Items", ArrayJson }
            }},
        });
        if (CastField<FStructProperty>(Property->GetInner()))
        {
            nlohmann::ordered_json fields = {
                {"type", "object"}, {"minProperties", 1}
            };
            Json["oneOf"].push_back({
                {"type", "object"}, {"additionalProperties", false},
                {"required", {"$Patch"}},
                {"properties", {{"$Patch", {
                    {"type", "array"}, {"minItems", 1},
                    {"items", {
                        {"type", "object"}, {"additionalProperties", false},
                        {"required", {"$Target"}},
                        {"oneOf", {{{"required", {"$Index"}}}, {{"required", {"$Match"}}}}},
                        {"properties", {
                            {"$Index", {{"type", "integer"}, {"minimum", 0}, {"maximum", 2147483647}}},
                            {"$Match", fields}, {"$Target", fields}
                        }}
                    }}
                }}}}
            });
        }
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
        if (!Property) return;
        if(!propertyBudget)throw std::runtime_error("Table property export exceeds 4096 fields.");
        auto PropertyName = RC::to_string(Property->GetName());
        Json[PropertyName] = nlohmann::ordered_json::object();
        nlohmann::ordered_json& JsonProperty = Json[PropertyName];
        if (propertyDepth>=10) {
            JsonProperty["description"]="Reflection limit reached; validate this field in game.";return;
        }
        --propertyBudget;
        struct DepthGuard {DepthGuard(){++propertyDepth;}~DepthGuard(){--propertyDepth;}} guard;

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

    void GenerateEnumSchema(const fs::path& DestinationPath)
    {
        nlohmann::ordered_json EnumJson;
        EnumJson["$schema"] = "http://json-schema.org/draft-07/schema#";
        EnumJson["definitions"] = nlohmann::ordered_json::object();

        std::vector<UObject*> EnumObjects;
        UObjectGlobals::FindAllOf(TEXT("Enum"), EnumObjects);
        if(EnumObjects.size()>8192)throw std::runtime_error("Enum export exceeds 8192 objects.");
        size_t totalNames=0;

        for (UObject* EnumObject : EnumObjects)
        {
            auto* Enum = static_cast<UEnum*>(EnumObject);
            auto EnumName = RC::to_string(Enum->GetName());

            nlohmann::ordered_json Definition;
            Definition["type"] = "string";
            Definition["enum"] = nlohmann::json::array();

            auto Names = Enum->GetEnumNames();
            if(Names.Num()<0 || Names.Num()>4096 || (totalNames+=Names.Num())>131072)
                throw std::runtime_error("Enum export exceeds name limits.");
            for (int32 Index = 0; Index < Names.Num(); ++Index)
            {
                auto FullName = Names[Index].Key.ToString();
                Definition["enum"].push_back(RC::to_string(FullName));

                auto ScopeIndex = FullName.find(TEXT("::"));
                if (ScopeIndex != RC::StringType::npos)
                {
                    Definition["enum"].push_back(RC::to_string(FullName.substr(ScopeIndex + 2)));
                }
            }

            EnumJson["definitions"][EnumName] = std::move(Definition);
        }

        WriteSchema(DestinationPath / "enums.schema.json",EnumJson);

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
        if(datatables.Num()>4096)throw std::runtime_error("Table export exceeds 4096 objects.");

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
            if (DataTableName.empty() || DataTableName.find_first_not_of(
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")!=std::string::npos) continue;
            if (RawSchemaJson["properties"].contains(DataTableName)) continue;
            propertyBudget=4096;

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

            WriteSchema(RawSchemaPath / std::format("{}.schema.json", DataTableName),DataTableSchemaJson);
            generated++;
        }

        WriteSchema(DestinationPath / "raw.schema.json",RawSchemaJson);

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

        WriteSchema(DestinationPath / "utility.schema.json",UtilityJson);

        PS::Log<LogLevel::Normal>(STR("Finished generating utility.schema.json.\n"));
    }

    std::string GenerateSchemaFiles(bool includeLoadedTables, const std::string& exportName)
    {
        PS::Log<LogLevel::Normal>(STR("Exporting schemas.\n"));

        const auto stamp=std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const auto bundle=fs::path(InspectionTools::DiagnosticExportName(exportName,"Schemas",std::to_string(stamp),false,"schemas")).stem();
        auto SchemaPath = PS::HostServices::ExportsDirectory() / "schema" / bundle;
        if (!fs::create_directories(SchemaPath / "loaders"))throw std::runtime_error("Schema export folder already exists");
        const auto schemas=LoaderSchemas();
        nlohmann::json index={{"coverage","Structural schemas; not complete loader validation or game-data dumps"},
            {"loaders",nlohmann::json::object()},{"loadedTableDetailsRequested",includeLoadedTables},
            {"excluded",{{"paks","Cooked files, not a JSON loader"},{"appearance","Use appearance fields in players; not a separate loader"}}}};
        for (const auto& [name,schema]:schemas.items()) {
            const auto file=name+".schema.json";
            WriteSchema(SchemaPath / "loaders" / file,schema);
            index["loaders"][name]="loaders/"+file;
        }
        if (includeLoadedTables) {
            GenerateUtilitySchema(SchemaPath);
            GenerateEnumSchema(SchemaPath);
            GenerateRawSchemas(SchemaPath);
        }
        WriteSchema(SchemaPath / "index.json",index);
        return SchemaPath.string();
    }
}
