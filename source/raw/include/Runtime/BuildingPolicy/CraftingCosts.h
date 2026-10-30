#pragma once
#include "Policy.h"
#include "Core/ScopedCostSwap.h"
#include "SDK/Helper/ReflectedContract.h"
#include <Windows.h>
#include <atomic>
#include <exception>
#include <memory>
#include <unordered_map>

namespace PS::Buildings {
class CraftingCosts {
 struct Costs {
  UObject* recipe;FArrayProperty* field;FScriptArray saved{};bool waived=false;
  void Swap(){
   auto* live=field->ContainerPtrToValuePtr<FScriptArray>(recipe);auto* inner=field->GetInner();
   FScriptArray temp{};const auto size=inner->GetElementSize();const auto alignment=inner->GetMinAlignment();
   temp.MoveAssign(*live,size,alignment);live->MoveAssign(saved,size,alignment);saved.MoveAssign(temp,size,alignment);
  }
 };
 struct Frame {UObject* owner;UFunction* function;void* params;UObject* recipe;std::shared_ptr<Costs> costs;std::unique_ptr<ScopedCostSwap> scope;};
 std::vector<Frame> frames;
 std::unordered_map<UObject*,std::weak_ptr<Costs>> active;
 std::atomic<DWORD> gameThread{0};
 bool querying=false;
 static UObject* Reference(UStruct* type,void* data,const wchar_t* name,int size,UFunction* input=nullptr){
  auto* field=CastField<FObjectProperty>(Field(type,name));
  if(!DragonWilds::ReflectedContract::Fits(field,size)||field->GetSize()!=sizeof(UObject*)||!field->GetPropertyClass()
     ||(input&&!DragonWilds::ReflectedContract::Input(field,input)))throw std::runtime_error("Crafting object reference contract differs");
  auto* value=field->GetObjectPropertyValue(field->ContainerPtrToValuePtr<void>(data));
  if(value&&(!Live(value)||!value->IsA(field->GetPropertyClass().Get())))throw std::runtime_error("Crafting object reference is unavailable");
  return value;
 }
 static UObject* Ref(UObject* owner,const wchar_t* name){return Reference(owner->GetClassPrivate(),owner,name,owner->GetClassPrivate()->GetPropertiesSize());}
 static UObject* Arg(UFunction* fn,void* params,const wchar_t* name){return Reference(fn,params,name,fn->GetParmsSize(),fn);}
 static UClass* ControllerType(){static auto* type=Find<UClass>(L"/Script/Dominion.DominionPlayerControllerBase");return type;}
 static UObject* Controller(UObject* actor){
  if(!Live(actor))return nullptr;
  if(actor->IsA(ControllerType()))return actor;
  static auto* playerType=Find<UClass>(L"/Script/Dominion.DominionPlayerCharacter");
  if(!actor->IsA(playerType))return nullptr;
  Call get(actor,L"GetController");get.Invoke();auto* result=get.Result();
  if(!Live(result)||!result->IsA(ControllerType()))return nullptr;
  Call pawn(result,L"K2_GetPawn");pawn.Invoke();return pawn.Result()==actor?result:nullptr;
 }
 static UObject* InventoryOwner(UObject* inventory){
  if(!Live(inventory))return nullptr;
  auto* owner=inventory->GetOuterPrivate();
  if(!Live(owner)||!owner->IsA(ControllerType())||owner->GetWorld()!=inventory->GetWorld())return nullptr;
  return Ref(owner,L"InventoryComponent")==inventory?owner:nullptr;
 }
 static UObject* FromInventories(UFunction* fn,void* params){
  auto* field=CastField<FArrayProperty>(Field(fn,L"Inventories"));auto* inner=field?CastField<FObjectProperty>(field->GetInner()):nullptr;
  if(!DragonWilds::ReflectedContract::Input(field,fn)||field->GetSize()!=sizeof(FScriptArray)||!inner||inner->GetSize()!=sizeof(UObject*)||!inner->GetPropertyClass())throw std::runtime_error("Crafting inventory array contract differs");
  auto* array=field->ContainerPtrToValuePtr<FScriptArray>(params);
  if(array->Num()<0||array->Num()>1024||(array->Num()&&!array->GetData()))throw std::runtime_error("Crafting inventory count differs");
  UObject* controller=nullptr;
  for(int i=0;i<array->Num();++i){
   auto* inventory=inner->GetObjectPropertyValue(static_cast<unsigned char*>(array->GetData())+i*sizeof(UObject*));
   if(inventory&&(!Live(inventory)||!inventory->IsA(inner->GetPropertyClass().Get())))throw std::runtime_error("Crafting inventory element is unavailable");
   if(auto* owner=InventoryOwner(inventory)){if(controller&&controller!=owner)return nullptr;controller=owner;}
  }
  return controller;
 }
 static bool Waived(UObject* controller){
  if(!Live(controller)||!controller->GetWorld())return false;
  Call authority(controller,L"HasAuthority");authority.Invoke();Call local(controller,L"IsLocalController");local.Invoke();
  if(!authority.Bool()&&!local.Bool())return false;
  Call pawn(controller,L"K2_GetPawn");pawn.Invoke();auto* player=pawn.Result();
  if(!Live(player)||player->GetWorld()!=controller->GetWorld()||Controller(player)!=controller)return false;
  Call location(player,L"K2_GetActorLocation");location.Invoke();Check(location.fn,L"ReturnValue",L"StructProperty",sizeof(Vec3));
  return PolicyBool(player->GetWorld(),L"RuneSchemaFreeCraftingMaterials",*static_cast<Vec3*>(location.Data(L"ReturnValue")),false);
 }
public:
 static bool Matches(FName name){
  static const FName craft(L"CraftRecipe",FNAME_Add),server(L"Server_CraftRecipe",FNAME_Add),can(L"CanCraftRecipe",FNAME_Add),max(L"GetMaxAmountCraftableOfRecipe",FNAME_Add),has(L"HasRequiredConsumedItemsForRecipe",FNAME_Add);
  return name==craft||name==server||name==can||name==max||name==has;
 }
 void OnTick(){if(!gameThread.load(std::memory_order_relaxed))gameThread.store(GetCurrentThreadId(),std::memory_order_release);}
 void Before(UObject* object,UFunction* fn,void* params){
  const auto thread=gameThread.load(std::memory_order_acquire);
  if(!thread||GetCurrentThreadId()!=thread||(providers.empty()&&active.empty()))return;
  const auto path=fn->GetPathName();
  const bool craft=path==L"/Script/Dominion.InventoryController:CraftRecipe"||path==L"/Script/Dominion.InventoryController:Server_CraftRecipe";
  const bool can=path==L"/Script/Dominion.CraftingFunctionLibrary:CanCraftRecipe";
  if(!craft&&!can&&path!=L"/Script/Dominion.CraftingFunctionLibrary:GetMaxAmountCraftableOfRecipe"&&path!=L"/Script/Dominion.CraftingFunctionLibrary:HasRequiredConsumedItemsForRecipe")return;
  if(!params||frames.size()>=64)throw std::runtime_error("Crafting call frame is unavailable or nested too deeply");
  DragonWilds::ReflectedContract::Frame(fn);
  auto* recipe=Arg(fn,params,L"Recipe");if(!recipe)return;
  static auto* recipeType=Find<UClass>(L"/Script/Dominion.RecipeData");if(!recipe->IsA(recipeType))throw std::runtime_error("Crafting recipe type differs");
  UObject* controller=nullptr;bool waive=false;std::exception_ptr failure;const bool previousQuery=querying;
  try{
   if(craft){
    static auto* inventoryType=Find<UClass>(L"/Script/Dominion.InventoryController");
    if(Live(object)&&object->IsA(inventoryType)){
     auto* owner=object->GetOuterPrivate();
     if(Live(owner)&&owner->IsA(ControllerType())&&owner->GetWorld()==object->GetWorld()&&Ref(owner,L"InventoryController")==object)controller=owner;
    }
   }else if(can)controller=Controller(Arg(fn,params,L"Crafter"));
   else controller=FromInventories(fn,params);
   if(!previousQuery){querying=true;waive=Waived(controller);}
  }catch(...){failure=std::current_exception();}
  querying=previousQuery;
  auto found=active.find(recipe);auto costs=found==active.end()?std::shared_ptr<Costs>{}:found->second.lock();
  if(!waive&&!costs){if(failure)std::rethrow_exception(failure);return;}
  if(!costs){
   auto* field=CastField<FArrayProperty>(Field(recipe->GetClassPrivate(),L"ItemsConsumed"));auto* inner=field?field->GetInner():nullptr;
   if(!DragonWilds::ReflectedContract::Fits(field,recipe->GetClassPrivate()->GetPropertiesSize())||field->GetSize()!=sizeof(FScriptArray)||!inner||inner->GetElementSize()<=0||inner->GetElementSize()>65536||inner->GetMinAlignment()<=0)throw std::runtime_error("Crafting ingredients contract differs");
   auto* array=field->ContainerPtrToValuePtr<FScriptArray>(recipe);
   if(array->Num()<0||array->Num()>1024||(array->Num()&&!array->GetData()))throw std::runtime_error("Crafting ingredient count differs");
   if(!array->Num())return;
   costs=std::make_shared<Costs>();costs->recipe=recipe;costs->field=field;active[recipe]=costs;
  }
  try{auto scope=std::make_unique<ScopedCostSwap>(costs->waived,waive,[costs]{costs->Swap();});frames.push_back({object,fn,params,recipe,costs,std::move(scope)});}
  catch(...){if(costs.use_count()==1)active.erase(recipe);throw;}
  if(failure)std::rethrow_exception(failure);
 }
 void After(UObject* object,UFunction* fn,void* params){
  if(GetCurrentThreadId()!=gameThread.load(std::memory_order_acquire))return;
  for(std::size_t i=frames.size();i;--i)if(frames[i-1].owner==object&&frames[i-1].function==fn&&frames[i-1].params==params){
   auto* recipe=frames[i-1].recipe;frames.erase(frames.begin()+i-1);
   const auto found=active.find(recipe);if(found!=active.end()&&found->second.expired())active.erase(found);return;
  }
 }
 void Clear(){while(!frames.empty())frames.pop_back();active.clear();gameThread=0;querying=false;}
};
}
