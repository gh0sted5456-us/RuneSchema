#pragma once
#include "Core/ConsoleCommand.h"
#include "Runtime/Storefront.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/ReflectedContract.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "Unreal/ULocalPlayer.hpp"
#include "Unreal/UnrealInitializer.hpp"
#include "Unreal/Property/FEnumProperty.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"

namespace PS::Console {
inline nlohmann::json CloseConsole(RC::Unreal::ULocalPlayer* player){
    using namespace RC::Unreal;
    namespace AH=DragonWilds::ActorHelper;namespace C=DragonWilds::ReflectedContract;
    if(!IsInGameThread()||Storefront::IsDedicatedServer())throw std::runtime_error("Closing the console requires the local game thread");
    const auto reference=[](UObject* owner,const TCHAR* name){
        if(!owner||owner->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))throw std::runtime_error("Console owner is unavailable");
        auto* field=CastField<FObjectPropertyBase>(owner->GetClassPrivate()->GetPropertyByNameInChain(name));
        if(!C::Fits(field,owner->GetClassPrivate()->GetPropertiesSize())||field->GetElementSize()!=sizeof(UObject*))throw std::runtime_error("Console object layout changed");
        auto* result=AH::GetObjectRef(owner,name);
        if(!result||result->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))throw std::runtime_error("Console object is unavailable");return result;
    };
    auto* controller=reference(player,TEXT("PlayerController"));
    AH::FunctionCall local(controller,TEXT("/Script/Engine.Controller:IsLocalController"));local.Invoke();
    if(!local.Result<bool>()||reference(controller,TEXT("Player"))!=player)throw std::runtime_error("Console controller is not the requesting local player");
    auto* viewport=reference(player,TEXT("ViewportClient"));auto* console=reference(viewport,TEXT("ViewportConsole"));
    auto* consoleType=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Engine.Console"),false);
    if(!consoleType||!console->IsA(consoleType)||console->GetOuterPrivate()!=viewport)throw std::runtime_error("Native viewport console ownership changed");
    auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,TEXT("/Script/Engine.PlayerController:ConsoleKey"),false);C::Frame(function);
    auto* owner=function->GetOuterPrivate();
    if(!owner||!owner->IsA<UClass>()||!controller->IsA(static_cast<UClass*>(owner))||!function->HasAnyFunctionFlags(FUNC_Native)
        ||!function->HasAnyFunctionFlags(FUNC_Exec)||function->HasAnyFunctionFlags(FUNC_Net))throw std::runtime_error("Native ConsoleKey owner/flags changed");
    auto* key=CastField<FStructProperty>(function->FindProperty(FName(TEXT("Key"),FNAME_Find)));
    auto* type=key?key->GetStruct().Get():nullptr;
    auto* keyName=type?CastField<FNameProperty>(type->GetPropertyByNameInChain(TEXT("KeyName"))):nullptr;
    if(C::Count(function)!=1||!C::Input(key,function)||!type||type->GetPathName()!=TEXT("/Script/InputCore.Key")
        ||key->GetElementSize()!=type->GetPropertiesSize()||!C::Fits(keyName,type->GetPropertiesSize()))throw std::runtime_error("Native ConsoleKey parameter contract changed");
    auto* settings=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,TEXT("/Script/Engine.Default__InputSettings"),false);
    if(!settings)throw std::runtime_error("Native console key settings unavailable");
    auto* keys=CastField<FArrayProperty>(settings->GetClassPrivate()->GetPropertyByNameInChain(TEXT("ConsoleKeys")));
    auto* inner=keys?CastField<FStructProperty>(keys->GetInner()):nullptr;
    if(!C::Fits(keys,settings->GetClassPrivate()->GetPropertiesSize())||!inner||inner->GetStruct().Get()!=type)throw std::runtime_error("Native console key array layout changed");
    FScriptArrayHelper values(keys,keys->ContainerPtrToValuePtr<void>(settings));std::string configured;
    if(values.Num()<1||values.Num()>32)throw std::runtime_error("Native console key list is unavailable");
    for(int i=0;i<values.Num();++i){const auto name=keyName->ContainerPtrToValuePtr<FName>(values.GetRawPtr(i))->ToString();if(!name.empty()&&name!=TEXT("None")){configured=RC::to_string(name);break;}}
    if(configured.empty())throw std::runtime_error("A native console key must be configured");
    // Two native cycles leave None or Typing; UConsole closes Typing when this command returns.
    for(int cycle=0;cycle<2;++cycle){AH::FunctionCall close(controller,function);close.JsonArg(TEXT("Key"),{{"KeyName",configured}}).Invoke();}
    return {{"Requested",true},{"Status","Native console close requested for command return; game menu and pause state unchanged."}};
}
// The game's input router owns focus, coupled panels, animations and pause guards.
inline nlohmann::json RequestMenu(RC::Unreal::ULocalPlayer* player,Command command,MenuAction action){
    using namespace RC::Unreal;
    namespace AH=DragonWilds::ActorHelper;namespace C=DragonWilds::ReflectedContract;
    if(!IsInGameThread())throw std::runtime_error("Native menus must be requested on the game thread");
    const auto alive=[](UObject* object){return object&&!object->HasAnyFlags(static_cast<EObjectFlags>(
        RF_ClassDefaultObject|RF_ArchetypeObject|RF_NeedLoad|RF_NeedPostLoad|RF_NeedInitialization|RF_BeginDestroyed|RF_FinishDestroyed));};
    if(Storefront::IsDedicatedServer()||!alive(player))throw std::runtime_error("A local in-world player is required");
    const auto* widgetName=MenuWidgetName(command);if(!widgetName)throw std::runtime_error("Menu is not allowlisted");
    const auto object=[&](UObject* owner,const TCHAR* name){
        if(!alive(owner))throw std::runtime_error("Menu owner is unavailable");
        auto* field=CastField<FObjectPropertyBase>(owner->GetClassPrivate()->GetPropertyByNameInChain(name));
        if(!field||!C::Fits(field,owner->GetClassPrivate()->GetPropertiesSize())||field->GetElementSize()!=sizeof(UObject*))
            throw std::runtime_error("Menu object property layout changed");
        auto* result=AH::GetObjectRef(owner,name);
        if(!alive(result))throw std::runtime_error("Menu object is unavailable");return result;
    };
    auto* controller=object(player,TEXT("PlayerController"));
    AH::FunctionCall local(controller,TEXT("/Script/Engine.Controller:IsLocalController"));local.Invoke();
    if(!local.Result<bool>()||!controller->GetWorld())throw std::runtime_error("The console does not belong to a local world player");
    if(object(controller,TEXT("Pawn"))->GetWorld()!=controller->GetWorld())throw std::runtime_error("The local character is changing worlds");
    auto* hud=object(controller,TEXT("MyHUD"));
    if(hud->GetWorld()!=controller->GetWorld())throw std::runtime_error("HUD belongs to a different world");
    auto* menu=object(hud,TEXT("MainMenuWidget"));
    AH::FunctionCall owner(menu,TEXT("/Script/UMG.Widget:GetOwningPlayer"));owner.Invoke();
    if(owner.Result<UObject*>()!=controller)throw std::runtime_error("Menu belongs to a different player");
    auto* subsystemType=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.HUDUISubsystem"),false);
    if(!subsystemType)throw std::runtime_error("Native HUD subsystem type is unavailable");
    TArray<UObject*> subsystems;UECustom::UObjectGlobals::GetObjectsOfClass(subsystemType,subsystems,true);
    if(subsystems.Num()<0||subsystems.Num()>64)throw std::runtime_error("Native HUD subsystem roster exceeds its bound");
    UObject* subsystem=nullptr;
    for(auto* candidate:subsystems)if(alive(candidate)&&(candidate->GetWorld()==controller->GetWorld()||(!candidate->GetWorld()&&candidate->GetOuterPrivate()==controller->GetWorld()))){
        if(subsystem)throw std::runtime_error("Multiple HUD subsystems match the local world");subsystem=candidate;
    }
    auto* api=object(subsystem,TEXT("InputManagerAPI"));
    if(api->GetWorld()&&api->GetWorld()!=controller->GetWorld())throw std::runtime_error("Native input router belongs to a different world");
    const auto* method=action==MenuAction::Open?TEXT("OpenWidgetIfClosed"):action==MenuAction::Close?TEXT("CloseWidgetIfOpen"):TEXT("ToggleWidget");
    const auto path=RC::StringType(TEXT("/Script/Dominion.InputManagerUIAPI:"))+method;
    auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path.c_str(),false);C::Frame(function);
    auto* type=function->GetOuterPrivate();
    if(!type||!type->IsA<UClass>()||!api->IsA(static_cast<UClass*>(type))
        ||!function->HasAnyFunctionFlags(FUNC_Native)||function->HasAnyFunctionFlags(FUNC_Static)||function->HasAnyFunctionFlags(FUNC_Net))throw std::runtime_error("Native input-router owner/flags changed");
    const auto field=[&](const TCHAR* name){return function->FindProperty(FName(name,FNAME_Find));};
    auto* playerField=CastField<FObjectPropertyBase>(field(TEXT("PlayerController")));
    auto* widget=field(TEXT("Type"));auto* outside=CastField<FBoolProperty>(field(TEXT("bIsTriggeredByOutsideWidget")));
    auto* pause=CastField<FBoolProperty>(field(TEXT("bShouldCheckPauseBlock")));
    auto* result=CastField<FBoolProperty>(function->GetReturnProperty());const bool toggle=action==MenuAction::Toggle;
    if(C::Count(function)!=(toggle?4:5)||!C::Input(playerField,function)||playerField->GetElementSize()!=sizeof(UObject*)
        ||!playerField->GetPropertyClass().Get()||!controller->IsA(playerField->GetPropertyClass().Get())
        ||!C::Input(widget,function)||widget->GetElementSize()!=1||!C::Input(outside,function)||!C::Input(pause,function)
        ||(toggle?function->GetReturnProperty()!=nullptr:!C::Output(result,function)))throw std::runtime_error("Native input-router parameter contract changed");
    UEnum* enumeration=nullptr;
    if(auto* p=CastField<FEnumProperty>(widget);p&&p->GetUnderlyingProperty())enumeration=p->GetEnum();
    else if(auto* p=CastField<FByteProperty>(widget))enumeration=p->GetEnum().Get();
    if(!enumeration||enumeration->GetPathName()!=TEXT("/Script/Dominion.EWidgetType"))throw std::runtime_error("Native widget-type enum changed");
    int64 ordinal=-1;const auto full=RC::StringType(TEXT("EWidgetType::"))+widgetName;
    for(const auto& entry:enumeration->GetEnumNames())if(entry.Key.ToString()==full||entry.Key.ToString()==widgetName)ordinal=entry.Value;
    if(ordinal<=0||ordinal>255)throw std::runtime_error("Requested native widget type is unavailable");
    AH::FunctionCall request(api,function);
    request.Arg(TEXT("PlayerController"),controller).Arg(TEXT("Type"),static_cast<uint8_t>(ordinal))
        .Arg(TEXT("bIsTriggeredByOutsideWidget"),true).Arg(TEXT("bShouldCheckPauseBlock"),true).Invoke();
    nlohmann::json reply={{"Requested",true},{"Menu",RC::to_string(RC::StringType(widgetName))},
        {"Operation",action==MenuAction::Open?"open":action==MenuAction::Close?"close":"toggle"},
        {"Status","Native menu request dispatched with game guards enabled. Close the console to view the result; no exit action or save edit was invoked."}};
    if(!toggle)reply["NativeResult"]=request.Result<bool>();return reply;
}
}
