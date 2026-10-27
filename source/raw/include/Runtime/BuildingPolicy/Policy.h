#pragma once
#include "Reflection.h"
#include <algorithm>
#include <cmath>
#include <optional>

namespace PS::Buildings {
struct Provider { WeakObject owner; };
inline std::vector<Provider> providers;

inline void Check(UFunction* fn,const wchar_t* name,const wchar_t* type,int size){
 auto* p=Field(fn,name);
 if(!p->HasAnyPropertyFlags(CPF_Parm)||p->GetClass().GetName()!=type||p->GetSize()!=size)
  throw std::runtime_error("Building policy parameter contract differs: "+RC::to_string(name));
}
inline void CheckPosition(UFunction* fn){
 Check(fn,L"Position",L"StructProperty",sizeof(Vec3));
 auto* p=CastField<FStructProperty>(Field(fn,L"Position"));
 if(!p||p->GetStruct()->GetPathName()!=L"/Script/CoreUObject.Vector")throw std::runtime_error("Building policy position type differs");
}
inline bool Register(UObject* object){
 if(!Live(object)||!object->GetWorld())return false;
 auto* version=object->GetFunctionByNameInChain(L"RuneSchemaBuildingPolicyVersion");
 if(!version)return false;
 Check(version,L"ReturnValue",L"IntProperty",4);
 Call call(object,L"RuneSchemaBuildingPolicyVersion");call.Invoke();
 if(*static_cast<int*>(call.Data(L"ReturnValue"))!=1)throw std::runtime_error("Unsupported building policy version");
 for(auto name:{L"RuneSchemaForceBuildingSupport",L"RuneSchemaBlocksBuildingDamage",L"RuneSchemaBlocksBuildingPlacement",L"RuneSchemaBuildingOverlayEnabled"}){
  auto* fn=object->GetFunctionByNameInChain(name);if(!fn)continue;
  CheckPosition(fn);Check(fn,L"ReturnValue",L"BoolProperty",1);
 }
 if(auto* fn=object->GetFunctionByNameInChain(L"RuneSchemaBlocksBuildingDestruction")){
  CheckPosition(fn);Check(fn,L"Reason",L"IntProperty",4);Check(fn,L"ReturnValue",L"BoolProperty",1);
 }
 if(auto* fn=object->GetFunctionByNameInChain(L"RuneSchemaValidateBuildingPlacement")){
  CheckPosition(fn);Check(fn,L"State",L"IntProperty",4);Check(fn,L"ReturnValue",L"IntProperty",4);
 }
 if(auto* fn=object->GetFunctionByNameInChain(L"RuneSchemaRefreshBuildingPolicy"))
  if(fn->GetParmsSize()!=0)throw std::runtime_error("Building policy refresh must have no parameters");
 if(auto* fn=object->GetFunctionByNameInChain(L"RuneSchemaBuildingOverlayMaterial")){
  auto* p=CastField<FObjectPropertyBase>(Field(fn,L"ReturnValue"));
  if(!p||p->GetSize()!=8)throw std::runtime_error("Building overlay material return differs");
 }
 for(auto& entry:providers)if(entry.owner.Get()==object)return false;
 Provider value;value.owner=object;providers.push_back(std::move(value));return true;
}
inline std::vector<UObject*> Providers(UWorld* world){
 std::vector<UObject*> result;
 for(auto it=providers.begin();it!=providers.end();){
  auto* owner=it->owner.Get();
  if(!Live(owner)){it=providers.erase(it);continue;}
  if(owner->GetWorld()==world)result.push_back(owner);++it;
 }
 return result;
}
inline void RefreshPolicies(UWorld* world){
 for(auto* owner:Providers(world))if(owner->GetFunctionByNameInChain(L"RuneSchemaRefreshBuildingPolicy")){Call call(owner,L"RuneSchemaRefreshBuildingPolicy");call.Invoke();}
}
inline bool ProviderBool(UObject* owner,const wchar_t* name,Vec3 position){
 if(!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(position.z))throw std::runtime_error("Building policy position is not finite");
 if(!owner->GetFunctionByNameInChain(name))return false;
 Call call(owner,name);call.Copy(L"Position",&position);call.Invoke();return call.Bool();
}
inline bool PolicyBool(UWorld* world,const wchar_t* name,Vec3 position,bool refresh=true){
 if(refresh)RefreshPolicies(world);
 for(auto* owner:Providers(world))if(ProviderBool(owner,name,position))return true;
 return false;
}
inline bool BlocksDestruction(UWorld* world,Vec3 position,unsigned char reason){
 RefreshPolicies(world);
 for(auto* owner:Providers(world))if(owner->GetFunctionByNameInChain(L"RuneSchemaBlocksBuildingDestruction")){
  Call call(owner,L"RuneSchemaBlocksBuildingDestruction");call.Copy(L"Position",&position);int value=reason;call.Copy(L"Reason",&value);call.Invoke();if(call.Bool())return true;
 }
 return false;
}
inline unsigned char ValidatePlacement(UWorld* world,Vec3 position,unsigned char state){
 RefreshPolicies(world);
 if(PolicyBool(world,L"RuneSchemaBlocksBuildingPlacement",position,false))return 9;
 int result=state;
 for(auto* owner:Providers(world))if(owner->GetFunctionByNameInChain(L"RuneSchemaValidateBuildingPlacement")){
  Call call(owner,L"RuneSchemaValidateBuildingPlacement");call.Copy(L"Position",&position);call.Copy(L"State",&result);call.Invoke();int next=*static_cast<int*>(call.Data(L"ReturnValue"));
  if(next<0||next>13)throw std::runtime_error("Building policy placement state out of range");result=next;
 }
 return static_cast<unsigned char>(result);
}
}
