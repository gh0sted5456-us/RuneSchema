#pragma once
#include "Core/PlayerStatus.h"
#include "Runtime/NetworkContext.h"
#include "Runtime/Storefront.h"
#include "SDK/Helper/ReflectedContract.h"
#include <Unreal/Property/FStructProperty.hpp>
#include <Unreal/Property/FStrProperty.hpp>
#include <Unreal/UEnum.hpp>
#include <cstring>
#include <format>
#include <cmath>
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
inline void Color(UObject* state,nlohmann::json& row) {
    try {
        if(!state)return;
        auto* data=CastField<FStructProperty>(PH::GetPropertyByName(state->GetClassPrivate(),TEXT("PlayerColor")));
        if(!data||!data->GetStruct()||data->GetStruct()->GetPathName()!=TEXT("/Script/Dominion.PlayerColor")
            ||!DragonWilds::ReflectedContract::Fits(data,state->GetClassPrivate()->GetPropertiesSize()))return;
        auto* color=CastField<FStructProperty>(PH::GetPropertyByName(data->GetStruct().Get(),TEXT("Color")));
        if(!color||!color->GetStruct()||color->GetStruct()->GetPathName()!=TEXT("/Script/CoreUObject.LinearColor")||color->GetElementSize()!=sizeof(float)*4
            ||!DragonWilds::ReflectedContract::Fits(color,data->GetElementSize()))return;
        std::array<float,4> rgba{};std::memcpy(rgba.data(),color->ContainerPtrToValuePtr<void>(data->ContainerPtrToValuePtr<void>(state)),sizeof(rgba));
        if(rgba[3]<=0||std::any_of(rgba.begin(),rgba.end(),[](float v){return !std::isfinite(v)||v<0||v>1;}))return;
        row["NativePlayerColor"]=rgba;row["NativePlayerColorVerified"]=true;
    }catch(...){}
}
inline std::string Platform(UObject* state) {
    try {
        if(!state)return "Unknown";
        auto* data=CastField<FStructProperty>(PH::GetPropertyByName(state->GetClassPrivate(),TEXT("PlatformData")));
        if(!data||!data->GetStruct()||data->GetStruct()->GetPathName()!=TEXT("/Script/Dominion.DomPlatformData")
            ||!DragonWilds::ReflectedContract::Fits(data,state->GetClassPrivate()->GetPropertiesSize()))return "Unknown";
        auto* name=CastField<FStrProperty>(PH::GetPropertyByName(data->GetStruct().Get(),TEXT("PlatformName")));
        if(!name||name->GetElementSize()!=sizeof(FString)||!DragonWilds::ReflectedContract::Fits(name,data->GetElementSize()))return "Unknown";
        const auto& chars=name->ContainerPtrToValuePtr<FString>(data->ContainerPtrToValuePtr<void>(state))->GetCharArray();
        if(chars.Num()<2||chars.Num()>65||!chars.GetData()||chars.GetData()[chars.Num()-1]!=0)return "Unknown";
        auto value=RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1));for(auto& ch:value)if(ch>='A'&&ch<='Z')ch+=32;
        if(value=="steam"||value=="cpsteam")return "Steam";
        if(value=="wingdk"||value=="gdk"||value=="cpgdk")return "Game Pass";
        if(value=="epic"||value=="epicgames"||value=="cpepic")return "Epic";
        if(value=="ps5"||value=="cpps5")return "PS5";
        if(value=="xbx"||value=="xbox"||value=="cpxbox")return "Xbox";
        if(value=="switch2"||value=="cpnintendo")return "Switch 2";
        if(value=="jagex")return "Jagex";
    }catch(...){}return "Unknown";
}
// Game-thread, read-only presentation. Does not admit any gameplay operation.
inline std::string Platform(UObject* state,bool local) {
    return Storefront::PlayerPlatform(Platform(state),local,Storefront::CurrentDetection());
}
inline std::string ControllerPlatform(UObject* controller) {
    if(!controller)return "Unknown";
    bool local=false;
    try {AH::FunctionCall query(controller,TEXT("/Script/Engine.Controller:IsLocalController"));query.Invoke();local=query.Result<bool>();}catch(...){}
    return Platform(AH::GetObjectRef(controller,TEXT("PlayerState")),local);
}
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
