#pragma once
#include "Policy.h"
#include "Core/BuildingNetworkGuard.h"
#include "Core/ScopedCostSwap.h"
#include "NativeMethod.h"
#include "SDK/Helper/ReflectedContract.h"
#include <polyhook2/Detour/x64Detour.hpp>
#include <memory>
#include <unordered_map>
namespace PS::Buildings {
// Move the requirement array only for this synchronous native build call.
// No registry entry, recipe identity or other player's permanent cost is changed.
class MaterialWaiver {
 struct Costs {FArrayProperty* field;UObject* data;FScriptArray saved{};bool waived=false;
  void Swap(){auto* array=field->ContainerPtrToValuePtr<FScriptArray>(data);auto* inner=field->GetInner();FScriptArray temp{};
   temp.MoveAssign(*array,inner->GetElementSize(),inner->GetMinAlignment());array->MoveAssign(saved,inner->GetElementSize(),inner->GetMinAlignment());saved.MoveAssign(temp,inner->GetElementSize(),inner->GetMinAlignment());}
 };
 static inline std::unordered_map<UObject*,std::weak_ptr<Costs>> active;
 UObject* recipe=nullptr;std::shared_ptr<Costs> costs;std::unique_ptr<ScopedCostSwap> scope;
public:
 static bool HasActive(){return !active.empty();}
 bool Allowed()const{return costs&&costs->waived;}
 MaterialWaiver(UWorld* world,Vec3 position,UObject* recipe){
  if(!Live(recipe))return;const auto found=active.find(recipe);if(found!=active.end())costs=found->second.lock();
  const bool waived=PolicyBool(world,L"RuneSchemaFreeBuildingMaterials",position,false);if(!waived&&!costs)return;
  if(!costs){
   auto* field=CastField<FArrayProperty>(Field(recipe->GetClassPrivate(),L"Requirements"));auto* inner=field?field->GetInner():nullptr;
   if(!DragonWilds::ReflectedContract::Fits(field,recipe->GetClassPrivate()->GetPropertiesSize())||field->GetSize()!=sizeof(FScriptArray)||!inner||inner->GetElementSize()<=0||inner->GetMinAlignment()<=0)throw std::runtime_error("Building material array contract differs");
   auto* array=field->ContainerPtrToValuePtr<FScriptArray>(recipe);if(array->Num()<0||array->Num()>1024||(array->Num()&&!array->GetData()))throw std::runtime_error("Building material count differs");
   costs=std::make_shared<Costs>();costs->field=field;costs->data=recipe;active[recipe]=costs;
  }
  this->recipe=recipe;try{scope=std::make_unique<ScopedCostSwap>(costs->waived,waived,[state=costs]{state->Swap();});}catch(...){if(costs.use_count()==1)active.erase(recipe);throw;}
 }
 ~MaterialWaiver(){
  scope.reset();costs.reset();if(recipe){const auto at=active.find(recipe);if(at!=active.end()&&at->second.expired())active.erase(at);}
 }
 MaterialWaiver(const MaterialWaiver&)=delete;
};
class MaterialCosts {
 struct Frame {UObject* owner;UFunction* function;void* params;std::unique_ptr<MaterialWaiver> waiver;};
 std::vector<Frame> frames;
 static inline std::unique_ptr<PLH::x64Detour> menuHook;
 static inline uint64_t menuOriginal=0;
 static inline DWORD menuThread=0;
 static inline MaterialCosts* current=nullptr;
 static std::unique_ptr<MaterialWaiver> MenuScope(UObject* slot){
   if(!Live(slot))return {};Call owner(slot,L"GetOwningPlayer");owner.Invoke();auto* controller=owner.Result();
   if(!Live(controller))return {};Call local(controller,L"IsLocalController");local.Invoke();if(!local.Bool())return {};
   Call pawn(controller,L"K2_GetPawn");pawn.Invoke();auto* player=pawn.Result();
   if(!Live(player)||player->GetWorld()!=slot->GetWorld())return {};Call playerOwner(player,L"GetController");playerOwner.Invoke();if(playerOwner.Result()!=controller)return {};
   auto* property=CastField<FObjectProperty>(Field(slot->GetClassPrivate(),L"ContainedItemDataRow"));
   if(!DragonWilds::ReflectedContract::Fits(property,slot->GetClassPrivate()->GetPropertiesSize())||!property->GetPropertyClass()||property->GetPropertyClass()->GetPathName()!=L"/Script/Dominion.BuildingPieceData")throw std::runtime_error("Building menu data contract changed");
   auto* data=property->GetObjectPropertyValue(property->ContainerPtrToValuePtr<void>(slot));
   Call location(player,L"K2_GetActorLocation");location.Invoke();Check(location.fn,L"ReturnValue",L"StructProperty",sizeof(Vec3));
   auto scope=std::make_unique<MaterialWaiver>(player->GetWorld(),*static_cast<Vec3*>(location.Data(L"ReturnValue")),data);
   if(current&&current->ObserveMenu)current->ObserveMenu(controller,slot,scope->Allowed());return scope;
 }
 static void Menu(UObject* slot){
  std::unique_ptr<MaterialWaiver> waiver;
  if(GetCurrentThreadId()==menuThread&&(!providers.empty()||MaterialWaiver::HasActive()))try{waiver=MenuScope(slot);}catch(const std::exception&){}
  reinterpret_cast<void(*)(UObject*)>(menuOriginal)(slot);
 }
 static UObject* ByIndex(int index){
  std::vector<UObject*> data;UObjectGlobals::FindAllOf(L"BuildingPieceData",data);
  for(auto* item:data)if(Live(item)&&*static_cast<int*>(Value(item->GetClassPrivate(),item,L"BuildingPieceDataIndex"))==index)return item;
  return nullptr;
 }
public:
 std::function<void(UObject*,UObject*,bool)> ObserveMenu;
 void InstallNative(){
  if(menuHook)return;auto* fn=Find<UFunction>(L"/Script/Dominion.BuildingUISlotBase:SetBuildableOpacity");
  if(!fn->HasAnyFunctionFlags(FUNC_Native)||fn->HasAnyFunctionFlags(FUNC_Static)||DragonWilds::ReflectedContract::Count(fn)!=0)throw std::runtime_error("Building menu native entry changed");
  auto hook=std::make_unique<PLH::x64Detour>(NativeMethod(fn,0),reinterpret_cast<uint64_t>(&Menu),&menuOriginal);
  menuThread=GetCurrentThreadId();current=this;if(!hook->hook()){current=nullptr;throw std::runtime_error("Building menu hook was refused");}menuHook=std::move(hook);
 }
 void Before(UObject* object,UFunction* fn,void* frame){
  if(!frame||!Live(object)||frames.size()>=64)throw std::runtime_error("Building material call frame is unavailable or nested too deeply");
  const auto path=fn->GetPathName();UObject* data=nullptr;Vec3 position{};
  if(path==L"/Script/Dominion.BuildModeComponent:Server_SpawnBuilding"){
   auto* transform=CastField<FStructProperty>(Field(fn,L"SpawnTransform"));if(!transform||transform->GetSize()!=sizeof(Transform))throw std::runtime_error("Building material spawn transform differs");position=transform->ContainerPtrToValuePtr<Transform>(frame)->location;
   if(!MaterialWaiver::HasActive()&&!PolicyBool(object->GetWorld(),L"RuneSchemaFreeBuildingMaterials",position))return;
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
  if(Live(data))frames.push_back({object,fn,frame,std::make_unique<MaterialWaiver>(object->GetWorld(),position,data)});
 }
 void After(UObject* object,UFunction* fn,void* frame){for(size_t i=frames.size();i;--i)if(frames[i-1].owner==object&&frames[i-1].function==fn&&frames[i-1].params==frame){frames.erase(frames.begin()+i-1);return;}}
 void Clear(){while(!frames.empty())frames.pop_back();if(menuHook){menuHook->unHook();menuHook.reset();}menuOriginal=0;menuThread=0;current=nullptr;}
};
}
