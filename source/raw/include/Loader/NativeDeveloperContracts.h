#pragma once
#include "Loader/NativeUnlockEquipContracts.h"
#include <Unreal/UObjectGlobals.hpp>
#include "DeveloperToolsKey.h"
#include "RuntimePaths.h"
namespace DragonWilds {
inline nlohmann::json InspectNativeDeveloperContracts(){
 using namespace RC;using namespace RC::Unreal;
 auto report=nlohmann::json{{"ReportVersion",1},{"ReadOnly",true},{"InvokedFunctions",0},{"Functions",nlohmann::json::array()},{"Classes",nlohmann::json::array()}};
 const auto matches=[](const std::string& path){return (path.starts_with("/Script/Dominion.")||path.starts_with("/Game/")||path=="/Script/Engine.CheatManager")&&(path.find("Cheat")!=path.npos||path.find("DevMenu")!=path.npos||path.find("DebugMenu")!=path.npos||path.find("Developer")!=path.npos);};
 UObjectGlobals::ForEachUObject([&](UObject* object,int32_t,int32_t)->LoopAction{
  if(!object||object->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed)))return LoopAction::Continue;
  if(object->IsA<UFunction>()){
   auto* function=static_cast<UFunction*>(object);auto* owner=function->GetOuterPrivate();if(!owner||!owner->IsA<UClass>()||!matches(RC::to_string(owner->GetPathName())))return LoopAction::Continue;
   auto fields=nlohmann::json::array();for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::Default))if(field->HasAnyPropertyFlags(CPF_Parm)){
    auto description=nlohmann::json{{"Name",RC::to_string(field->GetName())},{"Type",RC::to_string(field->GetClass().GetName())},{"Flags",static_cast<uint64_t>(field->GetPropertyFlags())},{"Offset",field->GetOffset_Internal()},{"Size",field->GetElementSize()},{"ArrayDim",field->GetArrayDim()},{"Return",field->HasAnyPropertyFlags(CPF_ReturnParm)},{"Out",field->HasAnyPropertyFlags(CPF_OutParm)}};
    if(auto* value=CastField<FStructProperty>(field);value&&value->GetStruct())description["Struct"]=RC::to_string(value->GetStruct()->GetPathName());
    if(auto* value=CastField<FObjectProperty>(field);value&&value->GetPropertyClass())description["ObjectClass"]=RC::to_string(value->GetPropertyClass()->GetPathName());
    fields.push_back(std::move(description));
   }
   report["Functions"].push_back({{"Path",RC::to_string(function->GetPathName())},{"FunctionFlags",static_cast<uint64_t>(function->GetFunctionFlags())},{"Native",function->HasAnyFunctionFlags(FUNC_Native)},{"Networked",function->HasAnyFunctionFlags(FUNC_Net)},{"ParmsSize",function->GetParmsSize()},{"Parameters",std::move(fields)}});
  }else if(object->IsA<UClass>()){
   auto* type=static_cast<UClass*>(object);if(!matches(RC::to_string(type->GetPathName())))return LoopAction::Continue;
   auto fields=nlohmann::json::array();for(auto* field:TFieldRange<FProperty>(type,EFieldIterationFlags::Default))fields.push_back({{"Name",RC::to_string(field->GetName())},{"Type",RC::to_string(field->GetClass().GetName())},{"Offset",field->GetOffset_Internal()},{"Size",field->GetElementSize()}});
   report["Classes"].push_back({{"Path",RC::to_string(type->GetPathName())},{"PropertiesSize",type->GetPropertiesSize()},{"Fields",std::move(fields)}});
  }return LoopAction::Continue;
 });
 report["Reason"]="Loaded native/Blueprint metadata only. Shipping debug implementations and effects require separate verification before execution.";return report;
}
}
