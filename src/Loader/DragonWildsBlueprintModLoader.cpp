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
#include "Utility/InlineHook.h"
#include "Utility/JsonHelpers.h"
#include "Loader/DragonWildsBlueprintModLoader.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Helper/Memory.h"
#include "SDK/Classes/Custom/UBlueprintGeneratedClass.h"
#include "SDK/Classes/Custom/UInheritableComponentHandler.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "Core/JsonPatchDirective.h"
#include "Loader/Spawn/RuntimeSupport.h"

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds {
    DragonWildsBlueprintModLoader::DragonWildsBlueprintModLoader() : DragonWildsModLoaderBase("blueprints")
    {
        SetDisplayName(TEXT("Blueprint Mod Loader"));
    }

    DragonWildsBlueprintModLoader::~DragonWildsBlueprintModLoader()
    {
        ResetHooks();
        ActorInitializedObserver = nullptr;

        for (auto* material : m_ghostRoots)
            if (material && material->IsRootSet()) material->ClearRootSet();

        m_modsMap.clear();
    }

    void DragonWildsBlueprintModLoader::SetActorInitializedObserver(
        std::function<void(AActor*)> observer)
    {
        ActorInitializedObserver = std::move(observer);
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
            const auto isPatch = [](const nlohmann::json& item) {
                return item.is_object() && (item.contains("$Patch") || item.contains("$Target"));
            };
            if (isPatch(data) || std::any_of(data.begin(), data.end(), isPatch)) {
                PS::Log<LogLevel::Warning>(STR("Blueprint $Patch rules changed in {}. Restart the game to reload rules and existing actors.\n"), modName);
                return;
            }
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
        // Require both hooks before enabling writes.
        HooksReady.store(false, std::memory_order_release);
        try
        {
            if (!HookPostLoad())
            {
                ResetHooks();
                PS::Log<LogLevel::Error>(TEXT("Cannot hook UBlueprintGeneratedClass::PostLoad which means blueprint mods will not function properly.\n"));
                return false;
            }

            if (!HookPostInitComponents())
            {
                ResetHooks();
                PS::Log<LogLevel::Error>(TEXT("Cannot hook AActor::PostInitComponents which means blueprint mods will not function properly.\n"));
                return false;
            }

            HooksReady.store(true, std::memory_order_release);
            return true;
        }
        catch (...)
        {
            ResetHooks();
            throw;
        }
    }

    void DragonWildsBlueprintModLoader::ResetHooks()
    {
        HooksReady.store(false, std::memory_order_release);
        PostInitComponentsHook = {};
        PostLoadHook = {};
        PostInitComponentsCallback = nullptr;
        PostLoadCallback = nullptr;
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

        return PS::InstallInlineHook(PostLoadHook, postloadPtr,
            reinterpret_cast<void*>(PostLoad));
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

            if (ActorInitializedObserver)
            {
                ActorInitializedObserver(self);
            }
            ApplyBlueprintVisualEffect(self);
        };

        return PS::InstallInlineHook(PostInitComponentsHook, postInitCompsPtr,
            reinterpret_cast<void*>(PostInitComponents));
    }

    void DragonWildsBlueprintModLoader::LoadSafe(const nlohmann::json& data)
    {
        if (data.is_array()) { for (const auto& entry : data) LoadSafe(entry); return; }
        static constexpr std::array<std::string_view, 0> noProtected{};
        if (const auto patch = JsonPatchDirective::Parse(data, noProtected, "blueprint"))
        {
            m_pendingBlueprintPatches.push_back({{patch->Reference, patch->Changes}});
            return;
        }
        for (auto& [assetName, assetData] : data.items())
        {
            if (assetName.starts_with("$"))
            {
                continue;
            }

            auto assetNameWide = RC::to_generic_string(assetName);
            if (const auto patch = JsonPatchDirective::Parse(assetData, noProtected, "blueprint"))
            {
                m_pendingBlueprintPatches.push_back({{patch->Reference, patch->Changes}});
                continue;
            }
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

                PS::Log<LogLevel::Verbose>(STR("Loaded changes to {}\n"), assetNameWide);
            }
        }
    }

    void DragonWildsBlueprintModLoader::LoadUnsafe(const nlohmann::json& data)
    {
        if (data.is_array()) { for (const auto& entry : data) LoadUnsafe(entry); return; }
        if (data.contains("$Patch") || data.contains("$Target")) return;
        for (auto& [assetName, assetData] : data.items())
        {
            if (assetData.is_object() && (assetData.contains("$Patch") || assetData.contains("$Target"))) continue;
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
                ApplyDeferredPatches(defaultObject.Get());

                PS::Log<RC::LogLevel::Verbose>(TEXT("Applied changes to {}\n"), static_cast<UClass*>(asset)->GetNamePrivate().ToString());
            }
        }
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

        for (const auto key : {objectName, FName(objectClass->GetPathName(), FNAME_Find)})
        {
          const auto found = m_modsMap.find(key);
          if (found == m_modsMap.end()) continue;
          for (auto& mod : found->second) {
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
        ApplyDeferredPatches(object);
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
            if (propertyName == "$Append" || propertyName == "$VisualEffect")
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

        if (!HooksReady.load(std::memory_order_acquire) || !PostLoadCallback)
        {
            return;
        }

        PostLoadCallback(self);
    }

    void DragonWildsBlueprintModLoader::PostInitComponents(RC::Unreal::AActor* self)
    {
        PostInitComponentsHook.call(self);

        if (!HooksReady.load(std::memory_order_acquire) || !PostInitComponentsCallback)
        {
            return;
        }

        PostInitComponentsCallback(self);
    }
}
