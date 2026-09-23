#include <cstring>
#include <cmath>
#include <format>
#include <stdexcept>
#include "Unreal/AActor.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/Property/FEnumProperty.hpp"
#include "Unreal/Transform.hpp"
#include "Unreal/UFunctionStructs.hpp"
#include "Unreal/UObject.hpp"
#include "Unreal/UObjectGlobals.hpp"
#include "Unreal/UnrealFlags.hpp"
#include "Unreal/World.hpp"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Classes/TSoftObjectPtr.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Structs/Custom/FScriptSetHelper.h"
#include "SDK/Structs/FSoftObjectPath.h"
#include "Utility/Logging.h"
#include "Utility/AssetAliases.h"

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds::ActorHelper {
    namespace {
        UObject* GetDefaultObject(const TCHAR* Path)
        {
            auto* defaultObject = UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, Path, false);
            if (!defaultObject)
            {
                throw std::runtime_error(std::format("Default object '{}' was unavailable", RC::to_string(StringType(Path))));
            }
            return defaultObject;
        }

        FProperty* RequireProperty(UObject* Container, const StringType& Name)
        {
            if (!Container)
            {
                throw std::runtime_error("Tried to access a property on a null object");
            }

            auto* property = PropertyHelper::GetPropertyByName(Container->GetClassPrivate(), Name);
            if (!property)
            {
                throw std::runtime_error(std::format("Property '{}' does not exist on {}",
                    RC::to_string(Name), RC::to_string(Container->GetClassPrivate()->GetName())));
            }
            return property;
        }

        void MakeSoftObjectRef(UObject* Value, void* Out, size_t Size)
        {
            std::memset(Out, 0, Size);
            if (!Value)
            {
                return;
            }

            auto call = FunctionCall(GetDefaultObject(TEXT("/Script/Engine.Default__KismetSystemLibrary")),
                                     STR("/Script/Engine.KismetSystemLibrary:Conv_ObjectToSoftObjectReference"));
            call.Arg(STR("Object"), Value);
            call.Invoke();
            call.MoveResult(Out, Size);
        }
    }

    StringType NormalizeObjectPath(const StringType& Path)
    {
        if (Path.empty()) return Path;

        StringType normalized = Path;
        const auto slash = normalized.find_last_of(STR("/\\"));
        const auto dot = normalized.find_last_of(STR("."));
        if (slash == StringType::npos || dot == StringType::npos || dot <= slash + 1
            || dot + 1 >= normalized.size())
        {
            return normalized;
        }

        for (size_t index = dot + 1; index < normalized.size(); ++index)
        {
            if (normalized[index] < static_cast<CharType>('0')
                || normalized[index] > static_cast<CharType>('9'))
            {
                return normalized;
            }
        }

        const auto assetName = normalized.substr(slash + 1, dot - slash - 1);
        normalized.replace(dot + 1, StringType::npos, assetName);
        return normalized;
    }

    UObject* ResolveObject(const StringType& Path)
    {
        const auto normalizedPath = PS::AssetAliases::Resolve(NormalizeObjectPath(Path));
        if (auto* found = UECustom::UObjectGlobals::StaticFindObject<UObject*>(
                nullptr, nullptr, normalizedPath.c_str(), false))
        {
            return found;
        }

        auto softObject = UECustom::TSoftObjectPtr<UObject>(
            UECustom::FSoftObjectPath(normalizedPath));
        return UECustom::UKismetSystemLibrary::LoadAsset_Blocking(softObject);
    }

    UClass* ResolveClass(const StringType& Path)
    {
        auto* found = ResolveObject(Path);
        return found && found->IsA<UClass>() ? static_cast<UClass*>(found) : nullptr;
    }

    bool IsAbstract(UClass* Class)
    {
        return Class && Class->HasAnyClassFlags(CLASS_Abstract);
    }

    bool IsActorClass(UClass* Class)
    {
        static auto* actorClass = UECustom::UObjectGlobals::StaticFindObject<UClass*>(
            nullptr, nullptr, TEXT("/Script/Engine.Actor"), false);

        return Class && actorClass && Class->IsChildOf(actorClass);
    }

    AActor* SpawnActor(UWorld* World,
                       UClass* ActorClass,
                       const FVector& Location,
                       const FRotator& Rotation,
                       const std::function<void(AActor*)>& Configure,
                       ESpawnActorScaleMethod ScaleMethod)
    {
        if (!World || !ActorClass)
        {
            throw std::runtime_error("Spawn was given a null world or class");
        }

        auto transform = FTransform(Rotation, Location, FVector(1.0, 1.0, 1.0));

        UObject* worldContext = World;
        AActor* owner = nullptr;
        auto collision = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto begin = FunctionCall(GetDefaultObject(TEXT("/Script/Engine.Default__GameplayStatics")),
                                  STR("/Script/Engine.GameplayStatics:BeginDeferredActorSpawnFromClass"));
        begin.Arg(STR("WorldContextObject"), worldContext)
             .Arg(STR("ActorClass"), ActorClass)
             .Arg(STR("SpawnTransform"), transform)
             .Arg(STR("CollisionHandlingOverride"), collision)
             .Arg(STR("Owner"), owner)
             .Arg(STR("TransformScaleMethod"), ScaleMethod);
        begin.Invoke();

        auto* actor = begin.Result<AActor*>();
        if (!actor)
        {
            throw std::runtime_error(std::format("BeginDeferredActorSpawnFromClass returned null for '{}'",
                RC::to_string(ActorClass->GetName())));
        }

        try {
        if (Configure)
        {
            Configure(actor);
        }

        auto finish = FunctionCall(GetDefaultObject(TEXT("/Script/Engine.Default__GameplayStatics")),
                                   STR("/Script/Engine.GameplayStatics:FinishSpawningActor"));
        finish.Arg(STR("Actor"), actor)
              .Arg(STR("SpawnTransform"), transform)
              .Arg(STR("TransformScaleMethod"), ScaleMethod);
        finish.Invoke();

        auto* spawned = finish.Result<AActor*>();
        if (!spawned)
        {
            throw std::runtime_error("FinishSpawningActor returned null");
        }

        return spawned;
        } catch (...) {
            try { DestroyActor(actor); } catch (...) {}
            throw;
        }
    }

    void DestroyActor(AActor* Actor)
    {
        if (!Actor)
        {
            return;
        }

        FunctionCall(Actor, STR("/Script/Engine.Actor:K2_DestroyActor")).Invoke();
    }

    void DestroyComponent(UObject* Component)
    {
        if(!Component)return;
        auto* type=ResolveClass(STR("/Script/Engine.ActorComponent"));
        if(!type || !Component->IsA(type))throw std::runtime_error("Cleanup target is not an ActorComponent");
        const auto validate=[](const TCHAR* path,const TCHAR* name,bool returns) {
            auto* fn=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path,false);
            auto* field=fn?CastField<FObjectProperty>(fn->FindProperty(FName(name,FNAME_Find))):nullptr;
            if(!fn || !field || fn->GetParmsSize()!=sizeof(UObject*) || field->GetElementSize()!=sizeof(UObject*)
                || field->GetArrayDim()!=1 || field->GetOffset_Internal()!=0 || !field->HasAnyPropertyFlags(CPF_Parm)
                || (returns ? fn->GetReturnProperty()!=field : fn->GetReturnProperty()!=nullptr))
                throw std::runtime_error("Component cleanup object parameter contract changed");
            size_t count=0;
            for(auto* p:TFieldRange<FProperty>(fn,EFieldIterationFlags::Default))if(p->HasAnyPropertyFlags(CPF_Parm))++count;
            if(count!=1)throw std::runtime_error("Component cleanup parameter count changed");
        };
        validate(TEXT("/Script/Engine.ActorComponent:GetOwner"),TEXT("ReturnValue"),true);
        FunctionCall ownerCall(Component,STR("/Script/Engine.ActorComponent:GetOwner"));
        ownerCall.Invoke();
        auto* owner=ownerCall.Result<UObject*>();
        if(!owner || !IsActorClass(owner->GetClassPrivate()) || owner->GetWorld()!=Component->GetWorld())
            throw std::runtime_error("Component cleanup owner is unavailable");
        validate(TEXT("/Script/Engine.ActorComponent:K2_DestroyComponent"),TEXT("Object"),false);
        FunctionCall(Component,STR("/Script/Engine.ActorComponent:K2_DestroyComponent")).Arg(STR("Object"),owner).Invoke();
    }

    FVector GetActorLocation(AActor* Actor)
    {
        auto call = FunctionCall(Actor, STR("/Script/Engine.Actor:K2_GetActorLocation"));
        call.Invoke();
        return call.Result<FVector>();
    }

    UObject* ConstructTransientObject(UClass* ObjectClass, const StringType& Name)
    {
        static auto* transientPackage = UECustom::UObjectGlobals::StaticFindObject(
            nullptr, nullptr, TEXT("/Engine/Transient"), false);

        if (!ObjectClass || !transientPackage)
        {
            throw std::runtime_error("Object construction was missing its class or the transient package");
        }

        FStaticConstructObjectParameters params(ObjectClass, transientPackage);
        params.Name = FName(Name, FNAME_Add);
        params.SetFlags = static_cast<EObjectFlags>(RF_Public | RF_Standalone | RF_Transactional);

        auto* object = UObjectGlobals::StaticConstructObject<UObject*>(params);
        if (!object)
        {
            throw std::runtime_error(std::format("Failed to construct '{}'", RC::to_string(Name)));
        }

        return object;
    }

    UObject* GetObjectRef(UObject* Container, const StringType& Name)
    {
        auto* property = RequireProperty(Container, Name);
        return *property->ContainerPtrToValuePtr<UObject*>(Container);
    }

    void SetObjectRef(UObject* Container, const StringType& Name, UObject* Value)
    {
        auto* property = RequireProperty(Container, Name);
        *property->ContainerPtrToValuePtr<UObject*>(Container) = Value;
    }

    void SetSoftObjectRef(UObject* Container, const StringType& Name, UObject* Value)
    {
        auto* property = RequireProperty(Container, Name);
        MakeSoftObjectRef(Value, property->ContainerPtrToValuePtr<void>(Container),
            static_cast<size_t>(property->GetElementSize()));
    }

    void AddToSoftObjectSet(UObject* Container, const StringType& Name, UObject* Value)
    {
        if (!Value)
        {
            return;
        }

        auto* setProperty = CastField<FSetProperty>(RequireProperty(Container, Name));
        if (!setProperty)
        {
            throw std::runtime_error(std::format("'{}' is not a set property", RC::to_string(Name)));
        }

        const auto size = static_cast<size_t>(setProperty->GetElementProp()->GetElementSize());
        if (size != sizeof(UECustom::TSoftObjectPtr<UObject>))
        {
            throw std::runtime_error(std::format("'{}' does not hold soft object references", RC::to_string(Name)));
        }

        UECustom::TSoftObjectPtr<UObject> element;
        MakeSoftObjectRef(Value, &element, size);

        UECustom::FScriptSetHelper helper(setProperty, setProperty->ContainerPtrToValuePtr<void>(Container));
        helper.Add(&element);
    }

    FunctionCall::FunctionCall(UObject* Self, const StringType& FunctionPath)
        : m_self(Self)
    {
        if (!m_self)
        {
            throw std::runtime_error("Tried to call a function on a null object");
        }

        m_function = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, FunctionPath.c_str(), false);
        if (!m_function)
        {
            throw std::runtime_error(std::format("Function '{}' was unavailable", RC::to_string(FunctionPath)));
        }

        m_params.assign(m_function->GetParmsSize(), 0);
    }

    FunctionCall::FunctionCall(UObject* Self, UFunction* Function)
        : m_self(Self), m_function(Function)
    {
        if (!m_self || !m_function)
            throw std::runtime_error("Tried to call a null object or function");
        const auto size = static_cast<size_t>(m_function->GetParmsSize());
        if (size > 65535)
            throw std::runtime_error("Function parameter buffer is outside the supported range");
        m_params.assign(size, 0);
    }

    FunctionCall::~FunctionCall()
    {
        for (auto it = m_initialized.rbegin(); it != m_initialized.rend(); ++it)
        {
            try { (*it)->DestroyValue_InContainer(m_params.data()); }
            catch (...) {}
        }
    }

    void FunctionCall::Write(const CharType* Name, const void* Data, size_t Size)
    {
        auto* property = m_function->FindProperty(FName(Name, FNAME_Find));
        if (!property || property->GetOffset_Internal() < 0
            || static_cast<size_t>(property->GetOffset_Internal()) + Size > m_params.size())
        {
            throw std::runtime_error(std::format("Parameter '{}' did not match the live function layout",
                RC::to_string(StringType(Name))));
        }

        std::memcpy(m_params.data() + property->GetOffset_Internal(), Data, Size);
    }

    FunctionCall& FunctionCall::SoftObjectArg(const CharType* Name, UObject* Value)
    {
        auto* property = m_function->FindProperty(FName(Name, FNAME_Find));
        if (!property)
        {
            throw std::runtime_error(std::format("Parameter '{}' does not exist", RC::to_string(StringType(Name))));
        }

        const auto size = static_cast<size_t>(property->GetElementSize());
        std::vector<uint8_t> soft(size, 0);
        MakeSoftObjectRef(Value, soft.data(), size);

        Write(Name, soft.data(), size);
        return *this;
    }

    FunctionCall& FunctionCall::JsonArg(const CharType* Name, const nlohmann::json& Value)
    {
        auto* property = m_function->FindProperty(FName(Name, FNAME_Find));
        if (!property || property == m_function->GetReturnProperty()
            || property->GetOffset_Internal() < 0
            || static_cast<size_t>(property->GetOffset_Internal())
                + static_cast<size_t>(property->GetElementSize()) > m_params.size())
        {
            throw std::runtime_error(std::format("Parameter '{}' did not match the live function layout",
                RC::to_string(StringType(Name))));
        }

        property->InitializeValue_InContainer(m_params.data());
        m_initialized.push_back(property);
        PropertyHelper::CopyJsonValueToContainer(m_params.data(), property, Value);
        return *this;
    }

    FunctionCall& FunctionCall::ArrayArg(const CharType* Name, FProperty* SourceProperty,
                                         const void* SourceValue)
    {
        auto* destination = CastField<FArrayProperty>(
            m_function->FindProperty(FName(Name, FNAME_Find)));
        auto* source = CastField<FArrayProperty>(SourceProperty);
        if (!destination || !source || !SourceValue
            || destination->GetOffset_Internal() < 0
            || static_cast<size_t>(destination->GetOffset_Internal())
                + static_cast<size_t>(destination->GetElementSize()) > m_params.size()
            || destination->GetInner()->GetClass() != source->GetInner()->GetClass()
            || destination->GetInner()->GetElementSize() != source->GetInner()->GetElementSize())
        {
            throw std::runtime_error(std::format(
                "Array parameter '{}' did not match the live source layout",
                RC::to_string(StringType(Name))));
        }

        if (auto* destinationStruct = CastField<FStructProperty>(destination->GetInner()))
        {
            auto* sourceStruct = CastField<FStructProperty>(source->GetInner());
            if (!sourceStruct || destinationStruct->GetStruct() != sourceStruct->GetStruct())
                throw std::runtime_error("Array parameter struct type changed");
        }

        destination->InitializeValue_InContainer(m_params.data());
        m_initialized.push_back(destination);
        destination->CopyCompleteValue(
            destination->ContainerPtrToValuePtr<void>(m_params.data()), SourceValue);
        return *this;
    }

    FunctionCall& FunctionCall::FirstNumericArg(double Value)
    {
        for (auto* property = m_function->GetPropertyLink(); property;
             property = property->GetPropertyLinkNext())
        {
            if (auto* enumProperty = CastField<FEnumProperty>(property);
                enumProperty && property != m_function->GetReturnProperty())
            {
                auto* underlying = enumProperty->GetUnderlyingProperty();
                auto* address = enumProperty->ContainerPtrToValuePtr<void>(m_params.data());
                if (!underlying || !address) continue;
                underlying->SetIntPropertyValue(address, static_cast<int64>(std::llround(Value)));
                return *this;
            }

            auto* numeric = CastField<FNumericProperty>(property);
            if (!numeric || property == m_function->GetReturnProperty()) continue;
            auto* address = numeric->ContainerPtrToValuePtr<void>(m_params.data());
            if (numeric->IsFloatingPoint())
                numeric->SetFloatingPointPropertyValue(address, Value);
            else if (numeric->IsInteger())
                numeric->SetIntPropertyValue(address, static_cast<int64>(std::llround(Value)));
            else
                continue;
            return *this;
        }
        throw std::runtime_error("Function has no numeric or enum input parameter");
    }

    void FunctionCall::Invoke()
    {
        m_self->ProcessEvent(m_function, m_params.data());
    }

    void FunctionCall::ReadReturn(void* Out, size_t Size)
    {
        auto* returnProperty = m_function->GetReturnProperty();
        if (!returnProperty || returnProperty->GetOffset_Internal() < 0
            || static_cast<size_t>(returnProperty->GetOffset_Internal()) + Size > m_params.size())
        {
            throw std::runtime_error("Function return value did not match the live layout");
        }

        std::memcpy(Out, m_params.data() + returnProperty->GetOffset_Internal(), Size);
    }

    void FunctionCall::MoveResult(void* Out, size_t Size)
    {
        ReadReturn(Out, Size);

        auto* source = m_params.data() + m_function->GetReturnProperty()->GetOffset_Internal();
        std::memset(source, 0, Size);
    }

    double FunctionCall::NumericResult()
    {
        auto* property = CastField<FNumericProperty>(m_function->GetReturnProperty());
        if (!property) throw std::runtime_error("Function has no numeric return value");
        auto* address = property->ContainerPtrToValuePtr<void>(m_params.data());
        if (property->IsFloatingPoint()) return property->GetFloatingPointPropertyValue(address);
        if (property->IsInteger())
            return static_cast<double>(property->GetSignedIntPropertyValue(address));
        throw std::runtime_error("Function return value is not a supported numeric type");
    }
}
