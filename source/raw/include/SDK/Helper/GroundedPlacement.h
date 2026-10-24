#pragma once
#include "Core/NearbyPlacement.h"
#include "Core/SpawnFootprint.h"
#include "Core/GroundSurface.h"
#include "Core/GroundingRetry.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include <Unreal/World.hpp>

namespace DragonWilds::GroundedPlacement {
inline std::optional<PS::SpawnFootprint::Capsule> NativeCapsule(RC::Unreal::UObject* defaults) {
    if(!defaults)return {};
    auto* property=RC::Unreal::CastField<RC::Unreal::FObjectProperty>(PropertyHelper::GetPropertyByName(defaults->GetClassPrivate(),TEXT("CapsuleComponent")));
    if(!property)return {};
    auto* capsule=property->GetObjectPropertyValue(property->ContainerPtrToValuePtr<void>(defaults));
    auto* type=ActorHelper::ResolveClass(TEXT("/Script/Engine.CapsuleComponent"));
    if(!capsule||!type||!capsule->IsA(type)||capsule->GetOuterPrivate()!=defaults
        ||ActorHelper::GetObjectRef(defaults,TEXT("RootComponent"))!=capsule)return {};
    ActorHelper::FunctionCall height(capsule,TEXT("/Script/Engine.CapsuleComponent:GetScaledCapsuleHalfHeight"));height.Invoke();
    ActorHelper::FunctionCall radius(capsule,TEXT("/Script/Engine.CapsuleComponent:GetScaledCapsuleRadius"));radius.Invoke();
    return PS::SpawnFootprint::Scaled({radius.NumericResult(),height.NumericResult()},1);
}
// The original floor remains the reference for every candidate: a search
// around a staircase must not choose a bridge/roof or a lower cave floor.
inline RC::Unreal::FVector CapsuleSite(RC::Unreal::UWorld* world,
    const RC::Unreal::FVector& foot,PS::SpawnFootprint::Capsule capsule,
    const std::vector<RC::Unreal::AActor*>& ignored,PS::Grounding::NearbyPolicy policy,
    const std::vector<RC::Unreal::AActor*>& clearanceIgnored={},
    const std::vector<PS::SpawnFootprint::Reservation>& reservations={},
    std::optional<double> clearanceHint={}) {
    using namespace PS::Grounding;
    using RC::Unreal::FVector;
    const auto range=SearchRange::Around(foot.Z());
    if(clearanceHint&&(!std::isfinite(*clearanceHint)||*clearanceHint<0))
        throw std::runtime_error("Grounded capsule requires non-negative foot clearance");
    double referenceZ=foot.Z()-clearanceHint.value_or(0);
    std::optional<double> originalFloor;
    const auto ground=[&](double x,double y,bool* obstructed=nullptr)->std::optional<double> {
        FVector hit{};std::string error;
        QueryState state;
        if(!UECustom::UKismetSystemLibrary::FindGroundSurface(world,
            FVector(x,y,range.Top),FVector(x,y,range.Bottom),ignored,hit,error,referenceZ,&state)) {
            if(!error.empty())throw std::runtime_error("Spawn footprint ground contract failed: "+error);
            if(obstructed&&state.Obstructed&&state.FloorReference){*obstructed=true;return state.FloorReference;}
            return {};
        }
        if(originalFloor&&!SameNearbyFloor(*originalFloor,hit.Z()))return {};
        return hit.Z();
    };
    bool originalObstructed=false;
    const auto originalGround=ground(foot.X(),foot.Y(),&originalObstructed);
    if(!originalGround)throw SurfacePending("Spawn footprint has no verified floor at its requested XY");
    originalFloor=*originalGround;referenceZ=*originalGround;
    const double clearance=clearanceHint.value_or(foot.Z()-*originalGround);
    if(!std::isfinite(clearance)||clearance<0)
        throw std::runtime_error("Grounded capsule requires non-negative foot clearance");
    auto selected=Nearby(policy,[&](double dx,double dy)->Site {
        const double x=foot.X()+dx,y=foot.Y()+dy;
        bool obstructed=false;const auto centre=ground(x,y,&obstructed);
        if(obstructed)return {SiteState::Blocked,{}};
        if(!centre)return {SiteState::Pending,{}};
        double originZ=0;
        try {
            originZ=PS::SpawnFootprint::OriginZ(capsule,*centre+clearance,[&](double offsetX,double offsetY) {
                const auto sample=ground(x+offsetX,y+offsetY);
                if(!sample)throw SurfacePending();
                return *sample;
            });
        }catch(const PS::SpawnFootprint::Unsafe&){return {SiteState::Blocked,{}};}
        catch(const SurfacePending&){return {SiteState::Blocked,{}};}
        for(const auto& reserved:reservations)
            if(PS::SpawnFootprint::Overlaps({{x,y,originZ},capsule},reserved))return {SiteState::Blocked,{}};
        std::string error;
        const bool blocked=UECustom::UKismetSystemLibrary::CapsuleBlocked(world,FVector(x,y,originZ),
            capsule.Radius,capsule.HalfHeight,clearanceIgnored,error);
        if(!error.empty())throw std::runtime_error("Spawn clearance contract failed: "+error);
        return {blocked?SiteState::Blocked:SiteState::Safe,{x,y,originZ}};
    });
    if(!selected)throw SurfacePending("No collision-clear capsule site on the verified floor within GroundMigrationRadius="+std::to_string(policy.Radius)+" cm");
    return FVector(selected->Position[0],selected->Position[1],selected->Position[2]);
}
}
