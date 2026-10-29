#pragma once
#include "WorldEffects.h"
#include "SDK/Helper/ActorHelper.h"
#include <polyhook2/Detour/x64Detour.hpp>
#include <Zydis/Zydis.h>
#include <Windows.h>
#include <functional>

namespace PS::Buildings {
class Farming {
 using UpdatePlot=void(*)(UObject*,uint64_t,void*,UObject*,const int64_t*);
 using WaterBroadcast=void(*)(uint32_t,const void*,UObject*);
 using WaterLocal=void(*)(UObject*,uint64_t,bool);
 static inline std::unique_ptr<PLH::x64Detour> updateHook;
 static inline uint64_t updateOriginal=0;
 static inline WaterBroadcast broadcast=nullptr;
 static inline WaterLocal localWater=nullptr;
 static inline ptrdiff_t mapOffset=0;
 static inline std::function<void(const std::string&)> report;
 struct Clone {WeakObject source;UObject* plant=nullptr;std::vector<UObject*> stages;};
 static inline std::unordered_map<UObject*,Clone> clones;
 static inline std::unordered_map<UWorld*,double> quantization;
 static bool Code(const void* pointer,std::size_t size=16){MEMORY_BASIC_INFORMATION page{};return VirtualQuery(pointer,&page,sizeof(page))&&page.State==MEM_COMMIT&&!(page.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&(page.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))&&reinterpret_cast<uintptr_t>(pointer)>=reinterpret_cast<uintptr_t>(page.BaseAddress)&&size<=page.RegionSize-(reinterpret_cast<uintptr_t>(pointer)-reinterpret_cast<uintptr_t>(page.BaseAddress));}
 template<class F>static void Instructions(uintptr_t start,unsigned size,F visit){
  if(!Code(reinterpret_cast<void*>(start),std::size_t(size)+15))throw std::runtime_error("Farming native entry is not executable");
  for(unsigned at=0;at<size;){ZydisDisassembledInstruction ins;if(!ZYAN_SUCCESS(ZydisDisassembleIntel(ZYDIS_MACHINE_MODE_LONG_64,start+at,reinterpret_cast<void*>(start+at),16,&ins)))throw std::runtime_error("Farming native path cannot be decoded");visit(ins,start+at);at+=ins.info.length;}
 }
 static std::vector<uintptr_t> Calls(uintptr_t start,unsigned size){
  std::vector<uintptr_t> result;Instructions(start,size,[&](auto& ins,auto address){if(ins.info.mnemonic==ZYDIS_MNEMONIC_CALL&&ins.operands[0].type==ZYDIS_OPERAND_TYPE_IMMEDIATE){ZyanU64 target;ZydisCalcAbsoluteAddress(&ins.info,&ins.operands[0],address,&target);if(std::find(result.begin(),result.end(),target)==result.end())result.push_back(target);}});return result;
 }
 static UScriptStruct* State(){static auto* type=Find<UScriptStruct>(L"/Script/Dominion.FarmPlotState");return type;}
 static std::pair<UStruct*,void*> Part(void* state,const wchar_t* name){auto* property=CastField<FStructProperty>(Field(State(),name));if(!property)throw std::runtime_error("Farm state part differs");return {property->GetStruct(),property->ContainerPtrToValuePtr<void>(state)};}
 static unsigned char& Byte(UStruct* type,void* owner,const wchar_t* name){auto* p=Field(type,name);if(p->GetSize()!=1)throw std::runtime_error("Farm byte field differs");return *p->ContainerPtrToValuePtr<unsigned char>(owner);}
 static UObject* CloneData(UObject* source){
  static unsigned sequence=0;
  auto* created=DragonWilds::ActorHelper::ConstructTransientObject(source->GetClassPrivate(),L"RuneSchemaHealthyPlant_"+std::to_wstring(++sequence));
  created->SetFlags(RF_Transient);created->SetRootSet();
  for(auto* p:TFieldRange<FProperty>(source->GetClassPrivate(),EFieldIterationFlags::Default))p->CopyCompleteValue_InContainer(created,source);
  for(auto* name:{L"DiseaseChance",L"DiseaseChanceNotPreferred"}){
   auto* p=CastField<FFloatProperty>(Field(created->GetClassPrivate(),name));if(!p)throw std::runtime_error("Farm disease chance field differs");*p->ContainerPtrToValuePtr<float>(created)=0;
  }
  return created;
 }
 static UObject* Healthy(UObject* source){
  auto found=clones.find(source);if(found!=clones.end()&&found->second.source.Get()==source)return found->second.plant;
  Clone clone;clone.source=source;clone.plant=CloneData(source);
  auto* property=CastField<FArrayProperty>(Field(source->GetClassPrivate(),L"Stages"));
  auto* inner=property?CastField<FObjectProperty>(property->GetInner()):nullptr;
  if(!inner||inner->GetSize()!=sizeof(UObject*))throw std::runtime_error("Farm plant stages contract differs");
  auto* array=property->ContainerPtrToValuePtr<FScriptArray>(clone.plant);
  if(array->Num()<0||array->Num()>256)throw std::runtime_error("Farm stage count differs");
  for(int i=0;i<array->Num();++i){auto* slot=static_cast<unsigned char*>(array->GetData())+i*sizeof(UObject*);auto* original=inner->GetObjectPropertyValue(slot);if(!original)continue;auto* stage=CloneData(original);clone.stages.push_back(stage);WriteObject(inner,slot,stage);}
  auto* result=clone.plant;clones[source]=std::move(clone);return result;
 }
 static std::optional<Vec3> Position(UObject* subsystem,uint64_t id,void* state){
  auto [type,server]=Part(state,L"ServerState");auto* p=Field(type,L"QuantizedWorldPosition");
  if(p->GetSize()!=12)throw std::runtime_error("Quantized farm position contract differs");
  auto* ints=p->ContainerPtrToValuePtr<int>(server);Vec3 packed{double(ints[0]),double(ints[1]),double(ints[2])};
  Call slot(subsystem,L"BP_GetFarmSlotActor");slot.Copy(L"PlotID",&id);slot.Invoke();
  if(auto* component=slot.Result();Live(component)){
   Call location(component,L"K2_GetComponentLocation");location.Invoke();if(location.Prop(L"ReturnValue")->GetSize()!=sizeof(Vec3))throw std::runtime_error("Farm slot position contract differs");
   auto actual=*static_cast<Vec3*>(location.Data(L"ReturnValue"));
   if(!quantization.contains(subsystem->GetWorld())&&(std::abs(packed.x)+std::abs(packed.y)+std::abs(packed.z)>100)){
    std::vector<double> matches;
    for(double scale:{1.,10.,100.,1000.})if(std::abs(actual.x-packed.x*scale)<=scale+1&&std::abs(actual.y-packed.y*scale)<=scale+1&&std::abs(actual.z-packed.z*scale)<=scale+1)matches.push_back(scale);
    if(matches.size()==1){quantization[subsystem->GetWorld()]=matches.front();if(report)report("FARM_POSITION_CALIBRATED unit_cm="+std::to_string(matches.front()));}
   }
   return actual;
  }
  auto scale=quantization.find(subsystem->GetWorld());if(scale==quantization.end())return {};
  return Vec3{packed.x*scale->second,packed.y*scale->second,packed.z*scale->second};
 }
 static void Wet(UObject* subsystem,uint64_t id,void* state,bool notify){
  auto [type,visible]=Part(state,L"VisibleState");
  auto* settings=Find<UClass>(L"/Script/Dominion.FarmingSettings")->GetClassDefaultObject().Get();
  auto* waters=CastField<FArrayProperty>(Field(subsystem->GetClassPrivate(),L"CachedWaterData"));
  auto* array=waters->ContainerPtrToValuePtr<FScriptArray>(subsystem);
  auto index=Byte(settings->GetClassPrivate(),settings,L"RainWaterIndex");
  auto progress=Byte(settings->GetClassPrivate(),settings,L"WaterNeededForWatering");
  if(index==0||index>=array->Num()||!progress)throw std::runtime_error("Full watering settings differ");
  auto& currentIndex=Byte(type,visible,L"WateredIndex");auto& currentProgress=Byte(type,visible,L"WateringProgress");
  bool changed=currentIndex!=index||currentProgress!=progress;currentIndex=index;currentProgress=progress;
  if(changed&&notify){unsigned char message[]={static_cast<unsigned char>(id>>32),index,progress};broadcast(static_cast<uint32_t>(id),message,subsystem);localWater(subsystem,id,false);}
 }
 static void Filter(UObject* subsystem,uint64_t id,void* state,UObject* plant,const int64_t* time){
  if(!HasWorldPolicy(subsystem->GetWorld(),L"RuneSchemaFarmProtection")){reinterpret_cast<UpdatePlot>(updateOriginal)(subsystem,id,state,plant,time);return;}
  UObject* healthy=plant;bool protectedPlot=false;
  try{
   Call server(Find<UObject>(L"/Script/Engine.Default__KismetSystemLibrary"),L"IsServer");server.Object(L"WorldContextObject",subsystem);server.Invoke();
   if(!server.Bool()){reinterpret_cast<UpdatePlot>(updateOriginal)(subsystem,id,state,plant,time);return;}
   if(auto position=Position(subsystem,id,state);position&&WorldScalar(subsystem->GetWorld(),L"RuneSchemaFarmProtection",*position)>0){
   protectedPlot=true;if(Live(plant))healthy=Healthy(plant);Wet(subsystem,id,state,true);
   auto [visibleType,visible]=Part(state,L"VisibleState");auto& stage=Byte(visibleType,visible,L"PlotStage");
   if(stage==4){stage=1;auto [serverType,server]=Part(state,L"ServerState");Boolean(serverType,server,L"bWasDiseasedThisStage",false);}
  }}catch(const std::exception& error){if(report)report(std::string("FARM_POLICY_FAILED ")+error.what());}
  reinterpret_cast<UpdatePlot>(updateOriginal)(subsystem,id,state,healthy,time);
  if(protectedPlot)try{Wet(subsystem,id,state,true);}catch(const std::exception& error){if(report)report(std::string("FARM_WATER_FAILED ")+error.what());}
 }
public:
 static bool Installed(){return updateHook!=nullptr;}
 static void Install(std::function<void(const std::string&)> logger){
  if(updateHook)return;report=std::move(logger);
  auto* type=Find<UClass>(L"/Script/Dominion.FarmingSubsystem");auto* object=type->GetClassDefaultObject().Get();
  if(type->GetPropertiesSize()!=208||State()->GetPropertiesSize()!=48)throw std::runtime_error("Farming native ABI is unsupported");
  auto layout=FScriptMap::GetScriptLayout(8,8,State()->GetPropertiesSize(),8);
  if(layout.ValueOffset!=8||sizeof(FScriptMap)!=80)throw std::runtime_error("Farm state map ABI differs");
  mapOffset=Field(type,L"CachedCompostData")->GetOffset_Internal()-sizeof(FScriptMap);
  if(mapOffset!=64)throw std::runtime_error("Farm state map offset is unsupported");
  uintptr_t update=0;
  auto tick=reinterpret_cast<uintptr_t>((*reinterpret_cast<void***>(reinterpret_cast<char*>(object)+0x30))[0]);
  auto* serverType=CastField<FStructProperty>(Field(State(),L"ServerState"))->GetStruct().Get();
  auto growth=CastField<FBoolProperty>(Field(serverType,L"bApplyGrowthLock"));
  int lock=CastField<FStructProperty>(Field(State(),L"ServerState"))->GetOffset_Internal()+growth->GetOffset_Internal();
  for(auto target:Calls(tick,768)){
   bool matches=false;
   Instructions(target,48,[&](auto& ins,auto){if(ins.info.mnemonic==ZYDIS_MNEMONIC_CMP&&ins.operands[0].type==ZYDIS_OPERAND_TYPE_MEMORY&&ins.operands[0].mem.base==ZYDIS_REGISTER_R8&&ins.operands[0].mem.disp.value==lock&&ins.operands[1].type==ZYDIS_OPERAND_TYPE_IMMEDIATE&&ins.operands[1].imm.value.u==0)matches=true;});
   if(matches){if(update&&update!=target)throw std::runtime_error("Native plot update is ambiguous");update=target;}
  }
  if(!update)throw std::runtime_error("Native plot update path is unsupported");
  // The native watering notification is the only update helper with this exact message layout.
  const unsigned char waterMessage[]={0x88,0x4c,0x24,0x24,0x4c,0x8b,0xc7,0x8b,0xcd,0x88,0x44,0x24,0x23,0x44,0x88,0x6c,0x24,0x22,0xe8};
  auto* bytes=reinterpret_cast<unsigned char*>(update);
  if(!Code(bytes,1024))throw std::runtime_error("Native water notification range is not executable");
  uintptr_t waterCall=0;
  for(unsigned at=0;at<1024-sizeof(waterMessage);++at)if(!std::memcmp(bytes+at,waterMessage,sizeof(waterMessage))){if(waterCall)throw std::runtime_error("Native water message path is ambiguous");waterCall=update+at+sizeof(waterMessage)-1;}
  if(!waterCall)throw std::runtime_error("Native water notification layout is unsupported");
  broadcast=reinterpret_cast<WaterBroadcast>(waterCall+5+*reinterpret_cast<int*>(waterCall+1));
  uintptr_t local=0;
  Instructions(waterCall+5,24,[&](auto& ins,auto address){if(ins.info.mnemonic==ZYDIS_MNEMONIC_CALL&&ins.operands[0].type==ZYDIS_OPERAND_TYPE_IMMEDIATE){ZyanU64 target;ZydisCalcAbsoluteAddress(&ins.info,&ins.operands[0],address,&target);if(local&&local!=target)throw std::runtime_error("Native water visual callback is ambiguous");local=target;}});
  if(!local)throw std::runtime_error("Native water visual callback is absent");localWater=reinterpret_cast<WaterLocal>(local);
  if(!Code(reinterpret_cast<void*>(broadcast))||!Code(reinterpret_cast<void*>(localWater)))throw std::runtime_error("Native farm notification target is not executable");
  updateHook=std::make_unique<PLH::x64Detour>(update,reinterpret_cast<uint64_t>(&Filter),&updateOriginal);
  if(!updateHook->hook()){updateHook.reset();throw std::runtime_error("Native farm callback could not bind");}
  if(report)report("FARM_CALLBACKS_BOUND version=1");
 }
 static void Update(UObject* subsystem){
  if(!updateHook||!Live(subsystem))return;
  auto* map=reinterpret_cast<FScriptMap*>(reinterpret_cast<char*>(subsystem)+mapOffset);
  auto layout=FScriptMap::GetScriptLayout(8,8,State()->GetPropertiesSize(),8);
  if(map->GetMaxIndex()<0||map->GetMaxIndex()>1000000)throw std::runtime_error("Farm plot map count differs");
  for(int i=0;i<map->GetMaxIndex();++i)if(map->IsValidIndex(i)){
   auto* pair=static_cast<unsigned char*>(map->GetData(i,layout));auto id=*reinterpret_cast<uint64_t*>(pair);auto* state=pair+layout.ValueOffset;
   if(auto position=Position(subsystem,id,state);position&&WorldScalar(subsystem->GetWorld(),L"RuneSchemaFarmProtection",*position)>0)Wet(subsystem,id,state,true);
  }
 }
 static double Harvest(UObject* object,UFunction* fn,void* frame){
  auto* plot=Field(fn,L"PlotID");if(plot->GetSize()!=8)throw std::runtime_error("Farm harvest plot ID differs");auto id=*plot->ContainerPtrToValuePtr<uint64_t>(frame);
  std::vector<UObject*> list;UObjectGlobals::FindAllOf(L"FarmingSubsystem",list);
  for(auto* subsystem:list)if(Live(subsystem)&&subsystem->GetWorld()==object->GetWorld()){
   Call slot(subsystem,L"BP_GetFarmSlotActor");slot.Copy(L"PlotID",&id);slot.Invoke();if(auto* component=slot.Result();Live(component)){
    Call location(component,L"K2_GetComponentLocation");location.Invoke();if(location.Prop(L"ReturnValue")->GetSize()!=sizeof(Vec3))throw std::runtime_error("Farm harvest position differs");
    return WorldScalar(object->GetWorld(),L"RuneSchemaFarmHarvestMultiplier",*static_cast<Vec3*>(location.Data(L"ReturnValue")),1);
   }
  }return 1;
 }
 static void Stop(){if(updateHook)updateHook->unHook();updateHook.reset();for(auto& [_,clone]:clones){for(auto* stage:clone.stages)stage->ClearRootSet();if(clone.plant)clone.plant->ClearRootSet();}clones.clear();quantization.clear();broadcast=nullptr;localWater=nullptr;}
};
}
