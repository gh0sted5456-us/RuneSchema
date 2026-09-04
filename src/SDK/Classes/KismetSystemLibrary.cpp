#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include "Utility/Logging.h"

#include <new>
#include <vector>

using namespace RC;
using namespace RC::Unreal;

namespace UECustom {
    RC::Unreal::UObject* UKismetSystemLibrary::LoadAsset_Blocking(UECustom::TSoftObjectPtr<UObject> Asset)
    {
        static auto Function = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr, nullptr, TEXT("/Script/Engine.KismetSystemLibrary:LoadAsset_Blocking"));

        if (!Function)
        {
            PS::Log<LogLevel::Error>(STR("Function /Script/Engine.KismetSystemLibrary:LoadAsset_Blocking was invalid.\n"));
            return nullptr;
        }

        auto* assetProperty = Function->FindProperty(FName(TEXT("Asset"), FNAME_Find));
        auto* returnProperty = Function->GetReturnProperty();
        if (!assetProperty || !returnProperty)
        {
            PS::Log<LogLevel::Error>(STR("LoadAsset_Blocking parameter metadata was invalid.\n"));
            return nullptr;
        }

        const auto paramsSize = Function->GetParmsSize();
        std::vector<uint8> params(paramsSize, 0);
        const auto assetOffset = assetProperty->GetOffset_Internal();
        const auto returnOffset = returnProperty->GetOffset_Internal();
        if (assetOffset + sizeof(Asset) > params.size()
            || returnOffset + sizeof(UObject*) > params.size())
        {
            PS::Log<LogLevel::Error>(STR("LoadAsset_Blocking parameter layout was out of bounds.\n"));
            return nullptr;
        }

        new (params.data() + assetOffset) UECustom::TSoftObjectPtr<UObject>(Asset);
        GetDefaultObj()->ProcessEvent(Function, params.data());
        return *reinterpret_cast<UObject**>(params.data() + returnOffset);
    }

    RC::Unreal::UClass* UKismetSystemLibrary::LoadClassAsset_Blocking(UECustom::TSoftClassPtr<UObject> AssetClass)
    {
        static auto Function = UECustom::UObjectGlobals::StaticFindObject<UFunction*>(
            nullptr, nullptr, TEXT("/Script/Engine.KismetSystemLibrary:LoadClassAsset_Blocking"));

        if (!Function)
        {
            PS::Log<LogLevel::Error>(STR("Function /Script/Engine.KismetSystemLibrary:LoadClassAsset_Blocking was invalid.\n"));
            return nullptr;
        }

        auto* assetClassProperty = Function->FindProperty(FName(TEXT("AssetClass"), FNAME_Find));
        auto* returnProperty = Function->GetReturnProperty();
        if (!assetClassProperty || !returnProperty)
        {
            PS::Log<LogLevel::Error>(STR("LoadClassAsset_Blocking parameter metadata was invalid.\n"));
            return nullptr;
        }

        const auto paramsSize = Function->GetParmsSize();
        std::vector<uint8> params(paramsSize, 0);
        const auto assetClassOffset = assetClassProperty->GetOffset_Internal();
        const auto returnOffset = returnProperty->GetOffset_Internal();
        if (assetClassOffset + sizeof(AssetClass) > params.size()
            || returnOffset + sizeof(UClass*) > params.size())
        {
            PS::Log<LogLevel::Error>(STR("LoadClassAsset_Blocking parameter layout was out of bounds.\n"));
            return nullptr;
        }

        new (params.data() + assetClassOffset) UECustom::TSoftClassPtr<UObject>(AssetClass);
        GetDefaultObj()->ProcessEvent(Function, params.data());
        return *reinterpret_cast<UClass**>(params.data() + returnOffset);
    }

	UKismetSystemLibrary* UKismetSystemLibrary::GetDefaultObj()
	{
		static auto Self = UECustom::UObjectGlobals::StaticFindObject<UKismetSystemLibrary*>(nullptr, nullptr, TEXT("/Script/Engine.Default__KismetSystemLibrary"));
		return Self;
	}
}
