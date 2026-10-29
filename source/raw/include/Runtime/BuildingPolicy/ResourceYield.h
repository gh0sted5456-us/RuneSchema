#pragma once
#include "WorldEffects.h"
#include <polyhook2/Detour/x64Detour.hpp>
#include <Zydis/Zydis.h>
#include <Windows.h>
#include <deque>

namespace PS::Buildings {
class ResourceYield {
 using Quantity=int(*)(UObject*,UObject*,const void*);
 static inline std::unique_ptr<PLH::x64Detour> hook;
 static inline uint64_t original=0;
 static inline bool observed=false;
 static inline bool observedNeutral=false;
 static inline std::function<void(const std::string&)> report;
 static DWORD RootFunction(PRUNTIME_FUNCTION function,DWORD64 image){
  for(int depth=0;depth<8;++depth){
   auto* info=reinterpret_cast<const unsigned char*>(image+function->UnwindData);
   if((info[0]&7)!=1)throw std::runtime_error("Native resource unwind version differs");
   if(!((info[0]>>3)&UNW_FLAG_CHAININFO))return function->BeginAddress;
   function=reinterpret_cast<PRUNTIME_FUNCTION>(const_cast<unsigned char*>(info)+4+((info[2]+1)&~1)*2);
  }
  throw std::runtime_error("Native resource unwind chain is cyclic");
 }
 template<class F>static void Instructions(uintptr_t start,F visit){
  DWORD64 image=0;auto* function=RtlLookupFunctionEntry(start,&image,nullptr);
  if(!function||image+function->BeginAddress!=start)throw std::runtime_error("Native resource function bounds differ");
  auto root=RootFunction(function,image);std::deque<PRUNTIME_FUNCTION> fragments{function};std::unordered_set<DWORD> seen;unsigned total=0;
  while(!fragments.empty()){
   auto* fragment=fragments.front();fragments.pop_front();if(!seen.insert(fragment->BeginAddress).second)continue;
   if(fragment->EndAddress<=fragment->BeginAddress||(total+=fragment->EndAddress-fragment->BeginAddress)>32768)throw std::runtime_error("Native resource function bounds differ");
   for(uintptr_t at=image+fragment->BeginAddress;at<image+fragment->EndAddress;){
    ZydisDisassembledInstruction ins;if(!ZYAN_SUCCESS(ZydisDisassembleIntel(ZYDIS_MACHINE_MODE_LONG_64,at,reinterpret_cast<void*>(at),std::min<uintptr_t>(15,image+fragment->EndAddress-at),&ins)))throw std::runtime_error("Native resource function cannot be decoded");visit(ins,at);
    if((ins.info.meta.category==ZYDIS_CATEGORY_COND_BR||ins.info.meta.category==ZYDIS_CATEGORY_UNCOND_BR)&&ins.operands[0].type==ZYDIS_OPERAND_TYPE_IMMEDIATE){
     ZyanU64 target;ZydisCalcAbsoluteAddress(&ins.info,&ins.operands[0],at,&target);DWORD64 targetImage=0;auto* targetFunction=RtlLookupFunctionEntry(target,&targetImage,nullptr);
     if(targetFunction&&targetImage==image&&RootFunction(targetFunction,image)==root&&!seen.contains(targetFunction->BeginAddress))fragments.push_back(targetFunction);
    }
    at+=ins.info.length;
   }
   DWORD64 nextImage=0;auto* next=RtlLookupFunctionEntry(image+fragment->EndAddress,&nextImage,nullptr);
   if(next&&nextImage==image&&RootFunction(next,image)==root&&!seen.contains(next->BeginAddress))fragments.push_back(next);
  }
 }
 static std::vector<uintptr_t> Calls(uintptr_t start){
  std::vector<uintptr_t> targets;Instructions(start,[&](auto& ins,auto at){if(ins.info.mnemonic==ZYDIS_MNEMONIC_CALL&&ins.operands[0].type==ZYDIS_OPERAND_TYPE_IMMEDIATE){ZyanU64 target;ZydisCalcAbsoluteAddress(&ins.info,&ins.operands[0],at,&target);if(std::find(targets.begin(),targets.end(),target)==targets.end())targets.push_back(target);}});return targets;
 }
 static bool Resource(UObject* owner){
  auto* base=Find<UClass>(L"/Script/Dominion.BaseInteractableResource");auto* destructible=Find<UClass>(L"/Script/Dominion.RespawningDestructibleWorldActorV2");
  return owner->IsA(base)||owner->IsA(destructible);
 }
 static int Filter(UObject* component,UObject* instigator,const void* data){
  const int quantity=reinterpret_cast<Quantity>(original)(component,instigator,data);
  if(quantity<=0||!Live(component))return quantity;
  try{
   Call getOwner(component,L"GetOwner");getOwner.Invoke();auto* owner=getOwner.Result();if(!Live(owner)||!Resource(owner))return quantity;
   Call authority(owner,L"HasAuthority");authority.Invoke();if(!authority.Bool())return quantity;
   double factor=WorldScalar(owner->GetWorld(),L"RuneSchemaResourceYieldMultiplier",ActorPosition(owner),1);
   const int result=PS::WorldEffectRules::Quantity(quantity,factor);
   if(factor!=1&&!observed){observed=true;if(report)report("RESOURCE_YIELD_CALLBACK_APPLIED factor="+std::to_string(factor)+" original="+std::to_string(quantity)+" result="+std::to_string(std::round(result)));}
   if(factor==1&&observed&&!observedNeutral){observedNeutral=true;if(report)report("RESOURCE_YIELD_CALLBACK_NEUTRAL original="+std::to_string(quantity)+" result="+std::to_string(quantity));}
   return result;
  }catch(const std::exception& error){if(report)report(std::string("RESOURCE_YIELD_FAILED ")+error.what());return quantity;}
 }
public:
 static void Install(std::function<void(const std::string&)> logger){
  if(hook)return;report=std::move(logger);
  auto* function=Find<UFunction>(L"/Script/Dominion.ItemDropComponent:DropItems");
  auto* parameter=CastField<FObjectPropertyBase>(Field(function,L"InOptionalInstigator"));
  if(!parameter||parameter->GetSize()!=8||function->GetParmsSize()!=8||function->GetReturnProperty())throw std::runtime_error("Native item-drop entry contract differs");
  auto wrapper=reinterpret_cast<uintptr_t>(function->GetFuncPtr());auto calls=Calls(wrapper);if(calls.empty())throw std::runtime_error("Native item-drop method is absent");
  auto entry=calls.back();
  auto* data=Find<UScriptStruct>(L"/Script/Dominion.ItemDropData");
  if(data->GetPropertiesSize()!=216)throw std::runtime_error("Native item-drop data size differs");
  const int minimum=Field(data,L"MinToDrop")->GetOffset_Internal(),maximum=Field(data,L"MaxToDrop")->GetOffset_Internal();
  std::deque<std::pair<uintptr_t,int>> pending{{entry,0}};std::unordered_set<uintptr_t> visited;std::vector<uintptr_t> matches;
  while(!pending.empty()&&visited.size()<256){
   auto [address,depth]=pending.front();pending.pop_front();if(!visited.insert(address).second)continue;
   bool min=false,max=false;
   try{
    Instructions(address,[&](auto& ins,auto at){
     if(at-address>96||ins.info.mnemonic!=ZYDIS_MNEMONIC_MOV)return;
     if(ins.operands[1].type==ZYDIS_OPERAND_TYPE_MEMORY&&ins.operands[1].mem.base==ZYDIS_REGISTER_R8&&ins.operands[1].size==32){
      min|=ins.operands[1].mem.disp.value==minimum;max|=ins.operands[1].mem.disp.value==maximum;
     }
    });
    if(min&&max)matches.push_back(address);
    if(depth<3)for(auto target:Calls(address))if(target>entry-0x200000&&target<entry+0x200000)pending.emplace_back(target,depth+1);
   }catch(const std::exception&){if(address==entry)throw;}
  }
  if(!pending.empty()||matches.size()!=1){
   std::string paths;auto module=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));for(auto address:visited)paths+=" "+std::to_string(address-module);
   throw std::runtime_error("Native resource quantity path is ambiguous or unsupported: matches="+std::to_string(matches.size())+" visited="+std::to_string(visited.size())+" pending="+std::to_string(pending.size())+" min="+std::to_string(minimum)+" max="+std::to_string(maximum)+" wrapper="+std::to_string(wrapper-module)+" entry="+std::to_string(entry-module)+" paths="+paths);
  }
  hook=std::make_unique<PLH::x64Detour>(matches.front(),reinterpret_cast<uint64_t>(&Filter),&original);
  if(!hook->hook()){hook.reset();throw std::runtime_error("Native resource yield callback could not bind");}
  if(report)report("RESOURCE_YIELD_CALLBACK_BOUND version=1");
 }
 static void Stop(){if(hook)hook->unHook();hook.reset();observed=observedNeutral=false;}
};
}
