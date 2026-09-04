#pragma once

#include "Unreal/UObject.hpp"
#include "SDK/Classes/TSoftClassPtr.h"
#include "SDK/Classes/TSoftObjectPtr.h"


namespace UECustom {
	class UKismetSystemLibrary : public RC::Unreal::UObject {
	public:
		static RC::Unreal::UObject* LoadAsset_Blocking(UECustom::TSoftObjectPtr<RC::Unreal::UObject> Asset);

        static RC::Unreal::UClass* LoadClassAsset_Blocking(UECustom::TSoftClassPtr<RC::Unreal::UObject> AssetClass);
	private:
		static UKismetSystemLibrary* GetDefaultObj();
	};
}
