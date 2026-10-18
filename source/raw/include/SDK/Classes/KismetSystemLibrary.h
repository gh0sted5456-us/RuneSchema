#pragma once

#include "Unreal/UObject.hpp"
#include "Unreal/UnrealCoreStructs.hpp"
#include "SDK/Classes/TSoftClassPtr.h"
#include "SDK/Classes/TSoftObjectPtr.h"
#include "Core/GroundSurface.h"

#include <string>
#include <vector>
#include <optional>

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
            RC::Unreal::UObject** HitComponent = nullptr,
            bool TraceComplex = false,
            RC::Unreal::FVector* ImpactNormal = nullptr,
            bool* StartPenetrating = nullptr);
        // Vertical placement query shared by NPC, spawn, event and authoring
        // loaders. Inspection rays retain the visibility-only helper above.
        static bool FindGroundSurface(
            RC::Unreal::UObject* WorldContext,
            const RC::Unreal::FVector& Start,const RC::Unreal::FVector& End,
            const std::vector<RC::Unreal::AActor*>& ActorsToIgnore,
            RC::Unreal::FVector& ImpactPoint,std::string& Error,
            std::optional<double> ReferenceZ = {},PS::Grounding::QueryState* State = nullptr);
        // A true result means blocked, not clear. Error is distinct from
        // occupancy; callers must not migrate around a broken native contract.
        static bool CapsuleBlocked(RC::Unreal::UObject* WorldContext,
            const RC::Unreal::FVector& Centre,double Radius,double HalfHeight,
            const std::vector<RC::Unreal::AActor*>& ActorsToIgnore,std::string& Error);
	private:
		static bool TraceCollision(RC::Unreal::UObject* WorldContext,
            const RC::Unreal::FVector& Start,const RC::Unreal::FVector& End,
            const std::vector<RC::Unreal::AActor*>& ActorsToIgnore,
            RC::Unreal::FVector& ImpactPoint,std::string& Error,
            RC::Unreal::UObject** HitComponent,bool TraceComplex,
            RC::Unreal::FVector* ImpactNormal,bool* StartPenetrating,
            double Radius=0,double HalfHeight=0);
		static UKismetSystemLibrary* GetDefaultObj();
	};
}
