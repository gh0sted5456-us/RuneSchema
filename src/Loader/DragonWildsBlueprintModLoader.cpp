#include <regex>
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/UObjectGlobals.hpp"
#include "Unreal/AActor.hpp"
#include "Helpers/String.hpp"
#include "SDK/Helper/PropertyHelper.h"
#include "Utility/Config.h"
#include "Utility/Logging.h"
#include "Utility/JsonHelpers.h"
#include "Loader/DragonWildsBlueprintModLoader.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Helper/Memory.h"
#include "SDK/Classes/Custom/UBlueprintGeneratedClass.h"
#include "SDK/Classes/Custom/UInheritableComponentHandler.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds {
    DragonWildsBlueprintModLoader::DragonWildsBlueprintModLoader() : DragonWildsModLoaderBase("blueprints")
    {
        SetDisplayName(TEXT("Blueprint Mod Loader"));
    }

    DragonWildsBlueprintModLoader::~DragonWildsBlueprintModLoader()
    {
        auto expectedPostLoad = PostLoadHook.disable();
        PostLoadHook = {};
        PostLoadCallback = nullptr;

        auto expectedPostInit = PostInitComponentsHook.disable();
        PostInitComponentsHook = {};
        PostInitComponentsCallback = nullptr;

        m_modsMap.clear();
    }

    void DragonWildsBlueprintModLoader::OnLoad(const std::filesystem::path& loaderPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        if (engineLifecyclePhase == EEngineLifecyclePhase::PostEngineInit)
        {
            PS::JsonHelpers::ParseJsonFilesInPath(loaderPath, [&](const nlohmann::json& data) {
                LoadSafe(data);
            });
        }
        else if (engineLifecyclePhase == EEngineLifecyclePhase::GameInstanceInit)
        {
            PS::JsonHelpers::ParseJsonFilesInPath(loaderPath, [&](const nlohmann::json& data) {
                LoadUnsafe(data);
            });
        }
    }

    void DragonWildsBlueprintModLoader::OnAutoReload(const std::filesystem::path::string_type& modName, const std::filesystem::path& modFilePath)
    {
        PS::JsonHelpers::ParseJsonFileInPath(modFilePath, [&](const nlohmann::json& data) {
            LoadUnsafe(data);
        });
    }

    bool DragonWildsBlueprintModLoader::CanInitialize(const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        if (engineLifecyclePhase == EEngineLifecyclePhase::PostEngineInit)
        {
            return true;
        }

        return false;
    }

    bool DragonWildsBlueprintModLoader::OnInitialize()
    {
        if (!HookPostLoad())
        {
            PS::Log<LogLevel::Error>(TEXT("Cannot hook UBlueprintGeneratedClass::PostLoad which means blueprint mods will not function properly.\n"));
            return false;
        }

        if (!HookPostInitComponents())
        {
            PS::Log<LogLevel::Error>(TEXT("Cannot hook AActor::PostInitComponents which means blueprint mods will not function properly.\n"));
            return false;
        }

        return true;
    }

    bool DragonWildsBlueprintModLoader::HookPostLoad()
    {
        auto vtable = DragonWilds::GetVTablePtrByClassPath(TEXT("/Script/Engine.BlueprintGeneratedClass"));
        if (!vtable)
        {
            PS::Log<LogLevel::Error>(STR("Something went wrong with getting VTable pointer for UBlueprintGeneratedClass."));
            return false;
        }

        void* postloadPtr = DragonWilds::GetVirtualFunctionFromVTable(vtable, 19);
        PS::Log<LogLevel::Verbose>(TEXT("Found UBlueprintGeneratedClass::PostLoad: {}\n"), postloadPtr);

        PostLoadCallback = [&](UClass* actorClass) {
            ModifyObject(actorClass->GetClassDefaultObject());
        };

        PostLoadHook = safetyhook::create_inline(postloadPtr,
            reinterpret_cast<void*>(PostLoad));

        return true;
    }

    bool DragonWildsBlueprintModLoader::HookPostInitComponents()
    {
        auto vtable = DragonWilds::GetVTablePtrByClassPath(TEXT("/Script/Engine.Actor"));
        if (!vtable)
        {
            PS::Log<LogLevel::Error>(TEXT("Something went wrong with getting VTable pointer for AActor.\n"));
            return false;
        }

        void* postInitCompsPtr = DragonWilds::GetVirtualFunctionFromVTable(vtable, 169);
        PS::Log<LogLevel::Verbose>(TEXT("Found AActor::PostInitializeComponents: {}\n"), postInitCompsPtr);

        PostInitComponentsCallback = [&](AActor* self) {
            ModifyObject(self);

            auto actorClass = self->GetClassPrivate();
            if (!actorClass)
            {
                return;
            }

            const RC::StringType componentArrayNames[] = {
                TEXT("BlueprintCreatedComponents"),
                TEXT("InstanceComponents")
            };

            for (const auto& arrayName : componentArrayNames)
            {
                auto arrayPropertyBase = DragonWilds::PropertyHelper::GetPropertyByName(actorClass, arrayName);
                auto arrayProperty = CastField<FArrayProperty>(arrayPropertyBase);
                if (!arrayProperty)
                {
                    continue;
                }

                auto objectInner = CastField<FObjectProperty>(arrayProperty->GetInner());
                if (!objectInner)
                {
                    continue;
                }

                FScriptArrayHelper arrayHelper(arrayProperty, arrayProperty->ContainerPtrToValuePtr<void>(self));
                for (int32 index = 0; index < arrayHelper.Num(); ++index)
                {
                    UObject* component = objectInner->GetObjectPropertyValue(arrayHelper.GetRawPtr(index));
                    if (component)
                    {
                        ModifyObject(component);
                    }
                }
            }
        };

        PostInitComponentsHook = safetyhook::create_inline(postInitCompsPtr,
            reinterpret_cast<void*>(PostInitComponents));

        return true;
    }

    void DragonWildsBlueprintModLoader::LoadSafe(const nlohmann::json& data)
    {
        for (auto& [assetName, assetData] : data.items())
        {
            if (assetName.starts_with("$"))
            {
                continue;
            }

            auto assetNameWide = RC::to_generic_string(assetName);
            if (!assetNameWide.starts_with(TEXT("/Game/")))
            {
                auto assetFName = FName(assetNameWide, FNAME_Add);
                auto newMod = DragonWildsBlueprintMod(assetFName, assetData);
                auto it = m_modsMap.find(assetFName);
                if (it != m_modsMap.end())
                {
                    m_modsMap.at(assetFName).push_back(newMod);
                }
                else
                {
                    auto newModContainer = std::vector<DragonWildsBlueprintMod>{
                        newMod
                    };
                    m_modsMap.emplace(assetFName, newModContainer);
                }

                PS::Log<LogLevel::Normal>(STR("Loaded changes to {}\n"), assetNameWide);
            }
        }
    }

    void DragonWildsBlueprintModLoader::LoadUnsafe(const nlohmann::json& data)
    {
        for (auto& [assetName, assetData] : data.items())
        {
            auto assetNameWide = RC::to_generic_string(assetName);
            if (assetNameWide.starts_with(TEXT("/Game/")))
            {
                static const std::wregex Pattern(LR"(^(.*/)([^/.]+)$)");
                assetNameWide = std::regex_replace(assetNameWide, Pattern, TEXT("$1$2.$2_C"));

                auto softObjectPtr = UECustom::TSoftObjectPtr<UObject>(UECustom::FSoftObjectPath(assetNameWide));
                auto asset = UECustom::UKismetSystemLibrary::LoadAsset_Blocking(softObjectPtr);
                if (!asset)
                {
                    throw std::runtime_error(RC::fmt("Failed to apply blueprint changes, asset '%S' was invalid", assetNameWide.c_str()));
                }

                asset->SetRootSet();

                auto& defaultObject = static_cast<UClass*>(asset)->GetClassDefaultObject();
                ApplyData(assetData, defaultObject.Get(), true);

                PS::Log<RC::LogLevel::Normal>(TEXT("Applied changes to {}\n"), static_cast<UClass*>(asset)->GetNamePrivate().ToString());
            }
        }
    }

    std::vector<DragonWildsBlueprintMod>& DragonWildsBlueprintModLoader::GetModsForBlueprint(const RC::Unreal::FName& name)
    {
        auto it = m_modsMap.find(name);
        if (it != m_modsMap.end())
        {
            return it->second;
        }

        throw std::runtime_error(RC::fmt("Failed to get mods for this blueprint. Affected mod name: %S", name.ToString().c_str()));
    }

    void DragonWildsBlueprintModLoader::ModifyObject(RC::Unreal::UObject* object)
    {
        if (!object) return;

        auto objectClass = object->GetClassPrivate();
        if (!objectClass)
        {
            return;
        }

        auto& objectName = objectClass->GetNamePrivate();

        if (!m_modsMap.contains(objectName))
        {
            return;
        }

        auto& mods = GetModsForBlueprint(objectName);
        for (auto& mod : mods)
        {
            try
            {
                ApplyMod(mod, object);
            }
            catch (const std::exception& e)
            {
                PS::Log<RC::LogLevel::Error>(TEXT("Failed modifying blueprint '{}', {}\n"), objectName.ToString(), PS::ToWideSafe(e.what()));
            }
        }
    }

    void DragonWildsBlueprintModLoader::ApplyMod(const DragonWildsBlueprintMod& mod, UObject* object)
    {
        auto& data = mod.GetData();
        ApplyData(data, object);
    }

    void DragonWildsBlueprintModLoader::ApplyData(const nlohmann::json& data, RC::Unreal::UObject* object, bool resolveWidgetTemplates)
    {
        auto objectClass = object->GetClassPrivate();
        if (!objectClass)
        {
            throw std::runtime_error("Cannot apply data, object class was null.");
        }

        auto& objectName = objectClass->GetNamePrivate();

        UECustom::UBlueprintGeneratedClass* blueprintClass = nullptr;
        if (objectClass->IsA(UECustom::UBlueprintGeneratedClass::StaticClass()))
        {
            blueprintClass = static_cast<UECustom::UBlueprintGeneratedClass*>(objectClass);
        }

        for (auto& [propertyName, propertyValue] : data.items())
        {
            if (propertyName == "$Append")
            {
                continue;
            }

            auto propertyNameWide = RC::to_generic_string(propertyName);
            auto property = DragonWilds::PropertyHelper::GetPropertyByName(objectClass, propertyNameWide);

            if (!property)
            {
                if (resolveWidgetTemplates && propertyValue.is_object())
                {
                    if (auto widgetTemplate = FindWidgetTemplate(objectClass, propertyNameWide))
                    {
                        ApplyData(propertyValue, widgetTemplate, resolveWidgetTemplates);
                        continue;
                    }
                }

                PS::Log<RC::LogLevel::Warning>(TEXT("Property '{}' does not exist in {}\n"), propertyNameWide, objectName.ToString());
                continue;
            }

            if (auto objectProperty = CastField<FObjectProperty>(property))
            {
                if (propertyValue.is_null())
                {
                    PropertyHelper::CopyJsonValueToContainer(object, property, propertyValue);
                    continue;
                }

                auto objectValue = *property->ContainerPtrToValuePtr<UObject*>(object);
                if (!objectValue)
                {
                    if (resolveWidgetTemplates && propertyValue.is_object())
                    {
                        if (auto widgetTemplate = FindWidgetTemplate(objectClass, propertyNameWide))
                        {
                            ApplyData(propertyValue, widgetTemplate, resolveWidgetTemplates);
                            continue;
                        }
                    }

                    if (blueprintClass)
                    {
                        HandleInheritableComponent(blueprintClass, propertyNameWide, propertyValue);
                    }
                    else
                    {
                        PS::Log<LogLevel::Warning>(TEXT("Property '{}' in {} was null and couldn't be resolved as a blueprint component template.\n"),
                            propertyNameWide, objectName.ToString());
                    }
                }
                else if (propertyValue.is_object()
                    && !propertyValue.contains("ObjectPath")
                    && !propertyValue.contains("ObjectName"))
                {
                    ApplyData(propertyValue, objectValue, resolveWidgetTemplates);
                }
                else
                {
                    PropertyHelper::CopyJsonValueToContainer(object, property, propertyValue);
                }
            }
            else
            {
                PropertyHelper::CopyJsonValueToContainer(object, property, propertyValue);
            }
        }

        auto append = data.find("$Append");
        if (append != data.end())
        {
            PropertyHelper::AppendJsonValuesToContainer(object, objectClass, *append);
        }
    }

    RC::Unreal::UObject* DragonWildsBlueprintModLoader::FindWidgetTemplate(RC::Unreal::UClass* objectClass, const RC::StringType& widgetName)
    {
        for (auto currentClass = objectClass; currentClass; currentClass = static_cast<UClass*>(currentClass->GetSuperStruct()))
        {
            auto treeProperty = CastField<FObjectProperty>(PropertyHelper::GetPropertyByName(currentClass->GetClassPrivate(), TEXT("WidgetTree")));
            if (!treeProperty)
            {
                return nullptr;
            }

            auto widgetTree = *treeProperty->ContainerPtrToValuePtr<UObject*>(currentClass);
            if (!widgetTree)
            {
                continue;
            }

            auto widgetPath = std::format(STR("{}.{}"), widgetTree->GetPathName(), widgetName);
            if (auto widget = UECustom::UObjectGlobals::StaticFindObject(nullptr, nullptr, widgetPath.c_str(), false))
            {
                return widget;
            }
        }

        return nullptr;
    }

    void DragonWildsBlueprintModLoader::HandleInheritableComponent(UECustom::UBlueprintGeneratedClass* bpClass, const RC::StringType& componentName,
                                                         const nlohmann::json& componentData)
    {
        auto& bpClassName = bpClass->GetNamePrivate();

        if (!componentData.is_object())
        {
            PS::Log<LogLevel::Warning>(TEXT("{} failed to apply, provided JSON value wasn't an object\n"), bpClassName.ToString());
            return;
        }

        auto componentFullName = std::format(TEXT("{}_GEN_VARIABLE"), componentName);
        UObject* inheritableComponent = nullptr;

        auto inheritableComponentHandler = bpClass->GetInheritableComponentHandler();
        if (inheritableComponentHandler)
        {
            auto records = inheritableComponentHandler->GetRecords();
            for (auto& record : records)
            {
                if (record.ComponentTemplate.Get() == nullptr) continue;

                if (record.ComponentTemplate.Get()->GetName() == componentFullName)
                {
                    inheritableComponent = record.ComponentTemplate.Get();
                    break;
                }
            }
        }

        if (inheritableComponent)
        {
            ModifyComponent(inheritableComponent, componentData);
            return;
        }

        HandleNodeComponent(bpClass, componentFullName, componentData);
    }

    void DragonWildsBlueprintModLoader::HandleNodeComponent(UECustom::UBlueprintGeneratedClass* bpClass, const RC::StringType& componentName, const nlohmann::json& componentData)
    {
        auto simpleConstructionScript = bpClass->GetSimpleConstructionScript();
        if (!simpleConstructionScript)
        {
            return;
        }

        UObject* nodeComponent = nullptr;

        auto& nodes = simpleConstructionScript->GetAllNodes();
        for (auto& nodeElement : nodes)
        {
            auto nodeComponentTemplate = nodeElement->GetComponentTemplate();
            if (!nodeComponentTemplate)
            {
                continue;
            }

            if (nodeComponentTemplate->GetName() == componentName)
            {
                nodeComponent = nodeComponentTemplate;
                break;
            }
        }

        if (!nodeComponent)
        {
            return;
        }

        ModifyComponent(nodeComponent, componentData);
    }

    void DragonWildsBlueprintModLoader::ModifyComponent(RC::Unreal::UObject* component, const nlohmann::json& componentData)
    {
        for (auto& [innerKey, innerValue] : componentData.items())
        {
            auto componentPropertyName = RC::to_generic_string(innerKey);
            auto componentProperty = PropertyHelper::GetPropertyByName(component->GetClassPrivate(), componentPropertyName.c_str());
            if (!componentProperty)
            {
                PS::Log<LogLevel::Warning>(TEXT("Property {} doesn't exist in {}\n"), componentPropertyName, component->GetName());
                continue;
            }

            PropertyHelper::CopyJsonValueToContainer(component, componentProperty, innerValue);
        }
    }

    void DragonWildsBlueprintModLoader::PostLoad(RC::Unreal::UClass* self)
    {
        PostLoadHook.call(self);

        if (!PostLoadCallback)
        {
            return;
        }

        PostLoadCallback(self);
    }

    void DragonWildsBlueprintModLoader::PostInitComponents(RC::Unreal::AActor* self)
    {
        PostInitComponentsHook.call(self);

        if (!PostInitComponentsCallback)
        {
            return;
        }

        PostInitComponentsCallback(self);
    }
}
