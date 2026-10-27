#pragma once
#include "Policy.h"
#include <polyhook2/Detour/x64Detour.hpp>
#include <Zydis/Zydis.h>
#include <Windows.h>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>
#include <cmath>
namespace PS::Buildings {
// Filter the solver's transient output, leaving saved support flags untouched.
class SupportTick {
 struct Result {unsigned id;float stability;};
 static_assert(sizeof(Result)==8);
 struct Context {WeakObject manager;std::unordered_map<unsigned,float> originals;DWORD thread;};
 static inline std::unordered_map<UObject*,Context> contexts;
 static inline std::unique_ptr<PLH::x64Detour> detour;
 static inline uint64_t trampoline=0;
 static inline ptrdiff_t outputOffset=0;
 static inline std::function<void(const std::string&)> report;
 static bool Covers(Context& context,const std::unordered_map<unsigned,Vec3>& positions,unsigned id){
  auto found=positions.find(id);
  return found!=positions.end()&&PolicyBool(context.manager.Get()->GetWorld(),L"RuneSchemaForceBuildingSupport",found->second,false);
 }
 static void Apply(UObject* component){
  auto found=contexts.find(component);
  if(found!=contexts.end())try{
   auto& context=found->second;
   if(context.manager.Get()&&GetCurrentThreadId()==context.thread){
    // Placement and toggles can happen before the periodic coordinator update.
    RefreshPolicies(context.manager.Get()->GetWorld());
    auto* queue=reinterpret_cast<FScriptArray*>(reinterpret_cast<char*>(component)+outputOffset);
    if(queue->Num()<0||queue->Num()>2000000)throw std::runtime_error("Native stability output count differs");
    std::unordered_map<unsigned,Vec3> positions;
    if(queue->Num()||!context.originals.empty()){
     auto* manager=context.manager.Get();auto* map=CastField<FMapProperty>(Field(manager->GetClassPrivate(),L"BuildingPieces"));auto* state=CastField<FStructProperty>(map->GetValueProp());
     auto* client=CastField<FStructProperty>(Field(state->GetStruct(),L"ClientVisible"));auto* location=Field(client->GetStruct(),L"Location");if(location->GetSize()!=sizeof(Vec3))throw std::runtime_error("Native support positions differ");
     MapPairs(map,manager,[&](void* key,void* value){positions.emplace(*static_cast<unsigned*>(key),*location->ContainerPtrToValuePtr<Vec3>(client->ContainerPtrToValuePtr<void>(value)));});
    }
    std::unordered_set<unsigned> seen;
    auto* values=static_cast<Result*>(queue->GetData());
    for(int i=0;i<queue->Num();++i){
     auto& value=values[i];seen.insert(value.id);
     if(!std::isfinite(value.stability))throw std::runtime_error("Native stability output is not finite");
     if(Covers(context,positions,value.id)){context.originals[value.id]=value.stability;value.stability=1;}
     else context.originals.erase(value.id);
    }
    for(auto it=context.originals.begin();it!=context.originals.end();){
     if(Covers(context,positions,it->first)){++it;continue;}
     if(!seen.contains(it->first)&&positions.contains(it->first)){
      int index=queue->Add(1,sizeof(Result),alignof(Result));static_cast<Result*>(queue->GetData())[index]={it->first,it->second};
     }
     it=context.originals.erase(it);
    }
   }
  }catch(const std::exception& error){if(report)report(std::string("DEVELOPMENT_SUPPORT_FAILED ")+error.what());}
  reinterpret_cast<void(*)(UObject*)>(trampoline)(component);
 }
 static void Install(UObject* component){
  if(component->GetClassPrivate()->GetPropertiesSize()!=1296)throw std::runtime_error("Native stability component layout differs");
  auto address=reinterpret_cast<uintptr_t>((*reinterpret_cast<void***>(component))[128]);uintptr_t inner=0;
  for(unsigned at=0;at<512;){ZydisDisassembledInstruction ins;if(!ZYAN_SUCCESS(ZydisDisassembleIntel(ZYDIS_MACHINE_MODE_LONG_64,address+at,reinterpret_cast<void*>(address+at),16,&ins)))break;
   if(ins.info.mnemonic==ZYDIS_MNEMONIC_RET)break;
   if(ins.info.mnemonic==ZYDIS_MNEMONIC_CALL&&ins.operands[0].type==ZYDIS_OPERAND_TYPE_IMMEDIATE){ZyanU64 target;ZydisCalcAbsoluteAddress(&ins.info,&ins.operands[0],address+at,&target);inner=target;}
   at+=ins.info.length;
  }
  if(!inner)throw std::runtime_error("Native stability calculation path differs");
  uintptr_t consume=0;const unsigned char signature[]={0x40,0x57,0x48,0x83,0xec,0x30,0x83,0xb9,0xd8,0x02,0x00,0x00,0x00};
  for(unsigned at=0;at<512;){ZydisDisassembledInstruction ins;if(!ZYAN_SUCCESS(ZydisDisassembleIntel(ZYDIS_MACHINE_MODE_LONG_64,inner+at,reinterpret_cast<void*>(inner+at),16,&ins)))break;
   if(ins.info.mnemonic==ZYDIS_MNEMONIC_CALL&&ins.operands[0].type==ZYDIS_OPERAND_TYPE_IMMEDIATE){ZyanU64 target;ZydisCalcAbsoluteAddress(&ins.info,&ins.operands[0],inner+at,&target);if(!std::memcmp(reinterpret_cast<void*>(target),signature,sizeof(signature))){if(consume&&consume!=target)throw std::runtime_error("Native stability output path is ambiguous");consume=target;}}
   at+=ins.info.length;
  }
  if(!consume)throw std::runtime_error("Native stability output method differs");
  for(unsigned at=0;at<256;){ZydisDisassembledInstruction ins;if(!ZYAN_SUCCESS(ZydisDisassembleIntel(ZYDIS_MACHINE_MODE_LONG_64,consume+at,reinterpret_cast<void*>(consume+at),16,&ins)))break;
   if(ins.info.mnemonic==ZYDIS_MNEMONIC_MOV&&ins.operands[0].type==ZYDIS_OPERAND_TYPE_REGISTER&&ins.operands[0].reg.value==ZYDIS_REGISTER_RBX&&ins.operands[1].type==ZYDIS_OPERAND_TYPE_MEMORY&&ins.operands[1].mem.base==ZYDIS_REGISTER_RDI){if(outputOffset)throw std::runtime_error("Native stability output array is ambiguous");outputOffset=ins.operands[1].mem.disp.value;}
   at+=ins.info.length;
  }
  if(outputOffset<192||outputOffset+sizeof(FScriptArray)>1296)throw std::runtime_error("Native stability output array is outside component");
  detour=std::make_unique<PLH::x64Detour>(consume,reinterpret_cast<uint64_t>(&Apply),&trampoline);
  if(!detour->hook())throw std::runtime_error("Could not bind native stability output");
 }
public:
 static void SetLogger(std::function<void(const std::string&)> logger){report=std::move(logger);}
 static void Update(UObject* manager){
  auto* component=Object(manager,L"StabilityComponent");if(!Live(component))throw std::runtime_error("Native stability component is absent");
  if(!detour)Install(component);
  auto& context=contexts[component];if(context.manager.Get()!=manager){context.originals.clear();context.manager=manager;context.thread=GetCurrentThreadId();}
 }
 static void Clear(){if(detour)detour->unHook();detour.reset();contexts.clear();outputOffset=0;}
};
}
