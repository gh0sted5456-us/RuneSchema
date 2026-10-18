#pragma once
#include "Core/PlayerStatus.h"
#include "Runtime/NetworkContext.h"
#include "SDK/Helper/ReflectedContract.h"
#include <Unreal/Property/FStructProperty.hpp>
#include <Unreal/UEnum.hpp>
#include <cstring>
#include <format>
namespace PS::PlayerStatusRuntime {
using namespace RC::Unreal;
namespace AH=DragonWilds::ActorHelper;namespace PH=DragonWilds::PropertyHelper;
inline std::string CharacterGuid(UObject* state) {
    if(!state)return {};
    void* data=state;UStruct* type=state->GetClassPrivate();int size=type->GetPropertiesSize();
    for(const auto& field:std::array<std::pair<const TCHAR*,const TCHAR*>,3>{{
        {TEXT("OwnerGuid"),TEXT("/Script/Dominion.DomOwnerGuid")},
        {TEXT("CharacterGuid"),TEXT("/Script/Dominion.DomCharacterGuid")},
        {TEXT("InnerGuid"),TEXT("/Script/CoreUObject.Guid")}}}) {
        FProperty* found=nullptr;for(auto* candidate:TFieldRange<FProperty>(type,EFieldIterationFlags::Default))if(candidate->GetName()==field.first){found=candidate;break;}
        auto* property=CastField<FStructProperty>(found);
        if(!property||!property->GetStruct()||property->GetStruct()->GetPathName()!=field.second||!DragonWilds::ReflectedContract::Fits(property,size))return {};
        data=property->ContainerPtrToValuePtr<void>(data);type=property->GetStruct().Get();size=property->GetElementSize();
    }
    if(size!=16)return {};uint32_t lanes[4]{};std::memcpy(lanes,data,16);
    if(!(lanes[0]|lanes[1]|lanes[2]|lanes[3]))return {};
    return std::format("{:08X}{:08X}{:08X}{:08X}",lanes[0],lanes[1],lanes[2],lanes[3]);
}
inline int PlayerId(UObject* state) {
    if(!state)return -1;
    auto* property=CastField<FIntProperty>(PH::GetPropertyByName(state->GetClassPrivate(),TEXT("PlayerId")));
    if(!property||property->GetElementSize()!=sizeof(int32_t)||!DragonWilds::ReflectedContract::Fits(property,state->GetClassPrivate()->GetPropertiesSize()))return -1;
    return property->GetPropertyValue(property->ContainerPtrToValuePtr<void>(state));
}
// Game-thread, read-only presentation. Does not admit any gameplay operation.
inline nlohmann::json Read(UObject* controller) {
    bool host=false;PlayerStatus::Flags flags;
    try {
        if(!controller)return PlayerStatus::Describe(0,flags,false);
        const auto mode=Network::Detect(controller).Mode;
        if(!Network::OwnsGameplay(mode))return PlayerStatus::Describe(0,flags,false);
        if(mode==Network::Role::ListenServer||mode==Network::Role::Standalone) {
            AH::FunctionCall local(controller,TEXT("/Script/Engine.Controller:IsLocalController"));local.Invoke();host=local.Result<bool>();
        }
        auto* state=AH::GetObjectRef(controller,TEXT("PlayerState"));
        auto* component=state?AH::GetObjectRef(state,TEXT("PrivilegeComponent")):nullptr;
        auto* type=AH::ResolveClass(TEXT("/Script/Dominion.PrivilegeComponent"));
        auto* enumeration=UECustom::UObjectGlobals::StaticFindObject<UEnum*>(nullptr,nullptr,TEXT("/Script/Dominion.EPlayerPrivilege"));
        if(!component||!type||!component->IsA(type)||!enumeration)return PlayerStatus::Describe(0,flags,host);
        for(const auto& pair:enumeration->GetEnumNames()) {
            auto name=RC::to_string(pair.Key.ToString());const auto colon=name.rfind("::");if(colon!=std::string::npos)name.erase(0,colon+2);
            if(pair.Value<=0||pair.Value>255)continue;const auto value=static_cast<uint8_t>(pair.Value);
            if(name=="Admin")flags.Admin=value;else if(name=="Build")flags.Build=value;
            else if(name=="OpenChests")flags.OpenChests=value;else if(name=="Chat")flags.Chat=value;else if(name=="Owner")flags.Owner=value;
        }
        auto* privileges=CastField<FStructProperty>(PH::GetPropertyByName(component->GetClassPrivate(),TEXT("Privileges")));
        if(!privileges||!privileges->GetStruct()||privileges->GetStruct()->GetPathName()!=TEXT("/Script/Dominion.PlayerPrivileges")
            ||!DragonWilds::ReflectedContract::Fits(privileges,component->GetClassPrivate()->GetPropertiesSize()))return PlayerStatus::Describe(0,{},host);
        auto* mask=CastField<FByteProperty>(PH::GetPropertyByName(privileges->GetStruct().Get(),TEXT("PrivilegeMask")));
        if(!mask||mask->GetElementSize()!=1||!DragonWilds::ReflectedContract::Fits(mask,privileges->GetElementSize()))return PlayerStatus::Describe(0,{},host);
        const auto value=mask->GetPropertyValue(mask->ContainerPtrToValuePtr<void>(privileges->ContainerPtrToValuePtr<void>(component)));
        return PlayerStatus::Describe(value,flags,host);
    }catch(...){return PlayerStatus::Describe(0,{},host);}
}
}
