#pragma once
#include "Policy.h"
#include "Core/ScopedCostSwap.h"
#include "SDK/Helper/ReflectedContract.h"
#include "Unreal/Property/FEnumProperty.hpp"
#include "NativeMethod.h"
#include <polyhook2/Detour/x64Detour.hpp>
#include <Windows.h>
#include <atomic>
#include <exception>
#include <memory>
#include <unordered_map>
#include <optional>
#include <array>

namespace PS::Buildings {
class CraftingCosts {
 struct Costs {
  UObject* recipe;FArrayProperty* field;FBoolProperty* individual;FScriptArray saved{};bool waived=false,savedIndividual=false;
  void Swap(){
   auto* live=field->ContainerPtrToValuePtr<FScriptArray>(recipe);auto* inner=field->GetInner();
   FScriptArray temp{};const auto size=inner->GetElementSize();const auto alignment=inner->GetMinAlignment();
   temp.MoveAssign(*live,size,alignment);live->MoveAssign(saved,size,alignment);saved.MoveAssign(temp,size,alignment);
   auto* flag=individual->ContainerPtrToValuePtr<void>(recipe);const bool previous=individual->GetPropertyValue(flag);individual->SetPropertyValue(flag,savedIndividual);savedIndividual=previous;
  }
 };
 struct Lease;
 struct Frame {UObject* owner;UFunction* function;void* params;std::unique_ptr<Lease> scope;};
 std::vector<Frame> frames;
 std::unordered_map<UObject*,std::weak_ptr<Costs>> active;
 std::atomic<DWORD> gameThread{0};
 bool querying=false;
 size_t leases=0;
 static inline CraftingCosts* current=nullptr;
 static inline std::array<std::unique_ptr<PLH::x64Detour>,3> hooks;
 static inline std::array<uint64_t,3> originals{};
 static inline UClass* nativeInventoryType=nullptr;
 struct Lease {
  CraftingCosts* owner;UObject* recipe;std::shared_ptr<Costs> costs;std::optional<ScopedCostSwap> scope;
  Lease(CraftingCosts* owner,UObject* recipe,bool waive):owner(owner),recipe(recipe){
   if(!Live(recipe))return;if(owner->leases>=64)throw std::runtime_error("Crafting cost scope nesting exceeded");
   const auto found=owner->active.find(recipe);if(found!=owner->active.end())costs=found->second.lock();if(!waive&&!costs)return;
   if(!costs){
    static auto* recipeType=Find<UClass>(L"/Script/Dominion.RecipeData");if(!recipe->IsA(recipeType))throw std::runtime_error("Crafting recipe type differs");
    auto* field=CastField<FArrayProperty>(Field(recipe->GetClassPrivate(),L"ItemsConsumed"));auto* inner=field?field->GetInner():nullptr;
    auto* individual=CastField<FBoolProperty>(Field(recipe->GetClassPrivate(),L"bConsumeItemsIndividually"));
    if(!DragonWilds::ReflectedContract::Fits(field,recipe->GetClassPrivate()->GetPropertiesSize())||field->GetSize()!=sizeof(FScriptArray)||!inner||inner->GetElementSize()<=0||inner->GetElementSize()>65536||inner->GetMinAlignment()<=0
        ||!DragonWilds::ReflectedContract::Fits(individual,recipe->GetClassPrivate()->GetPropertiesSize()))throw std::runtime_error("Crafting ingredients contract differs");
    auto* array=field->ContainerPtrToValuePtr<FScriptArray>(recipe);if(array->Num()<0||array->Num()>1024||(array->Num()&&!array->GetData()))throw std::runtime_error("Crafting ingredient count differs");
    if(!array->Num())return;
    costs=std::make_shared<Costs>();costs->recipe=recipe;costs->field=field;costs->individual=individual;owner->active[recipe]=costs;
   }
   try{scope.emplace(costs->waived,waive,[state=costs]{state->Swap();});++owner->leases;}
   catch(...){if(costs.use_count()==1)owner->active.erase(recipe);throw;}
  }
  ~Lease(){if(scope){scope.reset();--owner->leases;}costs.reset();const auto found=owner->active.find(recipe);if(found!=owner->active.end()&&found->second.expired())owner->active.erase(found);}
 };
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
 static UObject* FromInventories(const FScriptArray* array){
  if(!array||array->Num()<0||array->Num()>1024||(array->Num()&&!array->GetData()))throw std::runtime_error("Native crafting inventory count differs");
  UObject* controller=nullptr;
  for(int i=0;i<array->Num();++i){auto* inventory=static_cast<UObject*const*>(array->GetData())[i];
   if(inventory&&(!Live(inventory)||!nativeInventoryType||!inventory->IsA(nativeInventoryType)))throw std::runtime_error("Native crafting inventory type differs");
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
 bool Eligible(UObject* controller){
  if(querying)return false;querying=true;try{const bool result=Waived(controller);querying=false;return result;}catch(...){querying=false;throw;}
 }
 static bool NativeReady(){return current&&GetCurrentThreadId()==current->gameThread.load(std::memory_order_acquire)&&(!providers.empty()||!current->active.empty());}
 static uint8_t NativeCan(UObject* crafter,const FScriptArray* recipes,UObject* recipe,int32_t count,const FScriptArray* inventories){
  using Method=uint8_t(*)(UObject*,const FScriptArray*,UObject*,int32_t,const FScriptArray*);std::optional<Lease> scope;
  if(NativeReady())try{auto* controller=Controller(crafter);const bool waive=count>0&&current->Eligible(controller);if(current->ObserveMenu)current->ObserveMenu(controller,recipe,waive);scope.emplace(current,recipe,waive);}catch(const std::exception&){if(!current->active.empty())return 1;}
  return reinterpret_cast<Method>(originals[0])(crafter,recipes,recipe,count,inventories);
 }
 static int32_t NativeMax(UObject* recipe,const FScriptArray* inventories,float scale){
  using Method=int32_t(*)(UObject*,const FScriptArray*,float);std::optional<Lease> scope;
  if(NativeReady())try{const bool waive=current->Eligible(FromInventories(inventories));auto found=current->active.find(recipe);auto state=found==current->active.end()?std::shared_ptr<Costs>{}:found->second.lock();scope.emplace(current,recipe,waive&&state&&state->waived);if(waive)scale=0.f;}catch(const std::exception&){if(!current->active.empty())return 0;}
  return reinterpret_cast<Method>(originals[1])(recipe,inventories,scale);
 }
 static bool NativeHas(UObject* recipe,int32_t count,const FScriptArray* inventories,float scale){
  using Method=bool(*)(UObject*,int32_t,const FScriptArray*,float);std::optional<Lease> scope;
  if(NativeReady())try{const bool waive=count>0&&current->Eligible(FromInventories(inventories));auto found=current->active.find(recipe);auto state=found==current->active.end()?std::shared_ptr<Costs>{}:found->second.lock();scope.emplace(current,recipe,waive&&state&&state->waived);if(waive)scale=0.f;}catch(const std::exception&){if(!current->active.empty())return false;}
  return reinterpret_cast<Method>(originals[2])(recipe,count,inventories,scale);
 }
public:
 std::function<void(UObject*,UObject*,bool)> ObserveMenu;
 void InstallNative(){
  if(hooks[0])return;
  const std::array paths{L"/Script/Dominion.CraftingFunctionLibrary:CanCraftRecipe",L"/Script/Dominion.CraftingFunctionLibrary:GetMaxAmountCraftableOfRecipe",L"/Script/Dominion.CraftingFunctionLibrary:HasRequiredConsumedItemsForRecipe"};
  const std::array targets{reinterpret_cast<uint64_t>(&NativeCan),reinterpret_cast<uint64_t>(&NativeMax),reinterpret_cast<uint64_t>(&NativeHas)};
  const std::array<std::vector<std::wstring>,3> shapes{{{L"ObjectProperty",L"ArrayProperty",L"ObjectProperty",L"IntProperty",L"ArrayProperty"},{L"ObjectProperty",L"ArrayProperty",L"FloatProperty"},{L"ObjectProperty",L"IntProperty",L"ArrayProperty",L"FloatProperty"}}};
  std::array<std::unique_ptr<PLH::x64Detour>,3> pending;
  for(size_t i=0;i<paths.size();++i){auto* fn=Find<UFunction>(paths[i]);namespace C=DragonWilds::ReflectedContract;
   if(!fn->HasAnyFunctionFlags(FUNC_Native)||!fn->HasAnyFunctionFlags(FUNC_Static)||C::Count(fn)!=shapes[i].size()+1)throw std::runtime_error("Native crafting function contract changed");
   std::vector<FProperty*> inputs;for(auto* field:TFieldRange<FProperty>(fn,EFieldIterationFlags::Default))if(C::Input(field,fn))inputs.push_back(field);
   std::sort(inputs.begin(),inputs.end(),[](auto* a,auto* b){return a->GetOffset_Internal()<b->GetOffset_Internal();});
   if(inputs.size()!=shapes[i].size())throw std::runtime_error("Native crafting parameter count changed");
   for(size_t j=0;j<inputs.size();++j){auto* field=inputs[j];if(field->GetClass().GetName()!=shapes[i][j])throw std::runtime_error("Native crafting argument order changed");
    if(auto* array=CastField<FArrayProperty>(field)){auto* element=CastField<FObjectProperty>(array->GetInner());if(array->GetSize()!=sizeof(FScriptArray)||!element||element->GetSize()!=sizeof(UObject*)||!element->GetPropertyClass())throw std::runtime_error("Native crafting array ABI changed");
     if(i==1&&j==1)nativeInventoryType=element->GetPropertyClass().Get();
     if(i==2&&j==2&&element->GetPropertyClass().Get()!=nativeInventoryType)throw std::runtime_error("Native crafting inventory classes differ");}
    else if(field->GetSize()!=(shapes[i][j]==L"ObjectProperty"?sizeof(UObject*):4))throw std::runtime_error("Native crafting scalar ABI changed");
   }
   if(!fn->GetReturnProperty()||fn->GetReturnProperty()->GetSize()!=(i==1?4:1))throw std::runtime_error("Native crafting result ABI changed");
   if(i==0&&!CastField<FEnumProperty>(fn->GetReturnProperty())&&!CastField<FByteProperty>(fn->GetReturnProperty())
      ||i==1&&!CastField<FIntProperty>(fn->GetReturnProperty())||i==2&&!CastField<FBoolProperty>(fn->GetReturnProperty()))throw std::runtime_error("Native crafting result type changed");
   uintptr_t implementation=0;try{implementation=NativeMethod(fn,i==1?4:1);}
   catch(const std::exception& error){throw std::runtime_error(RC::to_string(fn->GetName())+": "+error.what());}
   pending[i]=std::make_unique<PLH::x64Detour>(implementation,targets[i],&originals[i]);
  }
  if(!gameThread.load(std::memory_order_acquire))gameThread.store(GetCurrentThreadId(),std::memory_order_release);
  current=this;try{for(auto& hook:pending)if(!hook->hook())throw std::runtime_error("Native crafting hook was refused");}
  catch(...){for(auto& hook:pending)if(hook)hook->unHook();current=nullptr;throw;}
  hooks=std::move(pending);
 }
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
  if(waive||active.contains(recipe))frames.push_back({object,fn,params,std::make_unique<Lease>(this,recipe,waive)});
  if(failure)std::rethrow_exception(failure);
 }
 void After(UObject* object,UFunction* fn,void* params){
  if(GetCurrentThreadId()!=gameThread.load(std::memory_order_acquire))return;
  for(std::size_t i=frames.size();i;--i)if(frames[i-1].owner==object&&frames[i-1].function==fn&&frames[i-1].params==params){
   frames.erase(frames.begin()+i-1);return;
  }
 }
 void Clear(){for(auto& hook:hooks)if(hook){hook->unHook();hook.reset();}current=nullptr;while(!frames.empty())frames.pop_back();active.clear();gameThread=0;querying=false;}
};
}
