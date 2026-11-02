#pragma once
#include "Runtime/BuildingPolicy/Reflection.h"
#include <Unreal/Property/FEnumProperty.hpp>
#include <Unreal/CoreUObject/UObject/FStrProperty.hpp>
#include <Unreal/Engine/UDataTable.hpp>
#include <charconv>
#include <cmath>
#include <memory>

namespace PS {
// Profiles own an Add modifier against the unmodified base. Native equipment
// and zone modifiers keep their own handles and ordering.
class PlayerAttributeEffect {
    Buildings::WeakObject component;
    RC::Unreal::UScriptStruct* type = nullptr;
    std::vector<unsigned char> handle;
    bool applied = false;
    bool owned = false;
    float delta = 0;

    static void Enum(RC::Unreal::UStruct* type, void* data,
        const wchar_t* name, const wchar_t* wanted) {
        using namespace RC::Unreal;
        auto* property = CastField<FEnumProperty>(Buildings::Field(type, name));
        if (!property || property->GetSize() != 1)
            throw std::runtime_error("Player modifier enum contract differs");
        for (const auto& [key, value] : property->GetEnum()->GetEnumNames()) {
            auto text = key.ToString();
            const auto separator = text.rfind(L"::");
            if (separator != text.npos) text = text.substr(separator + 2);
            if (text == wanted) {
                *property->ContainerPtrToValuePtr<uint8>(data) = uint8(value);
                return;
            }
        }
        throw std::runtime_error("Player modifier enum value is unavailable");
    }
    static void Number(RC::Unreal::UStruct* type, void* data,
        const wchar_t* name, double value) {
        auto* property = RC::Unreal::CastField<RC::Unreal::FFloatProperty>(Buildings::Field(type, name));
        if (!property || property->GetSize() != sizeof(float) || !std::isfinite(value))
            throw std::runtime_error("Player modifier float contract differs");
        property->SetPropertyValue(property->ContainerPtrToValuePtr<void>(data), float(value));
    }
    static std::shared_ptr<PlayerAttributeEffect> Create(RC::Unreal::UObject* pawn,
        RC::Unreal::UObject* attribute, float delta) {
        using namespace RC::Unreal;
        using namespace Buildings;
        const auto* path = Template(attribute);
        if (!path) throw std::runtime_error("Native player modifier template is unavailable");
        Call load(Find<UObject>(L"/Script/Engine.Default__KismetSystemLibrary"), L"LoadAsset_Blocking");
        load.Text(L"Asset", path); load.Invoke();
        auto* cls = Cast<UClass>(load.Result());
        if (!cls) throw std::runtime_error("Native player modifier template did not load");
        auto* original = cls->GetClassDefaultObject().Get();
        auto* target = Object(original, L"AttributeToModify");
        auto* children = CastField<FArrayProperty>(Field(cls, L"ChildGEsGroups"));
        if (!target || !target->IsA<UClass>() || !attribute->IsA(static_cast<UClass*>(target))
            || Object(original, L"InstanceClass") != Find<UClass>(L"/Script/Dominion.DominionGameplayEffect")
            || Object(original, L"ActivationCurrentValueAttribute")
            || Boolean(cls, original, L"bCannotBeRemoved")
            || !children || children->ContainerPtrToValuePtr<FScriptArray>(original)->Num() != 0)
            throw std::runtime_error("Native player modifier template has unexpected behavior");
        auto result = std::make_shared<PlayerAttributeEffect>();
        result->component = Object(pawn, L"GameplayEffectsComponent");
        result->delta = delta;
        Call create(result->component.Get(), L"InstantiateGameplayEffect");
        create.Object(L"DataClass", cls); create.Text(L"bForceNewInstance", L"True"); create.Invoke();
        auto* returned = CastField<FStructProperty>(create.Prop(L"ReturnValue"));
        if (!returned || returned->GetStruct()->GetPathName() != L"/Script/Dominion.DominionGameplayEffectHandle"
            || returned->GetSize() <= 0 || returned->GetSize() > 128)
            throw std::runtime_error("Native player modifier handle differs");
        result->handle.resize(returned->GetSize());
        result->type = returned->GetStruct();
        result->type->InitializeStruct(result->handle.data());
        returned->CopyCompleteValue(result->handle.data(), create.Data(L"ReturnValue"));
        result->owned = true;
        auto* identity = CastField<FStructProperty>(Field(result->type, L"ID"));
        if (!identity || identity->GetStruct()->GetPathName() != L"/Script/Dominion.DomId" || identity->GetSize() != sizeof(uint32))
            throw std::runtime_error("Native player modifier identity differs: " + (identity
                ? RC::to_string(identity->GetStruct()->GetPathName()) + " size=" + std::to_string(identity->GetSize()) : "not a struct"));
        UObject* instance = nullptr;
        for (auto* name : {L"ReplicatedInstances", L"NonReplicatedInstances"}) {
            auto* property = CastField<FArrayProperty>(Field(result->component.Get()->GetClassPrivate(), name));
            auto* inner = property ? CastField<FObjectPropertyBase>(property->GetInner()) : nullptr;
            if (!inner) throw std::runtime_error("Native player modifier instance array differs");
            auto* array = property->ContainerPtrToValuePtr<FScriptArray>(result->component.Get());
            if (array->Num() < 0 || array->Num() > 512 || (array->Num() && !array->GetData()))
                throw std::runtime_error("Native player modifier instance count differs");
            FScriptArrayHelper values(property, array);
            for (int index = 0; index < array->Num(); ++index) {
                auto* candidate = inner->GetObjectPropertyValue(values.GetRawPtr(index));
                if (!Live(candidate) || !candidate->IsA(Find<UClass>(L"/Script/Dominion.DominionGameplayEffect"))) continue;
                Call key(result->component.Get(), L"HandleFromGameplayEffect");
                key.Object(L"EffectInstance", candidate); key.Invoke();
                auto* keyType = CastField<FStructProperty>(key.Prop(L"ReturnValue"));
                if (!keyType || keyType->GetStruct().Get() != result->type)
                    throw std::runtime_error("Native player modifier lookup contract differs");
                if (std::memcmp(identity->ContainerPtrToValuePtr<void>(result->handle.data()),
                    identity->ContainerPtrToValuePtr<void>(key.Data(L"ReturnValue")), identity->GetSize()) == 0) {
                    instance = candidate; break;
                }
            }
            if (instance) break;
        }
        if (!instance) throw std::runtime_error("Native player modifier instance was not registered");
        Call read(instance, L"GetInstancedData"); read.Invoke();
        auto* property = CastField<FStructProperty>(read.Prop(L"ReturnValue"));
        if (!property || property->GetStruct()->GetPathName() != L"/Script/Dominion.DominionGameplayEffectInstancedData")
            throw std::runtime_error("Native player modifier data contract differs");
        auto* data = read.Data(L"ReturnValue");
        auto* fields = property->GetStruct().Get();
        Enum(fields, data, L"DurationType", L"Infinite");
        Enum(fields, data, L"ModifierOperation", L"Add");
        Enum(fields, data, L"ModifierApplyType", L"OnApply");
        for (auto* name : {L"bCanTickPeriodically", L"bTickIntervalImmediately", L"bAmountHasMinClamp", L"bAmountHasMaxClamp", L"bShowInTooltip"})
            Boolean(fields, data, name, false);
        Number(fields, data, L"PreMultiplyAdditiveValue", 0);
        Number(fields, data, L"PostMultiplyAdditiveValue", 0);
        Number(fields, data, L"AmountToModifyScale", 1);
        auto* amount = CastField<FStructProperty>(Field(fields, L"AmountToModify"));
        if (!amount) throw std::runtime_error("Native player modifier amount differs");
        auto* amountData = amount->ContainerPtrToValuePtr<void>(data);
        amount->DestroyValue(amountData); amount->InitializeValue(amountData);
        Number(amount->GetStruct(), amountData, L"Value", delta);
        Call write(instance, L"SetGameplayEffectInstancedData");
        write.Copy(L"InData", data); write.Invoke();
        Call can(result->component.Get(), L"CanApplyGameplayEffect");
        can.Copy(L"Handle", result->handle.data()); can.Invoke();
        if (!can.Bool()) throw std::runtime_error("Native player modifier was rejected");
        return result;
    }
    void Apply(RC::Unreal::UObject* pawn) {
        Buildings::Call call(component.Get(), L"ApplyGameplayEffect");
        call.Object(L"Instigator", pawn); call.Object(L"Source", pawn);
        call.Copy(L"Handle", handle.data()); call.Text(L"bIgnoreChanceToApply", L"True"); call.Invoke();
        if (!call.Bool()) throw std::runtime_error("Native player modifier application failed");
        applied = true;
    }
public:
    ~PlayerAttributeEffect() {
        try { Remove(); } catch (...) {}
        if (type) type->DestroyStruct(handle.data());
    }
    static const wchar_t* Template(RC::Unreal::UObject* attribute) {
        if (!attribute) return nullptr;
        for (const auto& [name, path] : {
            std::pair{L"/Script/Dominion.MaxHealthAttribute", L"/Game/Gameplay/GameplayEffects/Effects/GE_ModifyMaxHealth.GE_ModifyMaxHealth_C"},
            std::pair{L"/Script/Dominion.MaxStaminaAttribute", L"/Game/Gameplay/GameplayEffects/Effects/GE_ModifyMaxStamina.GE_ModifyMaxStamina_C"},
            std::pair{L"/Script/Dominion.MaxCarryWeightAttribute", L"/Game/Gameplay/GameplayEffects/SurvivalCore/GE_ModifyCarryWeightMax_Endurance.GE_ModifyCarryWeightMax_Endurance_C"}}) {
            auto* cls = RC::Unreal::UObjectGlobals::StaticFindObject<RC::Unreal::UClass*>(nullptr, nullptr, name);
            if (cls && attribute->IsA(cls)) return path;
        }
        return nullptr;
    }
    static double Base(RC::Unreal::UObject* attributes, RC::Unreal::UObject* attribute) {
        using namespace RC::Unreal;
        using namespace Buildings;
        // BaseValue is the effective result, including active native effects.
        auto* table = Cast<UDataTable>(Object(attributes, L"DefaultAttributes"));
        auto* rows = table ? Cast<UScriptStruct>(Object(table, L"RowStruct")) : nullptr;
        if (!rows || rows->GetPathName() != L"/Script/Dominion.AttributeDefaultsRow"
            || table->GetRowMap().Num() < 0 || table->GetRowMap().Num() > 4096)
            throw std::runtime_error("Native player default attribute table differs");
        auto* key = CastField<FObjectPropertyBase>(Field(rows, L"Attribute"));
        auto* starting = CastField<FStrProperty>(Field(rows, L"StartingValue"));
        auto* scale = CastField<FStructProperty>(Field(rows, L"DifficultySettingScaleTag"));
        if (!key || !starting || !scale || scale->GetStruct()->GetPathName() != L"/Script/GameplayTags.GameplayTag")
            throw std::runtime_error("Native player default attribute row differs");
        auto* data = Object(attribute, L"AttributeData");
        for (const auto& row : table->GetRowMap()) {
            if (!row.Value || key->GetObjectPropertyValue(key->ContainerPtrToValuePtr<void>(row.Value)) != data) continue;
            const auto& text = starting->ContainerPtrToValuePtr<FString>(row.Value)->GetCharArray();
            if (text.Num() < 1 || text.Num() > 128 || !text.GetData() || text[text.Num() - 1] != 0)
                throw std::runtime_error("Native player starting value is invalid");
            auto literal = RC::to_string(text.GetData());
            const auto first = literal.find_first_not_of(" \t\r\n");
            if (first == literal.npos) throw std::runtime_error("Native player starting value is empty");
            literal = literal.substr(first, literal.find_last_not_of(" \t\r\n") - first + 1);
            double value = 0;
            // shortcut: resolve numeric defaults only; add native expression evaluation if a supported table needs it.
            const auto parsed = std::from_chars(literal.data(), literal.data() + literal.size(), value);
            if (parsed.ec != std::errc{} || parsed.ptr != literal.data() + literal.size() || !std::isfinite(value))
                throw std::runtime_error("Native player starting value is not a finite numeric literal");
            auto* tag = scale->ContainerPtrToValuePtr<void>(row.Value);
            auto* name = CastField<FNameProperty>(Field(scale->GetStruct(), L"TagName"));
            if (!name || name->GetSize() != sizeof(FName))
                throw std::runtime_error("Native player difficulty tag differs");
            if (name->ContainerPtrToValuePtr<FName>(tag)->ToString() != L"None") {
                Call difficulty(Find<UObject>(L"/Script/Dominion.Default__DifficultyFunctionLibrary"), L"GetSettingValueAsFloat");
                difficulty.Object(L"WorldContextObject", attributes);
                difficulty.Copy(L"SettingTag", tag); difficulty.Invoke();
                auto* number = CastField<FFloatProperty>(difficulty.Prop(L"ReturnValue"));
                if (!number) throw std::runtime_error("Native player difficulty scale differs");
                value *= number->GetPropertyValue(difficulty.Data(L"ReturnValue"));
            }
            if (!std::isfinite(value) || value <= 0)
                throw std::runtime_error("Native player default maximum is invalid");
            return value;
        }
        throw std::runtime_error("Native player default maximum was not found");
    }
    bool Exists() const {
        if (!applied || !Buildings::Live(component.Get())) return false;
        Buildings::Call call(component.Get(), L"HasGameplayEffect");
        call.Copy(L"Handle", handle.data()); call.Invoke(); return call.Bool();
    }
    void Remove() {
        if (owned && Buildings::Live(component.Get())) {
            Buildings::Call call(component.Get(), L"RemoveGameplayEffect");
            call.Copy(L"Handle", handle.data()); call.Invoke();
            if (!call.Bool() && Exists()) throw std::runtime_error("Native player modifier could not be removed");
        }
        applied = false;
        owned = false;
    }
    static bool Set(RC::Unreal::UObject* pawn, RC::Unreal::UObject* attribute,
        double base, double desired, std::shared_ptr<PlayerAttributeEffect>& current) {
        const float delta = float(desired - base);
        if (!std::isfinite(base) || !std::isfinite(delta)) throw std::runtime_error("Player modifier delta is invalid");
        if (current && current->delta == delta && current->Exists()) return true;
        if (delta == 0) { if (current) current->Remove(); current.reset(); return true; }
        auto next = Create(pawn, attribute, delta);
        try {
            if (current) current->Remove();
            next->Apply(pawn);
        } catch (...) {
            if (current && !current->Exists()) {
                auto restored = Create(pawn, attribute, current->delta);
                restored->Apply(pawn); current = std::move(restored);
            }
            throw;
        }
        current = std::move(next);
        return current->Exists();
    }
};
}
