#pragma once
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Helper/ReflectedContract.h"
#include <chrono>
#include <cmath>
#include <optional>
#include <nlohmann/json.hpp>
namespace PS::PlayerVitals {
inline int64_t Now(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
// Only native, no-input getters with a checked return contract are permitted.
// RPCs, notifications and arbitrary Blueprint actions are never stat readers.
inline RC::Unreal::UFunction* Getter(const TCHAR* path){
    using namespace RC::Unreal;namespace C=DragonWilds::ReflectedContract;
    auto* fn=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path,false);
    if(!fn||!fn->HasAnyFunctionFlags(FUNC_Native)||fn->HasAnyFunctionFlags(FUNC_Net)||C::Count(fn)!=1||!C::Output(fn->GetReturnProperty(),fn))return nullptr;return fn;
}
inline std::optional<double> Number(RC::Unreal::UObject* object,const TCHAR* path){
    using namespace RC::Unreal;if(!object)return {};auto* fn=Getter(path);auto* field=fn?CastField<FNumericProperty>(fn->GetReturnProperty()):nullptr;
    if(!field||!field->IsFloatingPoint())return {};DragonWilds::ActorHelper::FunctionCall call(object,fn);call.Invoke();const auto value=call.NumericResult();return std::isfinite(value)?std::optional<double>{value}:std::nullopt;
}
inline RC::Unreal::UObject* Component(RC::Unreal::UObject* pawn,const TCHAR* path,const TCHAR* expected){
    using namespace RC::Unreal;auto* type=DragonWilds::ActorHelper::ResolveClass(expected);auto* fn=Getter(path);
    auto* field=fn?CastField<FObjectProperty>(fn->GetReturnProperty()):nullptr;if(!type||!field||field->GetPropertyClass().Get()!=type)return nullptr;
    DragonWilds::ActorHelper::FunctionCall call(pawn,fn);call.Invoke();auto* result=call.Result<UObject*>();return result&&result->IsA(type)?result:nullptr;
}
// Read-only presentation. Missing/changed native fields mean unknown, not zero HP.
inline nlohmann::json Read(RC::Unreal::UObject* pawn){
    using namespace RC::Unreal;namespace AH=DragonWilds::ActorHelper;namespace PH=DragonWilds::PropertyHelper;namespace Contract=DragonWilds::ReflectedContract;
    try{
        if(!pawn)return nullptr;auto* playerType=AH::ResolveClass(TEXT("/Script/Dominion.DominionPlayerCharacter"));if(!playerType||!pawn->IsA(playerType))return nullptr;
        auto* health=AH::GetObjectRef(pawn,TEXT("HealthComponent"));
        if(!health){
            auto* getter=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,TEXT("/Script/Dominion.DominionPlayerCharacter:GetHealthComponent"),false);
            if(!getter||Contract::Count(getter)!=1||!CastField<FObjectProperty>(getter->GetReturnProperty())||!Contract::Output(getter->GetReturnProperty(),getter))return nullptr;
            AH::FunctionCall get(pawn,getter);get.Invoke();health=get.Result<UObject*>();
        }
        auto* type=AH::ResolveClass(TEXT("/Script/Dominion.HealthComponent"));if(!health||!type||!health->IsA(type))return nullptr;
        auto* current=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,TEXT("/Script/Dominion.HealthComponent:GetAuthoritativeHealth"),false);
        auto* currentResult=current?CastField<FNumericProperty>(current->GetReturnProperty()):nullptr;
        if(!currentResult||!currentResult->IsFloatingPoint()||Contract::Count(current)!=1||!Contract::Output(currentResult,current))return nullptr;
        auto* maximum=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,TEXT("/Script/Dominion.HealthComponent:GetMaxHealth"),false);
        auto* result=maximum?CastField<FNumericProperty>(maximum->GetReturnProperty()):nullptr;
        if(!result||!result->IsFloatingPoint()||Contract::Count(maximum)!=1||!Contract::Output(result,maximum))return nullptr;
        AH::FunctionCall getMax(health,maximum);getMax.Invoke();const double limit=getMax.NumericResult();
        AH::FunctionCall getCurrent(health,current);getCurrent.Invoke();const double value=getCurrent.NumericResult();
        if(!std::isfinite(value)||!std::isfinite(limit)||value<0||limit<=0||limit>1e9||value>limit)return nullptr;
        return {{"Current",value},{"Maximum",limit},{"ObservedUnixMs",std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()}};
    }catch(...){return nullptr;}
}
inline nlohmann::json SurvivalGauge(RC::Unreal::UObject* pawn,const TCHAR* componentName,const TCHAR* classPath,const TCHAR* currentName,const TCHAR* maximumName){
    using namespace RC::Unreal;namespace AH=DragonWilds::ActorHelper;namespace PH=DragonWilds::PropertyHelper;namespace C=DragonWilds::ReflectedContract;
    try{
        if(!pawn)return nullptr;auto* type=AH::ResolveClass(classPath);auto* ref=CastField<FObjectProperty>(PH::GetPropertyByName(pawn->GetClassPrivate(),componentName));
        if(!type||!ref||!C::Fits(ref,pawn->GetClassPrivate()->GetPropertiesSize())||ref->GetPropertyClass().Get()!=type)return nullptr;
        auto* component=AH::GetObjectRef(pawn,componentName);if(!component||!component->IsA(type)||component->GetOuterPrivate()!=pawn)return nullptr;
        const auto read=[&](const TCHAR* field)->std::optional<double>{auto* p=CastField<FFloatProperty>(PH::GetPropertyByName(component->GetClassPrivate(),field));if(!p||!C::Fits(p,component->GetClassPrivate()->GetPropertiesSize()))return {};return p->GetFloatingPointPropertyValue(p->ContainerPtrToValuePtr<void>(component));};
        const auto value=read(currentName),maximum=read(maximumName);
        if(!value||!maximum||!std::isfinite(*value)||!std::isfinite(*maximum)||*value<0||*maximum<=0||*maximum>1e9||*value>*maximum)return nullptr;
        return {{"Current",*value},{"Maximum",*maximum},{"ObservedUnixMs",Now()}};
    }catch(...){return nullptr;}
}
inline nlohmann::json Details(RC::Unreal::UObject* pawn,bool authoritative){
    using Json=nlohmann::json;const auto unavailable=[](const char* reason){return Json{{"Reason",reason}};};
    Json result={{"Health",Read(pawn)},{"Stamina",unavailable("Native stamina getter unavailable")},
        {"SpecialPower",unavailable("Native special-charge getter unavailable")},
        {"Hunger",unavailable("Native sustenance gauge unavailable")},
        {"Thirst",unavailable("Native hydration gauge unavailable")},
        {"RestLevel",unavailable("Native endurance gauge unavailable")},
        {"Food",unavailable("Native food effects unavailable")},
        {"Rest",unavailable("Native rest state unavailable")}};
    try{
        auto* type=DragonWilds::ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerCharacter"));if(!pawn||!type||!pawn->IsA(type))return result;
        const auto at=Now();
        const auto hunger=SurvivalGauge(pawn,TEXT("SustenanceComponent"),TEXT("/Script/Dominion.SustenanceComponent"),TEXT("CurrentSustenance"),TEXT("CurrentMaxSustenance"));if(!hunger.is_null())result["Hunger"]=hunger;
        const auto thirst=SurvivalGauge(pawn,TEXT("HydrationComponent"),TEXT("/Script/Dominion.HydrationComponent"),TEXT("CurrentHydration"),TEXT("CurrentMaxHydration"));if(!thirst.is_null())result["Thirst"]=thirst;
        const auto rest=SurvivalGauge(pawn,TEXT("EnduranceComponent"),TEXT("/Script/Dominion.EnduranceComponent"),TEXT("CurrentEndurance"),TEXT("MaxEndurance"));if(!rest.is_null())result["RestLevel"]=rest;
        try{if(const auto power=Number(pawn,TEXT("/Script/Dominion.DominionPlayerCharacter:BP_GetPowerLevel"));power&&*power>=0&&*power<=1e6)
            result["PowerLevel"]={{"Value",*power},{"ObservedUnixMs",at}};
        }catch(...){}
        try{if(auto* stamina=Component(pawn,TEXT("/Script/Dominion.DominionPlayerCharacter:GetStaminaComponent"),TEXT("/Script/Dominion.StaminaComponent"))){
            const auto value=Number(stamina,TEXT("/Script/Dominion.StaminaComponent:GetStamina")),maximum=Number(stamina,TEXT("/Script/Dominion.StaminaComponent:GetMaxStamina"));
            if(value&&maximum&&*value>=0&&*maximum>0&&*maximum<=1e9&&*value<=*maximum)result["Stamina"]={{"Current",*value},{"Maximum",*maximum},{"ObservedUnixMs",at}};
        }}catch(...){}
        try{if(auto* special=Component(pawn,TEXT("/Script/Dominion.DominionPlayerCharacter:GetSpecialChargeComponent"),TEXT("/Script/Dominion.SpecialChargeComponent"))){
            const auto value=Number(special,TEXT("/Script/Dominion.SpecialChargeComponent:GetSpecialCharge")),maximum=Number(special,TEXT("/Script/Dominion.SpecialChargeComponent:GetMaxSpecialCharge"));
            if(value&&maximum&&*value>=0&&*maximum>0&&*maximum<=1e9&&*value<=*maximum)result["SpecialPower"]={{"Current",*value},{"Maximum",*maximum},{"ObservedUnixMs",at}};
        }}catch(...){}
        try{if(auto* rest=Component(pawn,TEXT("/Script/Dominion.DominionPlayerCharacter:GetPlayerRestComponent"),TEXT("/Script/Dominion.PlayerRestComponent"))){
            using namespace RC::Unreal;auto* fn=Getter(TEXT("/Script/Dominion.PlayerRestComponent:IsResting"));
            if(fn&&CastField<FBoolProperty>(fn->GetReturnProperty())){DragonWilds::ActorHelper::FunctionCall call(rest,fn);call.Invoke();result["Rest"]={{"State",call.Result<bool>()?"Resting":"Not resting"},{"ObservedUnixMs",at}};}
            auto* sleeping=Getter(TEXT("/Script/Dominion.PlayerRestComponent:GetIsSleeping"));if(sleeping&&CastField<FBoolProperty>(sleeping->GetReturnProperty())){DragonWilds::ActorHelper::FunctionCall call(rest,sleeping);call.Invoke();if(call.Result<bool>())result["Rest"]={{"State","Sleeping"},{"ObservedUnixMs",at}};}
        }}catch(...){}
        try{if(auto* food=Component(pawn,TEXT("/Script/Dominion.DominionPlayerCharacter:GetFoodComponent"),TEXT("/Script/Dominion.FoodComponent"))){
            using namespace RC::Unreal;namespace PH=DragonWilds::PropertyHelper;namespace C=DragonWilds::ReflectedContract;
            auto* array=CastField<FArrayProperty>(PH::GetPropertyByName(food->GetClassPrivate(),TEXT("ActiveFoodSlots")));
            auto* inner=array?CastField<FStructProperty>(array->GetInner()):nullptr;
            auto* capacity=CastField<FIntProperty>(PH::GetPropertyByName(food->GetClassPrivate(),TEXT("MaxFoodSlots")));
            if(array&&C::Fits(array,food->GetClassPrivate()->GetPropertiesSize())&&array->GetElementSize()==sizeof(FScriptArray)&&inner&&inner->GetStruct()&&RC::to_string(inner->GetStruct()->GetPathName())=="/Script/Dominion.ActiveFood"&&capacity&&C::Fits(capacity,food->GetClassPrivate()->GetPropertiesSize())){
                auto* slots=array->ContainerPtrToValuePtr<FScriptArray>(food);const auto max=capacity->GetSignedIntPropertyValue(capacity->ContainerPtrToValuePtr<void>(food));
                auto* item=CastField<FObjectProperty>(PH::GetPropertyByName(inner->GetStruct().Get(),TEXT("Food")));
                const auto stride=inner->GetElementSize();const auto size=inner->GetStruct()->GetPropertiesSize();
                if(slots&&max>0&&max<=32&&slots->Num()>=0&&slots->Num()<=32&&(!slots->Num()||slots->GetData())&&stride==size&&size>0&&size<=1024&&item&&C::Fits(item,size)){
                    int active=0;for(int i=0;i<slots->Num();++i){auto* entry=static_cast<uint8_t*>(slots->GetData())+static_cast<size_t>(i)*stride;if(*item->ContainerPtrToValuePtr<UObject*>(entry))++active;}
                    if(active<=max)result["Food"]={{"State",std::to_string(active)+" / "+std::to_string(max)+" food effects active"},{"ActiveSlots",active},{"MaximumSlots",max},{"ObservedUnixMs",at}};
                }
            }
        }}catch(...){}
    }catch(...){} // A missing stat must not suppress other independently read stats.
    result["Source"]=authoritative?"Host/server native":"Local/replicated native";
    return result;
}
}
