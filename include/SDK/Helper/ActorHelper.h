#pragma once

#include <functional>
#include <vector>
#include "nlohmann/json_fwd.hpp"
#include "Unreal/AActor.hpp"
#include "Unreal/Rotator.hpp"
#include "Unreal/UnrealCoreStructs.hpp"

namespace RC::Unreal {
    class UClass;
    class FProperty;
    class UFunction;
    class UObject;
    class UWorld;
}

namespace DragonWilds::ActorHelper {
    RC::StringType NormalizeObjectPath(const RC::StringType& Path);

    RC::Unreal::UObject* ResolveObject(const RC::StringType& Path);

    RC::Unreal::UClass* ResolveClass(const RC::StringType& Path);

    bool IsAbstract(RC::Unreal::UClass* Class);

    bool IsActorClass(RC::Unreal::UClass* Class);

    RC::Unreal::AActor* SpawnActor(RC::Unreal::UWorld* World,
                                   RC::Unreal::UClass* ActorClass,
                                   const RC::Unreal::FVector& Location,
                                   const RC::Unreal::FRotator& Rotation,
                                   const std::function<void(RC::Unreal::AActor*)>& Configure = {},
                                   RC::Unreal::ESpawnActorScaleMethod ScaleMethod =
                                       RC::Unreal::ESpawnActorScaleMethod::MultiplyWithRoot);

    void DestroyActor(RC::Unreal::AActor* Actor);
    void DestroyComponent(RC::Unreal::UObject* Component);

    RC::Unreal::FVector GetActorLocation(RC::Unreal::AActor* Actor);

    RC::Unreal::UObject* ConstructTransientObject(RC::Unreal::UClass* ObjectClass, const RC::StringType& Name);

    RC::Unreal::UObject* GetObjectRef(RC::Unreal::UObject* Container, const RC::StringType& Name);

    void SetObjectRef(RC::Unreal::UObject* Container, const RC::StringType& Name, RC::Unreal::UObject* Value);

    void SetSoftObjectRef(RC::Unreal::UObject* Container, const RC::StringType& Name, RC::Unreal::UObject* Value);

    void AddToSoftObjectSet(RC::Unreal::UObject* Container, const RC::StringType& Name, RC::Unreal::UObject* Value);

    class FunctionCall {
    public:
        FunctionCall(RC::Unreal::UObject* Self, const RC::StringType& FunctionPath);
        FunctionCall(RC::Unreal::UObject* Self, RC::Unreal::UFunction* Function);
        ~FunctionCall();
        FunctionCall(const FunctionCall&) = delete;
        FunctionCall& operator=(const FunctionCall&) = delete;

        template <typename T>
        FunctionCall& Arg(const RC::CharType* Name, const T& Value)
        {
            Write(Name, &Value, sizeof(T));
            return *this;
        }

        FunctionCall& SoftObjectArg(const RC::CharType* Name, RC::Unreal::UObject* Value);
        FunctionCall& JsonArg(const RC::CharType* Name, const nlohmann::json& Value);

        // Reflected containers cannot be passed with the scalar Arg helper. A
        // byte copy aliases the source allocation and lets ProcessEvent or its
        // parameter cleanup corrupt live game state. This method owns a deep
        // reflected copy for the duration of the call.
        FunctionCall& ArrayArg(const RC::CharType* Name,
                               RC::Unreal::FProperty* SourceProperty,
                               const void* SourceValue);

        FunctionCall& FirstNumericArg(double Value);

        void Invoke();

        template <typename T>
        T Result()
        {
            T value{};
            ReadReturn(&value, sizeof(T));
            return value;
        }

        void MoveResult(void* Out, size_t Size);

        double NumericResult();

    private:
        void Write(const RC::CharType* Name, const void* Data, size_t Size);
        void ReadReturn(void* Out, size_t Size);

        RC::Unreal::UObject* m_self = nullptr;
        RC::Unreal::UFunction* m_function = nullptr;
        std::vector<uint8_t> m_params;
        std::vector<RC::Unreal::FProperty*> m_initialized;
    };
}
