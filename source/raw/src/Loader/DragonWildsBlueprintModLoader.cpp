#include <regex>
#include <cctype>
#include <Windows.h>
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/UObjectGlobals.hpp"
#include "Unreal/AActor.hpp"
#include "Helpers/String.hpp"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Helper/ActorHelper.h"
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
#include "Runtime/Storefront.h"
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/Property/FEnumProperty.hpp>

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds {
    namespace {
        template<class Layout> void* ResolveLifecycle(uintptr_t** vtable,const Layout& layout,const TCHAR* name) {
            const auto slot=layout.find(name);
            if(!vtable || slot==layout.end() || slot->second%sizeof(void*) || slot->second>8192)
                throw std::runtime_error("Blueprint lifecycle metadata unavailable: "+RC::to_string(name));
            auto* target=GetVirtualFunctionFromVTable(vtable,slot->second/sizeof(void*));
            MEMORY_BASIC_INFORMATION memory{};
            if(!target || !VirtualQuery(target,&memory,sizeof(memory)) || memory.State!=MEM_COMMIT
                || (memory.Protect&(PAGE_GUARD|PAGE_NOACCESS))
                || !(memory.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))
                throw std::runtime_error("Blueprint lifecycle target is not executable: "+RC::to_string(name));
            return target;
        }

        bool RuntimePropertyTypeCompatible(FProperty* delegateProperty, FProperty* functionProperty)
        {
            if (!delegateProperty || !functionProperty) return delegateProperty == functionProperty;
            if (delegateProperty->GetClass() != functionProperty->GetClass()
                || delegateProperty->GetArrayDim() != functionProperty->GetArrayDim())
                return false;

            const auto sameFlag = [&](EPropertyFlags flag) {
                return delegateProperty->HasAnyPropertyFlags(flag)
                    == functionProperty->HasAnyPropertyFlags(flag);
            };
            if (!sameFlag(CPF_OutParm)
                || !sameFlag(CPF_ReferenceParm)
                || !sameFlag(CPF_ConstParm))
                return false;

            if (auto* left = CastField<FStructProperty>(delegateProperty))
            {
                auto* right = CastField<FStructProperty>(functionProperty);
                return right && left->GetStruct() == right->GetStruct();
            }

            if (auto* left = CastField<FClassProperty>(delegateProperty))
            {
                auto* right = CastField<FClassProperty>(functionProperty);
                return right
                    && left->GetMetaClass().Get() == right->GetMetaClass().Get();
            }

            if (auto* left = CastField<FSoftClassProperty>(delegateProperty))
            {
                auto* right = CastField<FSoftClassProperty>(functionProperty);
                return right
                    && left->GetMetaClass().Get() == right->GetMetaClass().Get();
            }

            if (auto* left = CastField<FObjectPropertyBase>(delegateProperty))
            {
                auto* right = CastField<FObjectPropertyBase>(functionProperty);
                return right
                    && left->GetPropertyClass().Get() == right->GetPropertyClass().Get();
            }

            if (PropertyHelper::IsPropertyA(
                    delegateProperty, DragonWilds::StaticClassStorage::EnumPropertyStaticClass))
            {
                if (!PropertyHelper::IsPropertyA(
                        functionProperty, DragonWilds::StaticClassStorage::EnumPropertyStaticClass))
                    return false;

                // UE4SS 3.0.1 exposes FEnumProperty layout but does not mark it
                // as an FFieldDerivative for CastField's concept constraints.
                // Runtime field-class verification above makes this cast safe.
                auto* left = reinterpret_cast<FEnumProperty*>(delegateProperty);
                auto* right = reinterpret_cast<FEnumProperty*>(functionProperty);
                return left->GetEnum() == right->GetEnum();
            }

            if (auto* left = CastField<FByteProperty>(delegateProperty))
            {
                auto* right = CastField<FByteProperty>(functionProperty);
                return right && left->GetEnum().Get() == right->GetEnum().Get();
            }

            if (auto* left = CastField<FArrayProperty>(delegateProperty))
            {
                auto* right = CastField<FArrayProperty>(functionProperty);
                return right && RuntimePropertyTypeCompatible(left->GetInner(), right->GetInner());
            }

            if (auto* left = CastField<FSetProperty>(delegateProperty))
            {
                auto* right = CastField<FSetProperty>(functionProperty);
                return right && RuntimePropertyTypeCompatible(
                    left->GetElementProp(), right->GetElementProp());
            }

            if (auto* left = CastField<FMapProperty>(delegateProperty))
            {
                auto* right = CastField<FMapProperty>(functionProperty);
                return right
                    && RuntimePropertyTypeCompatible(left->GetKeyProp(), right->GetKeyProp())
                    && RuntimePropertyTypeCompatible(left->GetValueProp(), right->GetValueProp());
            }

            // Primitive reflected property classes already matched above. Raw
            // byte offsets and ParmsSize are deliberately not part of delegate
            // compatibility because Unreal may pad equivalent signatures
            // differently between generated functions and delegate signatures.
            return true;
        }

        std::vector<FProperty*> RuntimeCallableParameters(UFunction* function)
        {
            std::vector<FProperty*> result;
            if (!function) return result;
            for (auto* property : TFieldRange<FProperty>(
                function, EFieldIterationFlags::Default))
            {
                if (property->HasAnyPropertyFlags(CPF_Parm)
                    && !property->HasAnyPropertyFlags(CPF_ReturnParm))
                    result.push_back(property);
            }
            return result;
        }

        bool RuntimeCallableCompatible(UFunction* delegateSignature, UFunction* targetFunction)
        {
            if (!delegateSignature || !targetFunction) return false;

            const auto delegateParameters = RuntimeCallableParameters(delegateSignature);
            const auto targetParameters = RuntimeCallableParameters(targetFunction);

            // Match UE4SS's delegate Add behavior without permitting an unsafe
            // target that requires arguments the delegate will never provide.
            // Unreal can pass a larger delegate parameter buffer to a function
            // that consumes only its leading subset; the extra event arguments
            // are simply unused by the target UFunction.
            if (targetParameters.size() > delegateParameters.size()) return false;

            for (size_t index = 0; index < targetParameters.size(); ++index)
                if (!RuntimePropertyTypeCompatible(
                    delegateParameters[index], targetParameters[index]))
                    return false;

            auto* delegateReturn = delegateSignature->GetReturnProperty();
            auto* targetReturn = targetFunction->GetReturnProperty();

            // A target must not introduce a return value when the delegate has
            // none. If both have one, keep strict reflected type compatibility.
            if (!delegateReturn && targetReturn) return false;
            if (delegateReturn && targetReturn
                && !RuntimePropertyTypeCompatible(delegateReturn, targetReturn))
                return false;
            return true;
        }

        bool RuntimeObjectUsable(UObject* object)
        {
            return object
                && object->GetClassPrivate()
                && !object->HasAnyFlags(static_cast<EObjectFlags>(
                    RF_ClassDefaultObject | RF_ArchetypeObject
                    | RF_BeginDestroyed | RF_FinishDestroyed
                    | RF_NeedLoad | RF_NeedPostLoad | RF_NeedInitialization));
        }

        std::vector<UObject*> RuntimeObjectArrayValues(
            UObject* container,
            const RC::StringType& propertyName,
            int32_t maximum = 512)
        {
            std::vector<UObject*> result;
            if (!RuntimeObjectUsable(container)) return result;

            auto* arrayProperty = CastField<FArrayProperty>(
                PropertyHelper::GetPropertyByName(
                    container->GetClassPrivate(), propertyName));
            auto* objectInner = arrayProperty
                ? CastField<FObjectPropertyBase>(arrayProperty->GetInner())
                : nullptr;
            if (!arrayProperty || !objectInner
                || arrayProperty->GetArrayDim() != 1
                || objectInner->GetElementSize() != sizeof(UObject*))
                return result;

            FScriptArrayHelper helper(
                arrayProperty,
                arrayProperty->ContainerPtrToValuePtr<void>(container));
            if (helper.Num() < 0 || helper.Num() > maximum)
                throw std::runtime_error(std::format(
                    "Runtime widget object array '{}' exceeded the safety limit",
                    RC::to_string(propertyName)));

            result.reserve(static_cast<size_t>(helper.Num()));
            for (int32_t index = 0; index < helper.Num(); ++index)
            {
                auto* value = objectInner->GetObjectPropertyValue(helper.GetRawPtr(index));
                if (RuntimeObjectUsable(value))
                    result.push_back(value);
            }
            return result;
        }

        bool RuntimeOuterChainContains(UObject* candidate, UObject* expectedOuter)
        {
            if (!candidate || !expectedOuter) return false;
            auto* current = candidate->GetOuterPrivate();
            for (int depth = 0; current && depth < 16; ++depth)
            {
                if (current == expectedOuter) return true;
                current = current->GetOuterPrivate();
            }
            return false;
        }

        bool RuntimeUiNameValid(const std::string& name)
        {
            if (name.empty() || name.size() > 64) return false;
            return std::all_of(name.begin(), name.end(), [](unsigned char value) {
                return std::isalnum(value) || value == '_';
            });
        }

        const TCHAR* RuntimeUiPrimitivePath(std::string type)
        {
            std::transform(type.begin(), type.end(), type.begin(),
                [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
            if (type == "canvaspanel" || type == "canvas") return TEXT("/Script/UMG.CanvasPanel");
            if (type == "border") return TEXT("/Script/UMG.Border");
            if (type == "textblock" || type == "text") return TEXT("/Script/UMG.TextBlock");
            if (type == "image") return TEXT("/Script/UMG.Image");
            if (type == "button") return TEXT("/Script/UMG.Button");
            return nullptr;
        }

        bool RuntimeStorefrontTokenMatches(std::string token)
        {
            std::transform(token.begin(), token.end(), token.begin(),
                [](unsigned char value) { return static_cast<char>(std::tolower(value)); });

            const auto storefront = PS::Storefront::Current();
            if (token == "any" || token == "all" || token == "universal") return true;
            if (token == "gamepass" || token == "wingdk" || token == "xbox")
                return storefront == PS::Storefront::Kind::GamePass;
            if (token == "steam" || token == "steamgog" || token == "gog" || token == "win64")
                return storefront == PS::Storefront::Kind::SteamGog;
            if (token == "unknown")
                return storefront == PS::Storefront::Kind::Unknown;
            throw std::runtime_error("Blueprint $RuntimeWidget $Storefront contains an unsupported value");
        }

        bool RuntimeStorefrontMatches(const nlohmann::json& value)
        {
            if (value.is_string())
                return RuntimeStorefrontTokenMatches(value.get<std::string>());
            if (value.is_array())
            {
                if (value.empty())
                    throw std::runtime_error("Blueprint $RuntimeWidget $Storefront array cannot be empty");
                for (const auto& entry : value)
                {
                    if (!entry.is_string())
                        throw std::runtime_error("Blueprint $RuntimeWidget $Storefront array must contain strings");
                    if (RuntimeStorefrontTokenMatches(entry.get<std::string>()))
                        return true;
                }
                return false;
            }
            throw std::runtime_error("Blueprint $RuntimeWidget $Storefront must be a string or array");
        }
    }
    DragonWildsBlueprintModLoader::DragonWildsBlueprintModLoader() : DragonWildsModLoaderBase("blueprints")
    {
        SetDisplayName(TEXT("Blueprint Mod Loader"));
    }

    DragonWildsBlueprintModLoader::~DragonWildsBlueprintModLoader()
    {
        if (m_runtimeWidgetCallbackId != Hook::ERROR_ID)
            Hook::UnregisterCallback(m_runtimeWidgetCallbackId);
        if (m_worldTeardownCallbackId != Hook::ERROR_ID)
            Hook::UnregisterCallback(m_worldTeardownCallbackId);
        ResetHooks();
        ActorInitializedObserver = nullptr;
        ActorInitializedObservers.clear();

        ClearWorldVisualEffects();
        ClearRuntimeUiInstances();
        ClearRuntimeWidgetState();
        m_reportedRuntimeWidgetFailures.clear();
        m_reportedRuntimeUiFailures.clear();
        m_runtimeWidgetRules.clear();
        m_runtimeUiRules.clear();
        m_modsMap.clear();
    }

    void DragonWildsBlueprintModLoader::ClearWorldVisualEffects()
    {
        for (const auto& ref : m_ghostRoots)
            if (auto* material=ref.Get()) if (material->IsRootSet()) material->ClearRootSet();
        m_ghostRoots.clear();
        m_ghostMaterials.clear();
    }

    void DragonWildsBlueprintModLoader::ClearRuntimeWidgetState()
    {
        m_runtimeWidgetActiveRules.clear();
        m_runtimeWidgetCompletedRules.clear();
        m_runtimeWidgetObservedTargets.clear();
    }

    void DragonWildsBlueprintModLoader::ClearRuntimeUiInstances()
    {
        m_runtimeUiTearingDown = true;
        for (auto& [key, instance] : m_runtimeUiInstances)
        {
            auto* widget = instance.Widget.Get();
            if (!widget) continue;
            try
            {
                const auto functionName = RC::StringType(TEXT("RemoveFromParent"));
                auto* function = widget->GetFunctionByNameInChain(functionName.c_str());
                if (function && function->GetParmsSize() == 0)
                    ActorHelper::FunctionCall(widget, function).Invoke();
            }
            catch (...) {}
        }
        m_runtimeUiInstances.clear();
        m_runtimeUiActiveRules.clear();
        m_runtimeUiTearingDown = false;
    }

    void DragonWildsBlueprintModLoader::SetActorInitializedObserver(
        std::function<void(AActor*)> observer)
    {
        ActorInitializedObserver = std::move(observer);
    }

    uint64_t DragonWildsBlueprintModLoader::RegisterActorInitializedObserver(
        std::function<void(AActor*)> observer)
    {
        const auto observerId = NextActorObserverId++;
        ActorInitializedObservers.emplace(observerId, std::move(observer));
        return observerId;
    }

    void DragonWildsBlueprintModLoader::UnregisterActorInitializedObserver(uint64_t observerId)
    {
        if (observerId != 0)
        {
            ActorInitializedObservers.erase(observerId);
        }
    }

    void DragonWildsBlueprintModLoader::OnLoad(const std::filesystem::path& loaderPath, const RC::StringType& modName, const EEngineLifecyclePhase& engineLifecyclePhase)
    {
        if (engineLifecyclePhase == EEngineLifecyclePhase::PostEngineInit)
        {
            PS::JsonHelpers::ParseJsonFilesInPath(loaderPath, [&](const nlohmann::json& data) {
                LoadSafe(data, modName);
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
            const auto requiresRestart = [](const nlohmann::json& item) {
                return item.is_object()
                    && (item.contains("$Patch")
                        || item.contains("$Target")
                        || item.contains("$RuntimeWidget")
                        || item.contains("$RuntimeUI"));
            };
            if (requiresRestart(data) || std::any_of(data.begin(), data.end(), requiresRestart)) {
                PS::Log<LogLevel::Warning>(STR("Blueprint runtime/$Patch rules changed in {}. Restart the game to reload rules and existing objects.\n"), modName);
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
            Hook::FCallbackOptions options{};
            options.OwnerModName = TEXT("RuneSchema");
            options.HookName = TEXT("BlueprintGhostWorldTeardown");
            m_worldTeardownCallbackId = Hook::RegisterInitGameStatePreCallback(
                [this](Hook::TCallbackIterationData<void>&, AGameModeBase*) {
                    ClearWorldVisualEffects();
                    ClearRuntimeUiInstances();
                    ClearRuntimeWidgetState();
                }, options);
            if (m_worldTeardownCallbackId == Hook::ERROR_ID)
                PS::Log<LogLevel::Warning>(
                    TEXT("Blueprint ghost world-teardown cleanup could not be registered.\n"));

            Hook::FCallbackOptions runtimeOptions{};
            runtimeOptions.OwnerModName = TEXT("RuneSchema");
            runtimeOptions.HookName = TEXT("BlueprintRuntimeWidget");
            m_runtimeWidgetCallbackId = Hook::RegisterProcessEventPostCallback(
                [this](Hook::TCallbackIterationData<void>&, UObject* source, UFunction* function, void*) {
                    try { ObserveRuntimeWidgetEvent(source, function); }
                    catch (const std::exception& error) {
                        PS::RoutineLog("blueprints",
                            STR("Blueprint $RuntimeWidget event failed: {}.\n"),
                            PS::ToWideSafe(error.what()));
                    }
                }, runtimeOptions);
            if (m_runtimeWidgetCallbackId == Hook::ERROR_ID)
                throw std::runtime_error("Blueprint $RuntimeWidget ProcessEvent observer registration failed");

            return true;
        }
        catch (...)
        {
            if (m_runtimeWidgetCallbackId != Hook::ERROR_ID) {
                Hook::UnregisterCallback(m_runtimeWidgetCallbackId);
                m_runtimeWidgetCallbackId = Hook::ERROR_ID;
            }
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

        void* postloadPtr = ResolveLifecycle(vtable,UObject::VTableLayoutMap,TEXT("PostLoad"));
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

        void* postInitCompsPtr = ResolveLifecycle(vtable,AActor::VTableLayoutMap,TEXT("PostInitializeComponents"));
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
            for (const auto& [observerId, observer] : ActorInitializedObservers)
            {
                if (observer)
                {
                    observer(self);
                }
            }
            ApplyBlueprintVisualEffect(self);
        };

        return PS::InstallInlineHook(PostInitComponentsHook, postInitCompsPtr,
            reinterpret_cast<void*>(PostInitComponents));
    }

    void DragonWildsBlueprintModLoader::LoadSafe(const nlohmann::json& data, const RC::StringType& modName)
    {
        if (data.is_array()) { for (const auto& entry : data) LoadSafe(entry, modName); return; }
        static constexpr std::array<std::string_view, 0> noProtected{};
        if (const auto patch = JsonPatchDirective::Parse(data, noProtected, "blueprint"))
        {
            m_pendingBlueprintPatches.push_back({{patch->Reference, patch->Changes}});
            WarnPatchConflicts(m_patchConflicts, "blueprints:" + patch->Reference, patch->Changes, RC::to_string(modName), false);
            return;
        }
        for (auto& [assetName, assetData] : data.items())
        {
            if (assetName.starts_with("$"))
            {
                continue;
            }

            auto assetNameWide = RC::to_generic_string(assetName);
            nlohmann::json staticData = assetData;

            if (assetData.is_object())
            {
                if (const auto runtime = assetData.find("$RuntimeWidget"); runtime != assetData.end())
                {
                    RegisterRuntimeWidgetRules(assetName, *runtime, modName);
                    staticData.erase("$RuntimeWidget");
                }
                if (const auto runtimeUi = assetData.find("$RuntimeUI"); runtimeUi != assetData.end())
                {
                    RegisterRuntimeUiRules(assetName, *runtimeUi, modName);
                    staticData.erase("$RuntimeUI");
                }
            }

            if (const auto patch = JsonPatchDirective::Parse(staticData, noProtected, "blueprint"))
            {
                m_pendingBlueprintPatches.push_back({{patch->Reference, patch->Changes}});
                WarnPatchConflicts(m_patchConflicts, "blueprints:" + patch->Reference, patch->Changes, RC::to_string(modName), false);
                continue;
            }
            if (!assetNameWide.starts_with(TEXT("/Game/")) && !staticData.empty())
            {
                auto assetFName = FName(assetNameWide, FNAME_Add);
                auto newMod = DragonWildsBlueprintMod(assetFName, staticData);
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
            }
        }
    }

    void DragonWildsBlueprintModLoader::RegisterRuntimeWidgetRules(
        const std::string& identity,
        const nlohmann::json& runtimeWidgets,
        const RC::StringType& modName)
    {
        if (!runtimeWidgets.is_object())
            throw std::runtime_error("Blueprint $RuntimeWidget must be an object keyed by widget path");

        auto classPath = identity;
        if (classPath.starts_with("/Game/"))
        {
            if (classPath.find('.') == std::string::npos)
            {
                const auto name = classPath.substr(classPath.find_last_of('/') + 1);
                classPath += "." + name + "_C";
            }
            if (!classPath.ends_with("_C"))
                throw std::runtime_error("Blueprint $RuntimeWidget path must identify a generated class ending _C");
        }
        else
        {
            if (classPath.contains('/') || classPath.contains('.') || !classPath.ends_with("_C"))
                throw std::runtime_error("Blueprint $RuntimeWidget requires a class name ending _C or a /Game/ class path");
        }

        if (const auto storefront = runtimeWidgets.find("$Storefront");
            storefront != runtimeWidgets.end() && !RuntimeStorefrontMatches(*storefront))
        {
            const auto message = runtimeWidgets.value("$SkipMessage", std::string{});
            if (!message.empty() && PS::Storefront::Current() != PS::Storefront::Kind::Unknown)
                PS::Log<LogLevel::Normal>(STR("[{}] {}\n"), modName, PS::ToWideSafe(message.c_str()));
            else
                PS::Log<LogLevel::Verbose>(
                    STR("Blueprint $RuntimeWidget rules from '{}' skipped on storefront {}.\n"),
                    modName,
                    PS::ToWideSafe(PS::Storefront::Name(PS::Storefront::Current())));
            return;
        }

        const auto ownerClass = FName(to_generic_string(classPath), FNAME_Add);
        for (const auto& [widgetPath, ruleData] : runtimeWidgets.items())
        {
            if (widgetPath == "$Storefront" || widgetPath == "$SkipMessage")
                continue;
            if (widgetPath.empty() || widgetPath.starts_with("$"))
                throw std::runtime_error("Blueprint $RuntimeWidget metadata/widget path was invalid");
            if (!ruleData.is_object())
                throw std::runtime_error("Blueprint $RuntimeWidget rule must be an object");

            m_runtimeWidgetRules.push_back({
                ownerClass,
                to_generic_string(widgetPath),
                ruleData,
                modName
            });
        }
    }

    void DragonWildsBlueprintModLoader::RegisterRuntimeUiRules(
        const std::string& identity,
        const nlohmann::json& runtimeUi,
        const RC::StringType& modName)
    {
        if (!runtimeUi.is_object())
            throw std::runtime_error("Blueprint $RuntimeUI must be an object keyed by UI name");

        auto classPath = identity;
        if (classPath.starts_with("/Game/"))
        {
            if (classPath.find('.') == std::string::npos)
            {
                const auto name = classPath.substr(classPath.find_last_of('/') + 1);
                classPath += "." + name + "_C";
            }
            if (!classPath.ends_with("_C"))
                throw std::runtime_error("Blueprint $RuntimeUI path must identify a generated class ending _C");
        }
        else
        {
            if (classPath.contains('/') || classPath.contains('.') || !classPath.ends_with("_C"))
                throw std::runtime_error("Blueprint $RuntimeUI requires a class name ending _C or a /Game/ class path");
        }

        if (const auto storefront = runtimeUi.find("$Storefront");
            storefront != runtimeUi.end() && !RuntimeStorefrontMatches(*storefront))
        {
            const auto message = runtimeUi.value("$SkipMessage", std::string{});
            if (!message.empty() && PS::Storefront::Current() != PS::Storefront::Kind::Unknown)
                PS::Log<LogLevel::Normal>(STR("[{}] {}\n"), modName, PS::ToWideSafe(message.c_str()));
            return;
        }

        const auto ownerClass = FName(to_generic_string(classPath), FNAME_Add);
        size_t count = 0;
        for (const auto& [uiName, ruleData] : runtimeUi.items())
        {
            if (uiName == "$Storefront" || uiName == "$SkipMessage")
                continue;
            if (!RuntimeUiNameValid(uiName))
                throw std::runtime_error("Blueprint $RuntimeUI names must use only letters, numbers, and underscores");
            if (!ruleData.is_object())
                throw std::runtime_error("Blueprint $RuntimeUI rule must be an object");
            if (++count > 16)
                throw std::runtime_error("Blueprint $RuntimeUI block exceeds the 16-widget safety limit");

            const auto root = ruleData.find("Root");
            if (root == ruleData.end() || !root->is_object())
                throw std::runtime_error("Blueprint $RuntimeUI rule requires a Root object");

            m_runtimeUiRules.push_back({
                ownerClass,
                uiName,
                ruleData,
                modName
            });
        }
    }

    RC::Unreal::UObject* DragonWildsBlueprintModLoader::ResolveRuntimeWidgetPath(
        UObject* owner,
        const RC::StringType& widgetPath)
    {
        if (!owner || widgetPath.empty()) return nullptr;

        UObject* current = owner;
        size_t offset = 0;
        while (offset < widgetPath.size())
        {
            const auto dot = widgetPath.find(TEXT('.'), offset);
            const auto length = dot == RC::StringType::npos ? widgetPath.size() - offset : dot - offset;
            if (!length) return nullptr;

            const auto segment = widgetPath.substr(offset, length);
            auto* objectProperty = CastField<FObjectProperty>(
                PropertyHelper::GetPropertyByName(current->GetClassPrivate(), segment));
            if (!objectProperty) return nullptr;

            current = objectProperty->GetObjectPropertyValue(
                objectProperty->ContainerPtrToValuePtr<void>(current));
            if (!current) return nullptr;

            if (dot == RC::StringType::npos) break;
            offset = dot + 1;
        }

        return current;
    }

    bool DragonWildsBlueprintModLoader::RuntimeWidgetPathContains(
        UObject* owner,
        UObject* candidate,
        const RC::StringType& widgetPath)
    {
        if (!owner || !candidate || widgetPath.empty()) return false;
        if (owner == candidate) return true;

        UObject* current = owner;
        size_t offset = 0;
        while (offset < widgetPath.size())
        {
            const auto dot = widgetPath.find(TEXT('.'), offset);
            const auto length = dot == RC::StringType::npos ? widgetPath.size() - offset : dot - offset;
            if (!length) return false;

            const auto segment = widgetPath.substr(offset, length);
            auto* objectProperty = CastField<FObjectProperty>(
                PropertyHelper::GetPropertyByName(current->GetClassPrivate(), segment));
            if (!objectProperty) return false;

            current = objectProperty->GetObjectPropertyValue(
                objectProperty->ContainerPtrToValuePtr<void>(current));
            if (!current) return false;
            if (current == candidate) return true;

            if (dot == RC::StringType::npos) break;
            offset = dot + 1;
        }
        return false;
    }

    UObject* DragonWildsBlueprintModLoader::FindRuntimeWidgetOwner(
        UObject* source,
        const FName& ownerClass)
    {
        UObject* current = source;
        for (int depth = 0; current && depth < 16; ++depth)
        {
            auto* type = current->GetClassPrivate();
            if (type)
            {
                const auto typeName = type->GetNamePrivate();
                const auto typePath = FName(type->GetPathName(), FNAME_Find);
                if (ownerClass == typeName || ownerClass == typePath)
                    return current;
            }
            current = current->GetOuterPrivate();
        }
        return nullptr;
    }

    bool DragonWildsBlueprintModLoader::RuntimeWidgetSelectorMatches(
        UObject* candidate,
        const nlohmann::json& selector) const
    {
        if (!RuntimeObjectUsable(candidate)) return false;

        const auto name = selector.value("Name", std::string{});
        const auto className = selector.value("Class", std::string{});
        if (name.empty() && className.empty())
            throw std::runtime_error("Blueprint $RuntimeWidget $Find requires Name or Class");

        if (!name.empty())
        {
            const auto wanted = FName(to_generic_string(name), FNAME_Find);
            if (wanted == NAME_None || candidate->GetFName() != wanted)
                return false;
        }

        if (!className.empty())
        {
            auto* type = candidate->GetClassPrivate();
            if (!type) return false;
            const auto actualName = RC::to_string(type->GetName());
            const auto actualPath = RC::to_string(type->GetPathName());
            if (className != actualName && className != actualPath)
                return false;
        }

        return true;
    }

    UObject* DragonWildsBlueprintModLoader::FindRuntimeWidgetTarget(
        UObject* owner,
        const RuntimeWidgetRule& rule)
    {
        const auto find = rule.Data.find("$Find");
        if (find == rule.Data.end()) return nullptr;
        if (!find->is_object())
            throw std::runtime_error("Blueprint $RuntimeWidget $Find must be an object");

        auto scope = find->value("Scope", std::string{});
        std::transform(scope.begin(), scope.end(), scope.begin(),
            [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        if (scope.empty())
            throw std::runtime_error("Blueprint $RuntimeWidget $Find requires Scope");

        const auto name = find->value("Name", std::string{});
        const auto className = find->value("Class", std::string{});
        if (name.empty() && className.empty())
            throw std::runtime_error("Blueprint $RuntimeWidget $Find requires Name or Class");

        std::vector<UObject*> matches;
        std::unordered_set<UObject*> seen;
        const auto addMatch = [&](UObject* candidate) {
            if (!RuntimeWidgetSelectorMatches(candidate, *find)) return;
            if (seen.emplace(candidate).second)
                matches.push_back(candidate);
        };

        if (scope == "widgettree" || scope == "widget-tree")
        {
            if (name.empty())
                throw std::runtime_error("Blueprint $RuntimeWidget WidgetTree discovery requires Name");

            auto* treeProperty = CastField<FObjectPropertyBase>(
                PropertyHelper::GetPropertyByName(
                    owner->GetClassPrivate(), TEXT("WidgetTree")));
            auto* widgetTree = treeProperty
                ? treeProperty->GetObjectPropertyValue(
                    treeProperty->ContainerPtrToValuePtr<void>(owner))
                : nullptr;
            if (!RuntimeObjectUsable(widgetTree)) return nullptr;

            const auto widgetName = to_generic_string(name);
            const auto directPath = std::format(
                STR("{}.{}"), widgetTree->GetPathName(), widgetName);
            if (auto* direct = UECustom::UObjectGlobals::StaticFindObject(
                    nullptr, nullptr, directPath.c_str(), false);
                RuntimeObjectUsable(direct)
                    && RuntimeOuterChainContains(direct, widgetTree))
            {
                addMatch(direct);
            }

            if (matches.empty())
            {
                const auto wantedName = FName(widgetName, FNAME_Find);
                if (wantedName != NAME_None)
                {
                    RC::Unreal::UObjectGlobals::ForEachUObject(
                        [&](UObject* candidate, int32_t, int32_t) -> LoopAction {
                            if (!RuntimeObjectUsable(candidate)
                                || candidate->GetFName() != wantedName
                                || !RuntimeOuterChainContains(candidate, widgetTree))
                                return LoopAction::Continue;
                            addMatch(candidate);
                            return matches.size() > 1
                                ? LoopAction::Break : LoopAction::Continue;
                        });
                }
            }
        }
        else if (scope == "hud")
        {
            auto* controllerClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                nullptr, nullptr, TEXT("/Script/Dominion.DominionPlayerController"));
            if (!controllerClass) return nullptr;

            auto* ownerWorld = owner->GetWorld();
            TArray<UObject*> controllers;
            UECustom::UObjectGlobals::GetObjectsOfClass(
                controllerClass, controllers, true);
            if (controllers.Num() > 64)
                throw std::runtime_error("Blueprint $RuntimeWidget HUD discovery exceeded the controller safety limit");

            for (auto* controller : controllers)
            {
                if (!RuntimeObjectUsable(controller)
                    || (ownerWorld && controller->GetWorld() != ownerWorld))
                    continue;

                auto* hudProperty = CastField<FObjectPropertyBase>(
                    PropertyHelper::GetPropertyByName(
                        controller->GetClassPrivate(), TEXT("MyHUD")));
                auto* hud = hudProperty
                    ? hudProperty->GetObjectPropertyValue(
                        hudProperty->ContainerPtrToValuePtr<void>(controller))
                    : nullptr;
                if (!RuntimeObjectUsable(hud)) continue;

                for (auto* candidate : RuntimeObjectArrayValues(
                        hud, TEXT("HUDWidgetRefs"), 256))
                    addMatch(candidate);
            }
        }
        else if (scope == "commonui" || scope == "common-ui"
            || scope == "activatable")
        {
            auto* containerClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                nullptr, nullptr,
                TEXT("/Script/CommonUI.CommonActivatableWidgetContainerBase"));
            if (!containerClass) return nullptr;

            auto* ownerWorld = owner->GetWorld();
            TArray<UObject*> containers;
            UECustom::UObjectGlobals::GetObjectsOfClass(
                containerClass, containers, true);
            if (containers.Num() > 128)
                throw std::runtime_error("Blueprint $RuntimeWidget CommonUI discovery exceeded the container safety limit");

            for (auto* container : containers)
            {
                if (!RuntimeObjectUsable(container)
                    || (ownerWorld && container->GetWorld()
                        && container->GetWorld() != ownerWorld))
                    continue;

                for (auto* candidate : RuntimeObjectArrayValues(
                        container, TEXT("WidgetList"), 256))
                    addMatch(candidate);
            }
        }
        else
        {
            throw std::runtime_error(
                "Blueprint $RuntimeWidget $Find Scope must be WidgetTree, HUD, or CommonUI");
        }

        if (matches.size() > 1)
            throw std::runtime_error("Blueprint $RuntimeWidget $Find matched more than one live object");
        return matches.empty() ? nullptr : matches.front();
    }

    UObject* DragonWildsBlueprintModLoader::ResolveRuntimeWidgetTarget(
        UObject* owner,
        const RuntimeWidgetRule& rule)
    {
        if (auto* direct = ResolveRuntimeWidgetPath(owner, rule.WidgetPath))
            return direct;

        auto* discovered = FindRuntimeWidgetTarget(owner, rule);
        if (discovered)
        {
            m_runtimeWidgetObservedTargets[discovered] = {
                PS::WeakObject(discovered),
                PS::WeakObject(owner)
            };
        }
        return discovered;
    }

    UObject* DragonWildsBlueprintModLoader::ResolveRuntimeWidgetCallTarget(
        UObject* owner,
        UObject* widget,
        const std::string& targetPath)
    {
        if (targetPath.empty() || targetPath == "." || targetPath == "$Self"
            || targetPath == "$Target")
            return widget;
        if (targetPath == "$Owner")
            return owner;
        return ResolveRuntimeWidgetPath(owner, to_generic_string(targetPath));
    }

    bool DragonWildsBlueprintModLoader::RuntimeWidgetRuleMatchesEvent(
        const RuntimeWidgetRule& rule,
        UFunction* function)
    {
        const auto when = rule.Data.find("$When");
        if (when == rule.Data.end()) return true;

        const auto matchesName = [&](const std::string& expected) {
            std::string lowered = expected;
            std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
            if (lowered == "any" || lowered == "always" || lowered == "resolved")
                return true;
            return function && RC::to_string(function->GetName()) == expected;
        };

        if (when->is_string())
            return matchesName(when->get<std::string>());

        if (!when->is_object())
            throw std::runtime_error("Blueprint $RuntimeWidget $When must be a function name or object");

        const auto functionFilter = when->find("Function");
        if (functionFilter == when->end())
            throw std::runtime_error("Blueprint $RuntimeWidget $When object requires Function");

        if (functionFilter->is_string())
            return matchesName(functionFilter->get<std::string>());
        if (functionFilter->is_array())
        {
            if (functionFilter->empty())
                throw std::runtime_error("Blueprint $RuntimeWidget $When Function array cannot be empty");
            for (const auto& entry : *functionFilter)
            {
                if (!entry.is_string())
                    throw std::runtime_error("Blueprint $RuntimeWidget $When Function array must contain strings");
                if (matchesName(entry.get<std::string>())) return true;
            }
            return false;
        }

        throw std::runtime_error("Blueprint $RuntimeWidget $When Function must be a string or array");
    }

    bool DragonWildsBlueprintModLoader::RuntimeUiRuleMatchesEvent(
        const RuntimeUiRule& rule,
        UFunction* function)
    {
        RuntimeWidgetRule proxy{
            rule.OwnerClass,
            {},
            rule.Data,
            rule.ModName
        };
        return RuntimeWidgetRuleMatchesEvent(proxy, function);
    }

    std::string DragonWildsBlueprintModLoader::RuntimeWidgetRuleKey(
        UObject* owner,
        const RuntimeWidgetRule& rule) const
    {
        return std::to_string(reinterpret_cast<uintptr_t>(owner))
            + ":" + std::to_string(reinterpret_cast<uintptr_t>(&rule))
            + ":" + RC::to_string(rule.ModName)
            + ":" + RC::to_string(rule.WidgetPath);
    }

    void DragonWildsBlueprintModLoader::ApplyRuntimeWidgetActivation(
        UObject* widget,
        const RuntimeWidgetRule& rule)
    {
        const auto activation = rule.Data.find("$Activate");
        if (activation == rule.Data.end()) return;
        if (!activation->is_boolean())
            throw std::runtime_error("Blueprint $RuntimeWidget $Activate must be true or false");

        const auto functionName = activation->get<bool>()
            ? RC::StringType(TEXT("ActivateWidget"))
            : RC::StringType(TEXT("DeactivateWidget"));
        auto* function = widget->GetFunctionByNameInChain(functionName.c_str());
        if (!function)
            throw std::runtime_error("Blueprint $RuntimeWidget $Activate target is not a CommonUI activatable widget");

        if (function->GetParmsSize() != 0)
            throw std::runtime_error("Blueprint $RuntimeWidget activation function has an unexpected parameter contract");

        ActorHelper::FunctionCall(widget, function).Invoke();
    }

    void DragonWildsBlueprintModLoader::ApplyRuntimeWidgetCalls(
        UObject* owner,
        UObject* widget,
        const RuntimeWidgetRule& rule)
    {
        const auto calls = rule.Data.find("$Call");
        if (calls == rule.Data.end()) return;

        const auto invoke = [&](const nlohmann::json& callData) {
            if (!callData.is_object())
                throw std::runtime_error("Blueprint $RuntimeWidget $Call entries must be objects");

            const auto functionName = callData.value("Function", std::string{});
            if (functionName.empty())
                throw std::runtime_error("Blueprint $RuntimeWidget $Call requires Function");

            const auto targetPath = callData.value("Target", std::string{"$Self"});
            auto* targetObject = ResolveRuntimeWidgetCallTarget(owner, widget, targetPath);
            if (!targetObject)
                throw std::runtime_error("Blueprint $RuntimeWidget $Call target could not be resolved");

            const auto functionNameWide = to_generic_string(functionName);
            auto* targetFunction = targetObject->GetFunctionByNameInChain(functionNameWide.c_str());
            if (!targetFunction)
                throw std::runtime_error("Blueprint $RuntimeWidget $Call Function was not found on target");

            if (targetFunction->GetReturnProperty())
                throw std::runtime_error("Blueprint $RuntimeWidget $Call does not support return-valued functions yet");
            for (auto* property : RuntimeCallableParameters(targetFunction))
            {
                if (property->HasAnyPropertyFlags(CPF_OutParm))
                    throw std::runtime_error("Blueprint $RuntimeWidget $Call does not support output parameters yet");
            }

            auto args = callData.find("Args");
            if (args != callData.end() && !args->is_object())
                throw std::runtime_error("Blueprint $RuntimeWidget $Call Args must be an object");

            ActorHelper::FunctionCall call(targetObject, targetFunction);
            if (args != callData.end())
            {
                for (const auto& [argName, argValue] : args->items())
                {
                    const auto argNameWide = to_generic_string(argName);
                    auto* property = targetFunction->FindProperty(FName(argNameWide, FNAME_Find));
                    if (!property
                        || !property->HasAnyPropertyFlags(CPF_Parm)
                        || property->HasAnyPropertyFlags(CPF_ReturnParm))
                        throw std::runtime_error(std::format(
                            "Blueprint $RuntimeWidget $Call argument '{}' is not an input parameter",
                            argName));
                    call.JsonArg(argNameWide.c_str(), argValue);
                }
            }
            call.Invoke();
        };

        if (calls->is_object())
        {
            invoke(*calls);
            return;
        }
        if (calls->is_array())
        {
            if (calls->empty())
                throw std::runtime_error("Blueprint $RuntimeWidget $Call array cannot be empty");
            for (const auto& call : *calls) invoke(call);
            return;
        }

        throw std::runtime_error("Blueprint $RuntimeWidget $Call must be an object or array");
    }

    void DragonWildsBlueprintModLoader::ApplyRuntimeWidgetBinding(
        UObject* owner,
        UObject* widget,
        const RuntimeWidgetRule& rule)
    {
        const auto binding = rule.Data.find("$Bind");
        if (binding == rule.Data.end()) return;
        if (!binding->is_object())
            throw std::runtime_error("Blueprint $RuntimeWidget $Bind must be an object");

        const auto eventName = binding->value("Event", std::string{});
        const auto functionName = binding->value("Function", std::string{});
        const auto targetPath = binding->value("Target", std::string{});
        if (eventName.empty() || functionName.empty())
            throw std::runtime_error("Blueprint $RuntimeWidget $Bind requires Event and Function");

        UObject* targetObject = owner;
        if (!targetPath.empty() && targetPath != "." && targetPath != "$Owner")
        {
            targetObject = ResolveRuntimeWidgetPath(owner, to_generic_string(targetPath));
        }
        if (!targetObject)
            throw std::runtime_error("Blueprint $RuntimeWidget $Bind target could not be resolved");

        auto* delegateProperty = CastField<FMulticastDelegateProperty>(
            PropertyHelper::GetPropertyByName(
                widget->GetClassPrivate(),
                to_generic_string(eventName)));
        if (!delegateProperty)
            throw std::runtime_error("Blueprint $RuntimeWidget $Bind Event is not a multicast delegate");

        const auto functionNameWide = to_generic_string(functionName);
        const auto functionFName = FName(functionNameWide, FNAME_Add);
        auto* targetFunction = targetObject->GetFunctionByNameInChain(functionNameWide.c_str());
        if (!targetFunction)
            throw std::runtime_error("Blueprint $RuntimeWidget $Bind Function was not found on target");

        auto& signaturePtr = delegateProperty->GetSignatureFunction();
        if (!signaturePtr.Get())
            throw std::runtime_error("Blueprint $RuntimeWidget delegate signature was unavailable");

        // Mirror UE4SS XMulticastDelegateProperty::Add: once the delegate,
        // target object, and target UFunction have all resolved, bind the
        // FScriptDelegate directly. UE4SS does not impose an additional
        // reflected-parameter equality gate here, and valid Blueprint events
        // can expose wrapper/signature metadata that differs from the native
        // target function even though Unreal accepts the binding.
        void* propertyValue = delegateProperty->ContainerPtrToValuePtr<void>(widget);
        auto* delegateValue = delegateProperty->GetMulticastDelegate(propertyValue);
        if (!delegateValue)
            throw std::runtime_error("Blueprint $RuntimeWidget delegate value was unavailable");

        for (int32_t index = 0; index < delegateValue->Num(); ++index)
        {
            const auto& existing = delegateValue->InvocationList[index];
            if (existing.GetUObject() == targetObject
                && existing.GetFunctionName() == functionFName)
                return;
        }

        FScriptDelegate scriptDelegate;
        scriptDelegate.BindUFunction(targetObject, functionFName);
        delegateProperty->AddDelegate(scriptDelegate, widget, propertyValue);
    }

    std::string DragonWildsBlueprintModLoader::RuntimeUiRuleKey(
        const RuntimeUiRule& rule) const
    {
        return "ui:" + std::to_string(reinterpret_cast<uintptr_t>(&rule))
            + ":" + RC::to_string(rule.ModName)
            + ":" + rule.Name;
    }

    UObject* DragonWildsBlueprintModLoader::BuildRuntimeUiNode(
        UObject* owner,
        UObject* widgetTree,
        const nlohmann::json& node,
        const RuntimeUiRule& rule,
        size_t depth,
        size_t& budget,
        std::unordered_set<std::string>& names)
    {
        if (!node.is_object())
            throw std::runtime_error("Blueprint $RuntimeUI nodes must be objects");
        if (depth > 8)
            throw std::runtime_error("Blueprint $RuntimeUI tree exceeds the depth safety limit");
        if (++budget > 64)
            throw std::runtime_error("Blueprint $RuntimeUI tree exceeds the 64-node safety limit");

        const auto type = node.value("Type", std::string{});
        const auto classPath = RuntimeUiPrimitivePath(type);
        if (!classPath)
            throw std::runtime_error("Blueprint $RuntimeUI Type must be CanvasPanel, Border, TextBlock, Image, or Button");

        auto name = node.value("Name", std::string{});
        if (name.empty())
            name = "RuneSchemaNode_" + std::to_string(budget);
        if (!RuntimeUiNameValid(name))
            throw std::runtime_error("Blueprint $RuntimeUI node names must use only letters, numbers, and underscores");
        if (!names.emplace(name).second)
            throw std::runtime_error("Blueprint $RuntimeUI node names must be unique within the tree");

        auto* widgetClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, classPath, false);
        if (!widgetClass)
            throw std::runtime_error("Blueprint $RuntimeUI primitive class was unavailable");

        FStaticConstructObjectParameters params(widgetClass, widgetTree);
        params.Name = FName(to_generic_string(name), FNAME_Add);
        params.SetFlags = static_cast<EObjectFlags>(RF_Transactional);
        auto* widget = UObjectGlobals::StaticConstructObject<UObject*>(params);
        if (!widget)
            throw std::runtime_error("Blueprint $RuntimeUI failed to construct a widget node");

        m_runtimeWidgetObservedTargets[widget] = {
            PS::WeakObject(widget),
            PS::WeakObject(owner)
        };

        auto properties = node;
        properties.erase("Type");
        properties.erase("Name");
        properties.erase("Children");
        properties.erase("Slot");
        properties.erase("$Bind");
        properties.erase("$Call");
        properties.erase("$When");
        properties.erase("$Once");
        properties.erase("$Activate");
        properties.erase("$Find");
        if (!properties.empty())
            ApplyData(properties, widget, false);

        if (const auto children = node.find("Children"); children != node.end())
        {
            if (!children->is_array())
                throw std::runtime_error("Blueprint $RuntimeUI Children must be an array");
            if (children->size() > 32)
                throw std::runtime_error("Blueprint $RuntimeUI node exceeds the 32-child safety limit");

            std::string loweredType = type;
            std::transform(loweredType.begin(), loweredType.end(), loweredType.begin(),
                [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
            if ((loweredType == "textblock" || loweredType == "text" || loweredType == "image")
                && !children->empty())
                throw std::runtime_error("Blueprint $RuntimeUI leaf widgets cannot contain Children");
            if ((loweredType == "border" || loweredType == "button")
                && children->size() > 1)
                throw std::runtime_error("Blueprint $RuntimeUI single-content widgets accept at most one child");

            for (const auto& childData : *children)
            {
                auto* child = BuildRuntimeUiNode(
                    owner, widgetTree, childData, rule, depth + 1, budget, names);

                const auto addChildName = RC::StringType(TEXT("AddChild"));
                auto* addChildFunction = widget->GetFunctionByNameInChain(
                    addChildName.c_str());
                if (!addChildFunction)
                    throw std::runtime_error("Blueprint $RuntimeUI parent does not support child widgets");

                ActorHelper::FunctionCall addChild(widget, addChildFunction);
                addChild.Arg(TEXT("Content"), child).Invoke();
                auto* slot = addChild.Result<UObject*>();

                if (const auto slotData = childData.find("Slot");
                    slotData != childData.end())
                {
                    if (!slotData->is_object())
                        throw std::runtime_error("Blueprint $RuntimeUI Slot must be an object");
                    if (!slot)
                        throw std::runtime_error("Blueprint $RuntimeUI child slot was unavailable");
                    ApplyData(*slotData, slot, false);
                }
            }
        }

        RuntimeWidgetRule actionRule{
            rule.OwnerClass,
            to_generic_string(name),
            node,
            rule.ModName
        };
        ApplyRuntimeWidgetBinding(owner, widget, actionRule);
        ApplyRuntimeWidgetCalls(owner, widget, actionRule);
        return widget;
    }

    void DragonWildsBlueprintModLoader::ApplyRuntimeUiRule(
        UObject* owner,
        const RuntimeUiRule& rule,
        UFunction* function)
    {
        if (m_runtimeUiTearingDown
            || !RuntimeUiRuleMatchesEvent(rule, function))
            return;

        const auto key = RuntimeUiRuleKey(rule);
        if (const auto existing = m_runtimeUiInstances.find(key);
            existing != m_runtimeUiInstances.end())
        {
            auto* existingOwner = existing->second.Owner.Get();
            auto* existingWidget = existing->second.Widget.Get();
            if (existingOwner && existingWidget)
                return;

            if (existingWidget)
            {
                try
                {
                    const auto removeName = RC::StringType(TEXT("RemoveFromParent"));
                    auto* remove = existingWidget->GetFunctionByNameInChain(removeName.c_str());
                    if (remove && remove->GetParmsSize() == 0)
                        ActorHelper::FunctionCall(existingWidget, remove).Invoke();
                }
                catch (...) {}
            }
            m_runtimeUiInstances.erase(existing);
        }

        if (!m_runtimeUiActiveRules.emplace(key).second)
            return;

        try
        {
            if (!owner)
            {
                m_runtimeUiActiveRules.erase(key);
                return;
            }

            auto* gameInstance = SpawnRuntime::CallWorldContextGetter(
                TEXT("/Script/Engine.GameplayStatics:GetGameInstance"),
                TEXT("/Script/Engine.Default__GameplayStatics"),
                owner);
            if (!RuntimeObjectUsable(gameInstance))
            {
                m_runtimeUiActiveRules.erase(key);
                return;
            }

            const auto zOrder = rule.Data.value("ZOrder", 95);
            if (!rule.Data.at("Root").is_object()
                || zOrder < -1000 || zOrder > 10000)
                throw std::runtime_error("Blueprint $RuntimeUI ZOrder is outside the supported range");

            auto* userWidgetClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                nullptr, nullptr, TEXT("/Script/UMG.UserWidget"), false);
            auto* widgetTreeClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
                nullptr, nullptr, TEXT("/Script/UMG.WidgetTree"), false);
            if (!userWidgetClass || !widgetTreeClass)
                throw std::runtime_error("Blueprint $RuntimeUI required UMG classes were unavailable");

            const auto generation = ++m_runtimeUiGeneration;
            const auto runtimeName = "RuneSchemaUI_" + rule.Name + "_"
                + std::to_string(generation);
            FStaticConstructObjectParameters widgetParams(userWidgetClass, gameInstance);
            widgetParams.Name = FName(to_generic_string(runtimeName), FNAME_Add);
            widgetParams.SetFlags = static_cast<EObjectFlags>(RF_Transactional);
            auto* userWidget = UObjectGlobals::StaticConstructObject<UObject*>(widgetParams);
            if (!userWidget)
                throw std::runtime_error("Blueprint $RuntimeUI failed to construct its UserWidget");

            FStaticConstructObjectParameters treeParams(widgetTreeClass, userWidget);
            treeParams.Name = FName(to_generic_string(runtimeName + "_Tree"), FNAME_Add);
            treeParams.SetFlags = static_cast<EObjectFlags>(RF_Transactional);
            auto* widgetTree = UObjectGlobals::StaticConstructObject<UObject*>(treeParams);
            if (!widgetTree)
                throw std::runtime_error("Blueprint $RuntimeUI failed to construct its WidgetTree");

            ActorHelper::SetObjectRef(userWidget, TEXT("WidgetTree"), widgetTree);

            if (const auto properties = rule.Data.find("Properties");
                properties != rule.Data.end())
            {
                if (!properties->is_object())
                    throw std::runtime_error("Blueprint $RuntimeUI Properties must be an object");
                ApplyData(*properties, userWidget, false);
            }

            size_t budget = 0;
            std::unordered_set<std::string> names;
            auto* root = BuildRuntimeUiNode(
                owner, widgetTree, rule.Data.at("Root"), rule, 0, budget, names);
            ActorHelper::SetObjectRef(widgetTree, TEXT("RootWidget"), root);

            m_runtimeWidgetObservedTargets[userWidget] = {
                PS::WeakObject(userWidget),
                PS::WeakObject(owner)
            };

            const auto addToViewportName = RC::StringType(TEXT("AddToViewport"));
            auto* addToViewport = userWidget->GetFunctionByNameInChain(
                addToViewportName.c_str());
            if (!addToViewport)
                throw std::runtime_error("Blueprint $RuntimeUI AddToViewport was unavailable");
            ActorHelper::FunctionCall(userWidget, addToViewport)
                .Arg(TEXT("ZOrder"), static_cast<int32_t>(zOrder)).Invoke();

            m_runtimeUiInstances.insert_or_assign(key, RuntimeUiInstance{
                PS::WeakObject(owner),
                PS::WeakObject(userWidget)
            });
        }
        catch (...)
        {
            m_runtimeUiActiveRules.erase(key);
            throw;
        }

        m_runtimeUiActiveRules.erase(key);
    }

    void DragonWildsBlueprintModLoader::ApplyRuntimeWidgetRule(
        UObject* owner,
        const RuntimeWidgetRule& rule)
    {
        const auto ruleKey = RuntimeWidgetRuleKey(owner, rule);

        bool runOnce = false;
        if (const auto once = rule.Data.find("$Once"); once != rule.Data.end())
        {
            if (!once->is_boolean())
                throw std::runtime_error("Blueprint $RuntimeWidget $Once must be true or false");
            runOnce = once->get<bool>();
            if (runOnce && m_runtimeWidgetCompletedRules.contains(ruleKey))
                return;
        }

        // $Call and $Activate themselves dispatch ProcessEvent. Never let the
        // post-observer recursively execute the same rule while it is active.
        if (!m_runtimeWidgetActiveRules.emplace(ruleKey).second)
            return;

        try
        {
            auto* target = ResolveRuntimeWidgetTarget(owner, rule);
            if (!target)
            {
                m_runtimeWidgetActiveRules.erase(ruleKey);
                return;
            }

            ApplyRuntimeWidgetBinding(owner, target, rule);

            auto properties = rule.Data;
            properties.erase("$Bind");
            properties.erase("$Call");
            properties.erase("$When");
            properties.erase("$Once");
            properties.erase("$Activate");
            properties.erase("$Find");
            if (!properties.empty())
                ApplyData(properties, target, false);

            ApplyRuntimeWidgetActivation(target, rule);
            ApplyRuntimeWidgetCalls(owner, target, rule);

            if (runOnce)
                m_runtimeWidgetCompletedRules.emplace(ruleKey);
        }
        catch (...)
        {
            m_runtimeWidgetActiveRules.erase(ruleKey);
            throw;
        }

        m_runtimeWidgetActiveRules.erase(ruleKey);
    }

    void DragonWildsBlueprintModLoader::ObserveRuntimeWidgetEvent(
        UObject* source,
        UFunction* function)
    {
        if (!source || (m_runtimeWidgetRules.empty() && m_runtimeUiRules.empty())) return;

        std::unordered_set<UObject*> ownersToRefresh;

        if (const auto observed = m_runtimeWidgetObservedTargets.find(source);
            observed != m_runtimeWidgetObservedTargets.end())
        {
            auto* trackedTarget = observed->second.Target.Get();
            auto* trackedOwner = observed->second.Owner.Get();
            if (trackedTarget == source && trackedOwner)
                ownersToRefresh.emplace(trackedOwner);
            else
                m_runtimeWidgetObservedTargets.erase(observed);
        }

        for (const auto& triggerRule : m_runtimeWidgetRules)
        {
            auto* owner = FindRuntimeWidgetOwner(source, triggerRule.OwnerClass);
            if (!owner) continue;
            if (source == owner
                || RuntimeWidgetPathContains(owner, source, triggerRule.WidgetPath))
                ownersToRefresh.emplace(owner);
        }
        for (const auto& uiRule : m_runtimeUiRules)
        {
            if (auto* owner = FindRuntimeWidgetOwner(source, uiRule.OwnerClass))
                ownersToRefresh.emplace(owner);
        }

        for (auto* owner : ownersToRefresh)
        {
            auto* type = owner ? owner->GetClassPrivate() : nullptr;
            if (!type) continue;
            const auto ownerName = type->GetNamePrivate();
            const auto ownerPath = FName(type->GetPathName(), FNAME_Find);

            for (const auto& rule : m_runtimeWidgetRules)
            {
                if (rule.OwnerClass != ownerName && rule.OwnerClass != ownerPath)
                    continue;
                try {
                    if (!RuntimeWidgetRuleMatchesEvent(rule, function))
                        continue;
                    ApplyRuntimeWidgetRule(owner, rule);
                } catch (const std::exception& error) {
                    const auto failureKey = RC::to_string(rule.ModName)
                        + ":" + RC::to_string(rule.WidgetPath)
                        + ":" + error.what();
                    if (m_reportedRuntimeWidgetFailures.emplace(failureKey).second)
                    {
                        PS::Log<LogLevel::Warning>(
                            STR("Blueprint $RuntimeWidget '{}' from '{}' failed: {}. Further identical failures are suppressed.\n"),
                            rule.WidgetPath,
                            rule.ModName,
                            PS::ToWideSafe(error.what()));
                    }
                }
            }

            for (const auto& rule : m_runtimeUiRules)
            {
                if (rule.OwnerClass != ownerName && rule.OwnerClass != ownerPath)
                    continue;
                try
                {
                    ApplyRuntimeUiRule(owner, rule, function);
                }
                catch (const std::exception& error)
                {
                    const auto failureKey = RC::to_string(rule.ModName)
                        + ":" + rule.Name + ":" + error.what();
                    if (m_reportedRuntimeUiFailures.emplace(failureKey).second)
                    {
                        PS::Log<LogLevel::Warning>(
                            STR("Blueprint $RuntimeUI '{}' from '{}' failed: {}. Further identical failures are suppressed.\n"),
                            PS::ToWideSafe(rule.Name.c_str()),
                            rule.ModName,
                            PS::ToWideSafe(error.what()));
                    }
                }
            }
        }
    }

    void DragonWildsBlueprintModLoader::LoadUnsafe(const nlohmann::json& data)
    {
        if (data.is_array()) { for (const auto& entry : data) LoadUnsafe(entry); return; }
        if (data.contains("$Patch") || data.contains("$Target")) return;
        constexpr size_t detailLimit = 8;
        size_t applied = 0;
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

                if (applied < detailLimit)
                    PS::Log<RC::LogLevel::Verbose>(TEXT("Applied Blueprint changes to {}.\n"),
                        static_cast<UClass*>(asset)->GetNamePrivate().ToString());
                ++applied;
            }
        }
        if (applied > detailLimit)
            PS::Log<LogLevel::Verbose>(STR("Blueprints: {} additional successful change detail(s) omitted.\n"),
                applied - detailLimit);
        if (applied)
            PS::LoaderSummary("blueprints", applied, 0, applied, 0, 0);
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
            if (propertyName == "$Append" || propertyName == "$VisualEffect"
                || propertyName == "$RuntimeWidget" || propertyName == "$RuntimeUI")
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
        const FName componentFullFName(componentFullName,FNAME_Add);
        UObject* inheritableComponent = nullptr;

        auto inheritableComponentHandler = bpClass->GetInheritableComponentHandler();
        if (inheritableComponentHandler)
        {
            auto records = inheritableComponentHandler->GetRecords();
            for (auto& record : records)
            {
                if (record.ComponentTemplate.Get() == nullptr) continue;

                if (record.ComponentTemplate.Get()->GetFName() == componentFullFName)
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
        const FName componentFName(componentName,FNAME_Add);

        auto& nodes = simpleConstructionScript->GetAllNodes();
        for (auto& nodeElement : nodes)
        {
            auto nodeComponentTemplate = nodeElement->GetComponentTemplate();
            if (!nodeComponentTemplate)
            {
                continue;
            }

            if (nodeComponentTemplate->GetFName() == componentFName)
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
