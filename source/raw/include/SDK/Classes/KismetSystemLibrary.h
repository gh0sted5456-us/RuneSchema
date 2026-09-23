#pragma once

#include "Unreal/UObject.hpp"
#include "Unreal/UnrealCoreStructs.hpp"
#include "SDK/Classes/TSoftClassPtr.h"
#include "SDK/Classes/TSoftObjectPtr.h"

#include <string>
#include <vector>

namespace RC::Unreal { class AActor; }


namespace UECustom {
	class UKismetSystemLibrary : public RC::Unreal::UObject {
	public:
		static RC::Unreal::UObject* LoadAsset_Blocking(UECustom::TSoftObjectPtr<RC::Unreal::UObject> Asset);

        static RC::Unreal::UClass* LoadClassAsset_Blocking(UECustom::TSoftClassPtr<RC::Unreal::UObject> AssetClass);

        static bool LineTraceGround(
            RC::Unreal::UObject* WorldContext,
            const RC::Unreal::FVector& Start,
            const RC::Unreal::FVector& End,
            const std::vector<RC::Unreal::AActor*>& ActorsToIgnore,
            RC::Unreal::FVector& ImpactPoint,
            std::string& Error,
            RC::Unreal::UObject** HitComponent = nullptr);
	private:
		static UKismetSystemLibrary* GetDefaultObj();
	};
}
