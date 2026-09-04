#pragma once

#include <functional>
#include <vector>
#include "Unreal/AActor.hpp"
#include "Unreal/Rotator.hpp"
#include "Unreal/UnrealCoreStructs.hpp"

namespace RC::Unreal {
    class UClass;
    class UFunction;
    class UObject;
    class UWorld;
}

namespace DragonWilds::ActorHelper {
    // Converts export-index paths copied from tools such as FModel
    // (/Game/Path/Asset.0) to Unreal's canonical object path
    // (/Game/Path/Asset.Asset). Canonical paths pass through unchanged.
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
                                       RC::Unreal::ESpawnActorScaleMethod::MultiplyWithRoot,
                                   RC::Unreal::UObject* WorldContext = nullptr,
                                   RC::Unreal::AActor* Owner = nullptr,
                                   bool UseAdjustedCollision = false);

    void DestroyActor(RC::Unreal::AActor* Actor);

    RC::Unreal::FVector GetActorLocation(RC::Unreal::AActor* Actor);

    RC::Unreal::FRotator GetActorRotation(RC::Unreal::AActor* Actor);

    RC::Unreal::UObject* ConstructTransientObject(RC::Unreal::UClass* ObjectClass, const RC::StringType& Name);

    RC::Unreal::UObject* GetObjectRef(RC::Unreal::UObject* Container, const RC::StringType& Name);

    void SetObjectRef(RC::Unreal::UObject* Container, const RC::StringType& Name, RC::Unreal::UObject* Value);

    void SetSoftObjectRef(RC::Unreal::UObject* Container, const RC::StringType& Name, RC::Unreal::UObject* Value);

    void AddToSoftObjectSet(RC::Unreal::UObject* Container, const RC::StringType& Name, RC::Unreal::UObject* Value);

    class FunctionCall {
    public:
        FunctionCall(RC::Unreal::UObject* Self, const RC::StringType& FunctionPath);
        FunctionCall(RC::Unreal::UObject* Self, RC::Unreal::UFunction* Function);

        template <typename T>
        FunctionCall& Arg(const RC::CharType* Name, const T& Value)
        {
            Write(Name, &Value, sizeof(T));
            return *this;
        }

        FunctionCall& SoftObjectArg(const RC::CharType* Name, RC::Unreal::UObject* Value);

        // Imports text through the live FTextProperty metadata instead of copying
        // RuneSchema's compiled FText layout across the reflected function boundary.
        FunctionCall& TextArg(const RC::CharType* Name, const RC::StringType& Value);

        FunctionCall& FirstNumericArg(double Value);

        void Invoke();

        void DestroyArg(const RC::CharType* Name);

        void ForEachObjectSetArg(
            const RC::CharType* Name,
            const std::function<void(RC::Unreal::UObject*)>& Callback);

        template <typename T>
        T Result()
        {
            T value{};
            ReadReturn(&value, sizeof(T));
            return value;
        }

        void MoveResult(void* Out, size_t Size);

        double NumericResult();

        RC::StringType EnumResultName();

        int64_t EnumResultValue();

    private:
        void Write(const RC::CharType* Name, const void* Data, size_t Size);
        void ReadReturn(void* Out, size_t Size);

        RC::Unreal::UObject* m_self = nullptr;
        RC::Unreal::UFunction* m_function = nullptr;
        std::vector<uint8_t> m_params;
    };
}
