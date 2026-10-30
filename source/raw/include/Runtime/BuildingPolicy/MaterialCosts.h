#pragma once
#include "Policy.h"
#include "Core/BuildingNetworkGuard.h"
#include <memory>
namespace PS::Buildings {
// Move the requirement array only for this synchronous native build call.
// No registry entry, recipe identity or other player's permanent cost is changed.
class MaterialWaiver {
 FArrayProperty* field=nullptr;UObject* data=nullptr;FScriptArray saved{};
public:
 MaterialWaiver(UWorld* world,Vec3 position,UObject* recipe){
  if(!Live(recipe)||!PolicyBool(world,L"RuneSchemaFreeBuildingMaterials",position))return;
  field=CastField<FArrayProperty>(Field(recipe->GetClassPrivate(),L"Requirements"));if(!field)throw std::runtime_error("Building material array contract differs");
  auto* array=field->ContainerPtrToValuePtr<FScriptArray>(recipe);if(array->Num()<0||array->Num()>1024)throw std::runtime_error("Building material count differs");
  data=recipe;saved.MoveAssign(*array,field->GetInner()->GetElementSize(),field->GetInner()->GetMinAlignment());
 }
 ~MaterialWaiver(){
  if(!field)return;
  auto* array=field->ContainerPtrToValuePtr<FScriptArray>(data);field->DestroyValue(array);field->InitializeValue(array);array->MoveAssign(saved,field->GetInner()->GetElementSize(),field->GetInner()->GetMinAlignment());
 }
 MaterialWaiver(const MaterialWaiver&)=delete;
};
class MaterialCosts {
 struct Frame {UObject* owner;UFunction* function;void* params;std::unique_ptr<MaterialWaiver> waiver;};
 std::vector<Frame> frames;
 static UObject* ByIndex(int index){
  std::vector<UObject*> data;UObjectGlobals::FindAllOf(L"BuildingPieceData",data);
  for(auto* item:data)if(Live(item)&&*static_cast<int*>(Value(item->GetClassPrivate(),item,L"BuildingPieceDataIndex"))==index)return item;
  return nullptr;
 }
public:
 void Before(UObject* object,UFunction* fn,void* frame){
  if(!frame||!Live(object)||frames.size()>=64)throw std::runtime_error("Building material call frame is unavailable or nested too deeply");
  const auto path=fn->GetPathName();UObject* data=nullptr;Vec3 position{};
  if(path==L"/Script/Dominion.BuildModeComponent:Server_SpawnBuilding"){
   auto* transform=CastField<FStructProperty>(Field(fn,L"SpawnTransform"));if(!transform||transform->GetSize()!=sizeof(Transform))throw std::runtime_error("Building material spawn transform differs");position=transform->ContainerPtrToValuePtr<Transform>(frame)->location;
   if(!PolicyBool(object->GetWorld(),L"RuneSchemaFreeBuildingMaterials",position))return;
   FProperty* first=nullptr;
   for(auto* p:TFieldRange<FProperty>(fn,EFieldIterationFlags::Default))if(p->HasAnyPropertyFlags(CPF_Parm)&&!p->HasAnyPropertyFlags(CPF_ReturnParm)&&(!first||p->GetOffset_Internal()<first->GetOffset_Internal()))first=p;
   auto* number=first?CastField<FNumericProperty>(first):nullptr;
   if(!number||!BuildingNetworkGuard::PlacementIndexContract(RC::to_string(first->GetClass().GetName()),first->GetOffset_Internal(),first->GetSize(),fn->GetParmsSize()))throw std::runtime_error("Building material index contract differs");
   const auto index=number->GetSignedIntPropertyValue(first->ContainerPtrToValuePtr<void>(frame));
   if(index<0||index>65535)throw std::runtime_error("Building material index is out of range");data=ByIndex(static_cast<int>(index));
  }else if(path==L"/Script/Dominion.BuildModeComponent:TrySpawnPieceAtLocation"){
   Check(fn,L"InLocation",L"StructProperty",sizeof(Vec3));position=*Field(fn,L"InLocation")->ContainerPtrToValuePtr<Vec3>(frame);
   auto* property=CastField<FObjectPropertyBase>(Field(fn,L"BuildingPieceData"));if(!property)throw std::runtime_error("Building material recipe argument differs");data=property->GetObjectPropertyValue(property->ContainerPtrToValuePtr<void>(frame));
  }else if(path==L"/Script/Dominion.BuildModeComponent:OnPieceHovered"||path==L"/Script/Dominion.BuildModeComponent:OnPieceSelected"){
   auto* player=Object(object,L"PlayerCharacter");if(!Live(player))return;Call location(player,L"K2_GetActorLocation");location.Invoke();Check(location.fn,L"ReturnValue",L"StructProperty",sizeof(Vec3));position=*static_cast<Vec3*>(location.Data(L"ReturnValue"));
   auto* property=CastField<FObjectPropertyBase>(Field(fn,L"SelectedPieceData"));if(!property)throw std::runtime_error("Building material selection argument differs");data=property->GetObjectPropertyValue(property->ContainerPtrToValuePtr<void>(frame));
  }else return;
  if(Live(data)&&PolicyBool(object->GetWorld(),L"RuneSchemaFreeBuildingMaterials",position))frames.push_back({object,fn,frame,std::make_unique<MaterialWaiver>(object->GetWorld(),position,data)});
 }
 void After(UObject* object,UFunction* fn,void* frame){for(size_t i=frames.size();i;--i)if(frames[i-1].owner==object&&frames[i-1].function==fn&&frames[i-1].params==frame){frames.erase(frames.begin()+i-1);return;}}
 void Clear(){while(!frames.empty())frames.pop_back();}
};
}
