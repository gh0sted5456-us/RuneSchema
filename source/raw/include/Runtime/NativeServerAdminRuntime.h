#pragma once
#include "../../../shared/NativeServerAdmin.h"
#include "SDK/Helper/PlayerStatus.h"
#include "SDK/WeakObjectHandle.h"
#include "Runtime/HelpyPlayerAccess.h"
#include <Unreal/Property/FEnumProperty.hpp>

namespace PS::NativeServerAdminRuntime {
using namespace RC::Unreal;
namespace AH=DragonWilds::ActorHelper;
namespace RCt=DragonWilds::ReflectedContract;
using Json=nlohmann::json;
inline std::string Token(UObject* value){const PS::WeakObjectHandle key(value);return std::to_string(key.ObjectIndex)+":"+std::to_string(key.ObjectSerialNumber);}
inline bool Owner(UObject* controller){
    auto* state=controller?AH::GetObjectRef(controller,TEXT("PlayerState")):nullptr;
    auto* component=state?AH::GetObjectRef(state,TEXT("PrivilegeComponent")):nullptr;
    auto* type=AH::ResolveClass(TEXT("/Script/Dominion.PrivilegeComponent"));
    auto* function=component?component->GetFunctionByNameInChain(TEXT("IsOwner")):nullptr;
    auto* result=function?CastField<FBoolProperty>(function->GetReturnProperty()):nullptr;
    if(!component||!type||!component->IsA(type)||!function||function->GetPathName()!=TEXT("/Script/Dominion.PrivilegeComponent:IsOwner")
        ||RCt::Count(function)!=1||!RCt::Output(result,function)||!result->IsNativeBool()||result->GetElementSize()!=sizeof(bool))throw std::runtime_error("Native owner verification is unavailable.");
    AH::FunctionCall call(component,function);call.Invoke();return call.Result<bool>();
}
struct Contract {UFunction* Function;FStructProperty* Identity;uint8_t Admin;};
inline bool HelpyAuthorized(UObject* controller,const std::string& guid){
    if(HelpyPlayerAccess::Allowed(guid))return true;
    try{return Owner(controller);}catch(...){return false;}
}
inline Contract Resolve(UObject* controller,bool grant){
    const auto* name=grant?TEXT("Server_GrantPrivileges"):TEXT("Server_RevokePrivileges");
    auto* function=controller?controller->GetFunctionByNameInChain(name):nullptr;
    const auto path=std::wstring(TEXT("/Script/Dominion.DominionPlayerController:"))+name;
    if(!function||function->GetPathName()!=path||!function->HasAnyFunctionFlags(FUNC_Native)||!function->HasAnyFunctionFlags(FUNC_Net)||!function->HasAnyFunctionFlags(FUNC_NetServer)
        ||!function->HasAnyFunctionFlags(FUNC_NetReliable)||RCt::Count(function)!=2||function->GetReturnProperty())throw std::runtime_error("The native grant/revoke contract is unavailable in this game build.");
    auto* identity=CastField<FStructProperty>(function->FindProperty(FName(TEXT("TargetNedId"),FNAME_Find)));
    auto* privilege=function->FindProperty(FName(TEXT("Privileges"),FNAME_Find));
    UEnum* enumeration=nullptr;
    if(auto* field=CastField<FEnumProperty>(privilege);field&&field->GetUnderlyingProperty()&&field->GetUnderlyingProperty()->GetElementSize()==1)enumeration=field->GetEnum();
    else if(auto* field=CastField<FByteProperty>(privilege))enumeration=field->GetEnum().Get();
    if(!RCt::Input(identity,function)||!identity->GetStruct()||identity->GetStruct()->GetPathName()!=TEXT("/Script/Engine.UniqueNetIdRepl")
        ||!RCt::Input(privilege,function)||privilege->GetElementSize()!=1||!enumeration||enumeration->GetPathName()!=TEXT("/Script/Dominion.EPlayerPrivilege"))throw std::runtime_error("Native admin parameter types do not match the supported contract.");
    uint8_t admin=0;for(const auto& entry:enumeration->GetEnumNames())if(entry.Key.ToString()==TEXT("EPlayerPrivilege::Admin")&&entry.Value>0&&entry.Value<=255)admin=uint8_t(entry.Value);
    if(!admin||(admin&(admin-1)))throw std::runtime_error("Native Admin privilege is not a single verified flag.");
    return {function,identity,admin};
}
inline std::vector<UObject*> Controllers(UObject* world){
    auto* type=AH::ResolveClass(TEXT("/Script/Dominion.DominionPlayerController"));std::vector<UObject*> result;
    if(!type)return result;TArray<UObject*> objects;UECustom::UObjectGlobals::GetObjectsOfClass(type,objects,true);
    for(auto* controller:objects)if(controller&&controller->GetWorld()==world&&!controller->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed))){
        auto* pawn=AH::GetObjectRef(controller,TEXT("Pawn"));if(pawn&&pawn->GetWorld()==world&&AH::GetObjectRef(pawn,TEXT("Controller"))==controller)result.push_back(controller);
        if(result.size()>256)throw std::runtime_error("Connected player roster exceeds the administration limit.");
    }return result;
}
inline Json Read(UObject* caller,UObject* world){
    Json rows=Json::array();bool canManage=false,canAuthorize=!caller;std::string reason;
    if(caller)try{canAuthorize=caller->GetWorld()==world&&Owner(caller);}catch(...){}
    try{if(!caller)reason="Local server Helpy can authorize RuneSchema access. Native Admin changes use the owner's connected client.";else{(void)Resolve(caller,true);(void)Resolve(caller,false);canManage=Owner(caller);if(!canManage)reason="Only the native server owner can change admin rights.";}}
    catch(const std::exception& error){reason=error.what();}
    for(auto* controller:Controllers(world)){
        auto* state=AH::GetObjectRef(controller,TEXT("PlayerState"));const auto guid=PlayerStatusRuntime::CharacterGuid(state);if(guid.empty())continue;
        std::string name="Player";AH::FunctionCall getName(state,TEXT("/Script/Engine.PlayerState:GetPlayerName"));getName.Invoke();const auto native=getName.Result<FString>();const auto& chars=native.GetCharArray();
        if(chars.Num()>1&&chars.Num()<=257&&chars.GetData())name=RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1));
        auto role=PlayerStatusRuntime::Read(controller);bool owner=false;try{owner=Owner(controller);}catch(...){role=PlayerStatus::Describe(0,{},false);}
        if(owner&&role.value("NativeKnown",false))role["Role"]="owner";
        const auto identity=HelpyPlayerAccess::Identity(controller);
        rows.push_back({{"Name",name},{"Guid",guid},{"TargetToken",Token(controller)},{"RoleStatus",role},{"Owner",owner},
            {"Authorized",owner||HelpyPlayerAccess::Allowed(guid)},{"PlatformAccountName",identity.accountName},{"Platform",identity.platform},{"PlayerId",identity.playerId}});
    }
    return {{"Kind","RuneSchemaServerControl1"},{"WorldToken",Token(world)},{"CanManage",canManage},{"CanAuthorize",canAuthorize},
        {"AutoAuthorize",PSConfig::Get()->LiveHelpyAuthority().autoAuthorizeConnectingPlayers},{"Reason",reason},{"Players",rows}};
}
inline Json Handle(UObject* caller,UObject* world,const Json& request){
    const auto mode=world?Network::Detect(world).Mode:Network::Role::Unknown;
    if(!world||mode==Network::Role::Client||mode==Network::Role::Unknown)throw std::runtime_error("Native administration executes only on the authoritative server.");
    const auto operation=NativeServerAdmin::Operation(request);
    if(operation=="Read")return {{"ServerControl",Read(caller,world)},{"Status","Read native server roles. No rights changed."}};
    const bool access=operation=="Authorize"||operation=="Unauthorize"||operation=="AuthorizeAll"||operation=="AutoAuthorize";
    if(access){
        if(caller&&(caller->GetWorld()!=world||!Owner(caller)))throw std::runtime_error("Player authorization requires local server Helpy or the native owner.");
        if(request.at("WorldToken")!=Token(world))throw std::runtime_error("The world changed. Refresh connected players.");
        std::vector<UObject*> targets;
        if(operation=="AuthorizeAll"||(operation=="AutoAuthorize"&&request.at("Enabled").get<bool>()))targets=Controllers(world);
        else if(operation!="AutoAuthorize"){
            const auto guid=NativeServerAdmin::Guid(request.at("PlayerGuid").get<std::string>());
            for(auto* controller:Controllers(world))if(HelpyPlayerAuthorization::Guid(HelpyPlayerAccess::Identity(controller).characterGuid)==guid)targets.push_back(controller);
            if(targets.size()!=1||request.at("TargetToken")!=Token(targets.front()))throw std::runtime_error("The player connection changed. Refresh before authorizing.");
        }
        auto* config=PSConfig::Get();const auto& before=config->LiveHelpyAuthority();auto proposed=before;
        if(operation=="AutoAuthorize")proposed.autoAuthorizeConnectingPlayers=request.at("Enabled").get<bool>();
        for(auto* target:targets){const auto identity=HelpyPlayerAccess::Identity(target);
            if(operation=="Unauthorize")std::erase_if(proposed.authorizedPlayers,[&](const auto& row){return HelpyPlayerAuthorization::Guid(row.characterGuid)==HelpyPlayerAuthorization::Guid(identity.characterGuid);});
            else HelpyPlayerAuthorization::Remember(proposed.authorizedPlayers,identity);
        }
        if(!config->SaveHelpyPlayerAccess(before,proposed))throw std::runtime_error("Player authorization was not saved. Refresh before retrying.");
        return {{"ServerControl",Read(caller,world)},{"Status","RuneSchema access saved and active. Native game roles are unchanged."}};
    }
    const auto wantedGuid=NativeServerAdmin::Guid(request.at("PlayerGuid").get<std::string>());
    UObject* target=nullptr;
    for(auto* controller:Controllers(world)){
        const auto guid=PlayerStatusRuntime::CharacterGuid(AH::GetObjectRef(controller,TEXT("PlayerState")));
        if(!guid.empty()&&NativeServerAdmin::Guid(guid)==wantedGuid){if(target)throw std::runtime_error("Player identity is ambiguous; no admin change was made.");target=controller;}
    }
    if(!target)throw std::runtime_error("The target player is no longer connected.");
    NativeServerAdmin::Authorize(caller&&caller->GetWorld()==world&&Owner(caller),Owner(target),request.at("WorldToken")==Token(world),request.at("TargetToken")==Token(target));
    auto role=PlayerStatusRuntime::Read(target);if(!role.value("NativeKnown",false))throw std::runtime_error("The target's native privileges could not be verified.");
    const bool grant=operation=="Grant",already=role.value("Role",std::string{})=="admin";
    if(already==grant)return {{"ServerControl",Read(caller,world)},{"Status",grant?"Player is already an admin; no native call was made.":"Player is already a regular player; no native call was made."}};
    const auto contract=Resolve(caller,grant);auto* state=AH::GetObjectRef(target,TEXT("PlayerState"));
    auto* identity=CastField<FStructProperty>(DragonWilds::PropertyHelper::GetPropertyByName(state->GetClassPrivate(),TEXT("UniqueId")));
    if(!identity)identity=CastField<FStructProperty>(DragonWilds::PropertyHelper::GetPropertyByName(state->GetClassPrivate(),TEXT("UniqueID")));
    if(!identity||!RCt::Fits(identity,state->GetClassPrivate()->GetPropertiesSize())||!identity->GetStruct()||identity->GetStruct()->GetPathName()!=TEXT("/Script/Engine.UniqueNetIdRepl")
        ||identity->GetElementSize()!=contract.Identity->GetElementSize())throw std::runtime_error("The current target's native account identity is unavailable.");
    const PS::WeakObjectHandle targetIdentity(target);
    AH::FunctionCall call(caller,contract.Function);call.StructArg(TEXT("TargetNedId"),identity,identity->ContainerPtrToValuePtr<void>(state)).Arg(TEXT("Privileges"),contract.Admin).Invoke();
    target=targetIdentity.Get();bool confirmed=false;
    if(target&&target->GetWorld()==world){role=PlayerStatusRuntime::Read(target);confirmed=role.value("NativeKnown",false)&&((role.value("Role",std::string{})=="admin")==grant);}
    auto snapshot=Read(caller,world);snapshot["Change"]={{"PlayerGuid",wantedGuid},{"Operation",operation},{"Confirmed",confirmed},{"Persistence","Managed by the game; restart persistence has not been verified."}};
    return {{"ServerControl",snapshot},{"Status",confirmed?"Native role change confirmed. The game manages persistence.":"Native call submitted; role change is unconfirmed. Refresh before retrying."}};
}
}
