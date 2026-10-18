#pragma once
#include "SDK/Helper/PlayerVitals.h"
namespace PS::ResourceObservation {
inline bool Near(const nlohmann::json& location,const nlohmann::json& players,double metres){
    if(!location.is_array()||location.size()!=3||!players.is_array()||!std::isfinite(metres)||metres<25||metres>2000)return false;
    try{const double x=location[0].get<double>(),y=location[1].get<double>(),radius=metres*100;
        if(!std::isfinite(x)||!std::isfinite(y))return false;size_t count=0;
        for(const auto& player:players){if(++count>256)break;try{const auto& at=player.at("Location");const double px=at.at(0).get<double>(),py=at.at(1).get<double>();
            if(std::isfinite(px)&&std::isfinite(py)&&std::hypot(px-x,py-y)<=radius)return true;}catch(...){}}
    }catch(...){}return false;
}
inline nlohmann::json Read(RC::Unreal::UObject* actor){
    using namespace RC::Unreal;namespace AH=DragonWilds::ActorHelper;namespace PH=DragonWilds::PropertyHelper;namespace C=DragonWilds::ReflectedContract;
    nlohmann::json result={{"ObservedUnixMs",PlayerVitals::Now()},{"TimerReason","Native respawn deadline not verified; no guessed countdown"}};
    try{
        auto* base=AH::ResolveClass(TEXT("/Script/Dominion.BaseInteractableResource"));auto* type=AH::ResolveClass(TEXT("/Script/Dominion.ResourceRespawnComponent"));
        if(!actor||!base||!type||!actor->IsA(base))return result;
        auto* property=CastField<FObjectProperty>(PH::GetPropertyByName(actor->GetClassPrivate(),TEXT("RespawnComponent")));
        if(!property||!C::Fits(property,actor->GetClassPrivate()->GetPropertiesSize())||property->GetPropertyClass().Get()!=type)return result;
        auto* component=AH::GetObjectRef(actor,TEXT("RespawnComponent"));if(!component||!component->IsA(type))return result;
        const auto read=[&](const TCHAR* path)->std::optional<int32_t>{auto* fn=PlayerVitals::Getter(path);if(!fn||!CastField<FIntProperty>(fn->GetReturnProperty()))return {};AH::FunctionCall call(component,fn);call.Invoke();return call.Result<int32_t>();};
        const auto quantity=read(TEXT("/Script/Dominion.ResourceRespawnComponent:GetQuantityAvailable")),maximum=read(TEXT("/Script/Dominion.ResourceRespawnComponent:GetMaxResources"));
        if(quantity&&maximum&&*quantity>=0&&*maximum>0&&*quantity<=*maximum){result["Quantity"]=*quantity;result["Maximum"]=*maximum;result["State"]=*quantity==0?"Depleted":"Available";}
        auto* paused=CastField<FBoolProperty>(PH::GetPropertyByName(component->GetClassPrivate(),TEXT("bIsRespawningPaused")));
        if(paused&&C::Fits(paused,component->GetClassPrivate()->GetPropertiesSize())){const bool value=paused->GetPropertyValueInContainer(component);result["Paused"]=value;if(value)result["TimerReason"]="Respawn is paused by native rules";}
    }catch(...){}return result;
}
}
