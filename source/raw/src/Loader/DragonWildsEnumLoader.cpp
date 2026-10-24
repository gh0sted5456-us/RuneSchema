#include "Loader/DragonWildsEnumLoader.h"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Helpers/String.hpp"
#include "Utility/Logging.h"
#include "Utility/StructuredDiagnosticB3C.h"
#include "Utility/StructuredDiagnosticOutcomeEvidence.h"
#include "Utility/JsonHelpers.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "Core/CookedEnumContract.h"

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds {
    DragonWildsEnumLoader::DragonWildsEnumLoader() : DragonWildsModLoaderBase("enums") {
        SetDisplayName(TEXT("Enum Loader"));
    }

    DragonWildsEnumLoader::~DragonWildsEnumLoader() {}

    void DragonWildsEnumLoader::OnLoad(const std::filesystem::path& loaderPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        if (engineLifecyclePhase != EEngineLifecyclePhase::PostEngineInit)
        {
            return;
        }

        PS::JsonHelpers::ParseJsonFilesInPathWithSource(loaderPath, [&](const nlohmann::json& data,const std::filesystem::path& relative) {
            PS::StructuredDiagnosticOutcomeEvidence::AppliedProgress applied;
            try { LoadEnums(data, applied); }
            catch(const std::exception& e) {
                PS::StructuredDiagnosticB3C::Report("RS-ENUM-002","Error",applied.Interrupted(),"EnumLoader",
                    "Enum declaration rejected",e.what(),RC::to_string(modName),
                    PS::StructuredDiagnosticB2::Utf8Path(relative),{},applied.InterruptedPhase());
                throw;
            }
        });
    }

    bool DragonWildsEnumLoader::CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        if (engineLifecyclePhase == EEngineLifecyclePhase::PostEngineInit)
        {
            return true;
        }

        return false;
    }

    bool DragonWildsEnumLoader::OnInitialize()
    {
        auto enumClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.Enum"));

        if (!enumClass)
        {
            if (!PS::StructuredDiagnosticB3C::Report("RS-ENUM-001","Error","Failed","EnumLoader",
                "Enum class unavailable","Required /Script/CoreUObject.Enum could not be resolved.",{}, {},"/Script/CoreUObject.Enum","Resolution"))
                PS::Log<LogLevel::Error>(STR("[enums] Unable to initialize {}, failed to get /Script/CoreUObject.Enum.\n"), GetDisplayName());
            return false;
        }

        TArray<UObject*> results;

        UECustom::UObjectGlobals::GetObjectsOfClass(enumClass, results);

        int addedUEnums = 0;
        for (auto& result : results)
        {
            m_enumNameToObjectMap.emplace(result->GetName(), static_cast<UEnum*>(result));
            addedUEnums++;
        }

        PS::Log<LogLevel::Verbose>(STR("[enums] Mapped {} UEnums.\n"), addedUEnums);

        return true;
    }

    void DragonWildsEnumLoader::LoadEnums(const nlohmann::json& data,
        PS::StructuredDiagnosticOutcomeEvidence::AppliedProgress& applied)
    {
        for (auto& [enumNamespace, enumValues] : data.items())
        {
            if (enumNamespace.starts_with("$"))
            {
                continue;
            }

            auto enumNamespaceWide = RC::to_generic_string(enumNamespace);
            if (!enumValues.is_array())
            {
                throw std::runtime_error(std::format("Values in {} must be arrays of strings", enumNamespace));
            }

            auto enumObject = GetEnumByName(enumNamespaceWide);
            if (!enumObject) {
                throw std::runtime_error(std::format("Enum object {} was invalid.", enumNamespace));
            }
            PS::CookedEnumContract::RequireLegacyAppendSafe(RC::to_string(enumObject->GetPathName()));

            for (auto& enumValue : enumValues)
            {
                if (!enumValue.is_string()) throw std::runtime_error(std::format("Array must only contain strings"));

                auto enumValueString = enumValue.get<std::string>();
                if (enumValueString.find(':') != std::string::npos) {
                    throw std::runtime_error(
                        std::format("Enum value '{}' must not contain the namespace. Example: Write ExampleEnum instead of {}::ExampleEnum",
                            enumValueString, enumNamespace));
                }

                auto enumValueStringWide = std::format(STR("{}::{}"), enumNamespaceWide, RC::to_generic_string(enumValueString));

                auto enumName = FName(enumValueStringWide, FNAME_Add);
                int32 indexToInsertAt = enumObject->NumEnums() - 1;

                FEnumNamePair enumNamePair;
                enumNamePair.Key = enumName;
                enumNamePair.Value = indexToInsertAt;

                auto ResultIndex = enumObject->InsertIntoNames(enumNamePair, indexToInsertAt, true);
                if (ResultIndex < 0)
                {
                    throw std::runtime_error(std::format("Something went wrong adding the enum {}", enumValueString));
                }

                PS::Log<LogLevel::Normal>(STR("[enums] Added value {} to {}.\n"), enumValueStringWide, enumNamespaceWide);
            }
        }
    }

    RC::Unreal::UEnum* DragonWildsEnumLoader::GetEnumByName(const RC::StringType& Name)
    {
        auto enumMapIterator = m_enumNameToObjectMap.find(Name);
        if (enumMapIterator != m_enumNameToObjectMap.end())
        {
            return enumMapIterator->second;
        }

        return nullptr;
    }
}
