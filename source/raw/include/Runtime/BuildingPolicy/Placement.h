#pragma once
#include "Policy.h"

#include <polyhook2/Detour/x64Detour.hpp>
#include <Windows.h>
#include <memory>
#include <cstring>
#include <functional>
#include <cstdlib>
namespace PS::Buildings {
class Placement {
 using Validate=unsigned char(*)(UObject*,void*,const void*,void*,float*,bool,bool);
 static inline uint64_t trampoline=0;
 static inline std::unique_ptr<PLH::x64Detour> detour;
 static inline std::function<void(const std::string&)> report;
 static Vec3 Position(void* facade){
  return FacadeTransform(facade).location;
 }
public:
 static UObject* PieceDataFor(void* facade){
  static int slot=[] {
   auto* type=Find<UScriptStruct>(L"/Script/Dominion.LightweightBuildingPiece");
   auto offset=Field(type,L"BuildingPieceData")->GetOffset_Internal();
   auto* empty=_aligned_malloc(type->GetPropertiesSize(),16);if(!empty)throw std::bad_alloc();
   type->InitializeStruct(empty);auto** table=*static_cast<void***>(empty);int found=-1;
   for(int index=0;index<64;++index){
    auto* code=static_cast<unsigned char*>(table[index]);MEMORY_BASIC_INFORMATION page{};
    if(!VirtualQuery(code,&page,sizeof(page))||page.State!=MEM_COMMIT||!(page.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))continue;
    if(code[0]==0x48&&code[1]==0x8b&&code[2]==0x81&&*reinterpret_cast<const int*>(code+3)==offset&&code[7]==0xc3){
     if(found!=-1)throw std::runtime_error("Building recipe getter is ambiguous");found=index;
    }
   }
   type->DestroyStruct(empty);_aligned_free(empty);
   if(found<0)throw std::runtime_error("Building recipe getter signature differs");return found;
  }();
  if(!facade)return nullptr;
  auto* data=reinterpret_cast<UObject*(*)(void*)>((*static_cast<void***>(facade))[slot])(facade);
  if(data&&!data->IsA(Find<UClass>(L"/Script/Dominion.BuildingPieceData")))throw std::runtime_error("Building recipe getter returned another type");
  return data;
 }
 static UObject* RecipePolicy(UObject* data){
  if(!data)return nullptr;
  auto* field=CastField<FObjectPropertyBase>(Field(data->GetClassPrivate(),L"BuildableActor"));if(!field)throw std::runtime_error("Building actor recipe reference differs");
  auto* address=field->ContainerPtrToValuePtr<void>(data);auto* type=field->GetObjectPropertyValue(address);
  if(!type){
   Call load(Find<UObject>(L"/Script/Engine.Default__KismetSystemLibrary"),L"LoadClassAsset_Blocking");
   for(auto* p:TFieldRange<FProperty>(load.fn,EFieldIterationFlags::Default))if(p->HasAnyPropertyFlags(CPF_Parm)&&!p->HasAnyPropertyFlags(CPF_ReturnParm)){
    if(p->GetClass().GetName()!=L"SoftClassProperty"||p->GetSize()!=field->GetSize())throw std::runtime_error("Building class loader signature differs");
    p->CopyCompleteValue(p->ContainerPtrToValuePtr<void>(load.frame.data()),address);
   }
   load.Invoke();type=load.Result();
  }
  if(!type)return nullptr;
  if(!type->IsA<UClass>())throw std::runtime_error("Building actor recipe did not resolve a class");
  return static_cast<UClass*>(type)->GetClassDefaultObject().Get();
 }
 static UObject* ActorFor(void* facade){
  static auto* type=Find<UScriptStruct>(L"/Script/Dominion.ActorBuildingPiece");
  static void* table=[&]{
   auto size=type->GetPropertiesSize();if(size<8||size>65536)throw std::runtime_error("Actor building facade size differs");
   auto* empty=_aligned_malloc(size,16);if(!empty)throw std::bad_alloc();
   type->InitializeStruct(empty);auto* result=*static_cast<void**>(empty);type->DestroyStruct(empty);_aligned_free(empty);return result;
  }();
  if(!facade||*static_cast<void**>(facade)!=table)return nullptr;
  auto* property=CastField<FObjectPropertyBase>(Field(type,L"BuildingActor"));
  if(!property)throw std::runtime_error("Actor building facade reference differs");
  auto* actor=property->GetObjectPropertyValue(property->ContainerPtrToValuePtr<void>(facade));return Live(actor)?actor:nullptr;
 }
 static unsigned char OwnState(void* piece,unsigned char state){
  auto* actor=ActorFor(piece);if(!actor)actor=RecipePolicy(PieceDataFor(piece));if(!actor)return state;
  auto* fn=actor->GetFunctionByNameInChain(L"RuneSchemaValidateOwnBuildingPlacement");if(!fn)return state;
  Check(fn,L"State",L"IntProperty",4);Check(fn,L"ReturnValue",L"IntProperty",4);
  Call call(actor,L"RuneSchemaValidateOwnBuildingPlacement");int input=state;call.Copy(L"State",&input);call.Invoke();
  int result=*static_cast<int*>(call.Data(L"ReturnValue"));if(result<0||result>13)throw std::runtime_error("Own building placement state out of range");
  return static_cast<unsigned char>(result);
 }
private:
 static unsigned char Filter(UObject* validator,void* piece,const void* snapping,void* supporting,float* stability,bool ghost,bool cheat){
  auto state=reinterpret_cast<Validate>(trampoline)(validator,piece,snapping,supporting,stability,ghost,cheat);
  try{
  if(!piece||!Live(validator))return state;
  auto position=Position(piece);
  auto result=ValidatePlacement(validator->GetWorld(),position,OwnState(piece,state));
  if(result==0&&stability&&PolicyBool(validator->GetWorld(),L"RuneSchemaForceBuildingSupport",position))*stability=1;
  return result;
  }catch(const std::exception& error){if(report)report(std::string("DEVELOPMENT_PLACEMENT_FAILED ")+error.what());return state;}
 }
public:
 static void Install(std::function<void(const std::string&)> logger){
  report=std::move(logger);
  // Match the validator's complete argument-saving prologue, never a release-specific address.
  const unsigned char signature[]={0x48,0x89,0x5c,0x24,0x08,0x4c,0x89,0x4c,0x24,0x20,0x4c,0x89,0x44,0x24,0x18,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x6c,0x24,0xf1,0x48,0x81,0xec,0x00,0x01,0x00,0x00,0x48,0x8b,0x02,0x4c,0x8b,0xf9,0x48,0x8b,0xca,0x48,0x8b,0xf2};
  // Game Pass saves RBX on the stack and uses R13 for the same validator instance.
  const unsigned char gdkSignature[]={0x4c,0x89,0x4c,0x24,0x20,0x4c,0x89,0x44,0x24,0x18,0x55,0x53,0x56,0x57,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x6c,0x24,0xf1,0x48,0x81,0xec,0x00,0x01,0x00,0x00,0x48,0x8b,0x02,0x4c,0x8b,0xe9,0x48,0x8b,0xca,0x48,0x8b,0xf2,0xff,0x90,0x00,0x01,0x00,0x00,0x84,0xc0};
  auto* module=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(module);auto* nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(module+dos->e_lfanew);
  auto* sections=IMAGE_FIRST_SECTION(nt);std::vector<unsigned char*> matches;
  for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i)if(sections[i].Characteristics&IMAGE_SCN_MEM_EXECUTE){
   auto size=sections[i].Misc.VirtualSize;auto* start=module+sections[i].VirtualAddress;
   for(size_t at=0;at+std::max(sizeof(signature),sizeof(gdkSignature))<=size;++at)
    if((start[at]==signature[0]&&!std::memcmp(start+at,signature,sizeof(signature)))||(start[at]==gdkSignature[0]&&!std::memcmp(start+at,gdkSignature,sizeof(gdkSignature))))matches.push_back(start+at);
  }
  if(matches.size()!=1)throw std::runtime_error("Native placement validator signature is not unique or unsupported");
  auto* type=Find<UClass>(L"/Script/Dominion.BuildValidationComponent");if(type->GetPropertiesSize()!=232)throw std::runtime_error("Native placement validator type differs");
  detour=std::make_unique<PLH::x64Detour>(reinterpret_cast<uint64_t>(matches[0]),reinterpret_cast<uint64_t>(&Filter),&trampoline);
  if(!detour->hook())throw std::runtime_error("Could not bind native placement validator");
  if(report)report("PLACEMENT_CALLBACK_BOUND version=1");
 }
 static bool BlockPlacement(UObject* object,UFunction* function,void* params){
  if(function->GetPathName()!=L"/Script/Dominion.BuildModeComponent:Server_SpawnBuilding")return false;
  auto* p=CastField<FStructProperty>(Field(function,L"SpawnTransform"));if(!p||p->GetSize()!=sizeof(Transform))throw std::runtime_error("Native spawn transform ABI differs");
  return PolicyBool(object->GetWorld(),L"RuneSchemaBlocksBuildingPlacement",p->ContainerPtrToValuePtr<Transform>(params)->location);
 }
 static void Clear(){if(detour)detour->unHook();detour.reset();}
};
}
