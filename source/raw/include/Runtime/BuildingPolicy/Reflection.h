#pragma once
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Helpers/Casting.hpp>
#include <Helpers/String.hpp>
#include <stdexcept>
#include <vector>
#include <cstring>
using namespace RC;using namespace RC::Unreal;
namespace PS::Buildings {
inline FProperty* Field(UStruct* type,const wchar_t* name){auto* p=type->FindProperty(FName(name,FNAME_Find));if(!p)throw std::runtime_error("Missing native field: "+RC::to_string(name));return p;}
inline void* Value(UStruct* type,void* owner,const wchar_t* name){return Field(type,name)->ContainerPtrToValuePtr<void>(owner);}
inline UObject* Object(UObject* owner,const wchar_t* name){auto* p=CastField<FObjectPropertyBase>(Field(owner->GetClassPrivate(),name));if(!p)throw std::runtime_error("Native object field type differs");return p->GetObjectPropertyValue(p->ContainerPtrToValuePtr<void>(owner));}
inline void WriteObject(FProperty* property,void* destination,UObject* object){
 if(!CastField<FObjectProperty>(property)||property->GetElementSize()!=sizeof(UObject*))throw std::runtime_error("Native object value type differs");
 property->CopyCompleteValue(destination,&object);
}
inline bool Boolean(UStruct* type,void* owner,const wchar_t* name){auto* p=CastField<FBoolProperty>(Field(type,name));if(!p)throw std::runtime_error("Native boolean field type differs");return p->GetPropertyValue(p->ContainerPtrToValuePtr<void>(owner));}
inline void Boolean(UStruct* type,void* owner,const wchar_t* name,bool value){auto* p=CastField<FBoolProperty>(Field(type,name));if(!p)throw std::runtime_error("Native boolean field type differs");p->SetPropertyValue(p->ContainerPtrToValuePtr<void>(owner),value);}
inline bool Live(UObject* value){return value&&!value->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed));}
template<class T>T* Find(const wchar_t* path){auto* o=UObjectGlobals::StaticFindObject<T*>(nullptr,nullptr,path);if(!o)throw std::runtime_error("Missing native type: "+RC::to_string(path));return o;}
struct Call {
 UObject* self;UFunction* fn;std::vector<unsigned char> frame;std::vector<FProperty*> initialized;
 Call(UObject* target,const wchar_t* name):self(target){
  if(!target||!(fn=target->GetFunctionByNameInChain(name)))throw std::runtime_error("Missing native function: "+RC::to_string(name));
  auto size=fn->GetParmsSize();if(size<0||size>65536)throw std::runtime_error("Invalid native frame size");frame.resize(size?size:1);
  for(auto* p:TFieldRange<FProperty>(fn,EFieldIterationFlags::Default))if(p->HasAnyPropertyFlags(CPF_Parm)){
   if(p->GetOffset_Internal()<0||p->GetOffset_Internal()+p->GetSize()>size)throw std::runtime_error("Native parameter outside frame");
   p->InitializeValue_InContainer(frame.data());initialized.push_back(p);
  }
 }
 ~Call(){for(auto p=initialized.rbegin();p!=initialized.rend();++p)(*p)->DestroyValue_InContainer(frame.data());}
 FProperty* Prop(const wchar_t* name){auto* p=Field(fn,name);if(!p->HasAnyPropertyFlags(CPF_Parm))throw std::runtime_error("Not a native parameter");return p;}
 void* Data(const wchar_t* name){return Prop(name)->ContainerPtrToValuePtr<void>(frame.data());}
 void Text(const wchar_t* name,const wchar_t* text){if(!Prop(name)->ImportText_Direct(text,Data(name),self,0,nullptr))throw std::runtime_error("Native parameter import failed");}
 void Copy(const wchar_t* name,const void* data){Prop(name)->CopyCompleteValue(Data(name),data);}
 void Object(const wchar_t* name,UObject* object){WriteObject(Prop(name),Data(name),object);}
 void Invoke(){self->ProcessEvent(fn,frame.data());}
 bool Bool(const wchar_t* name=L"ReturnValue"){auto* p=CastField<FBoolProperty>(Prop(name));if(!p)throw std::runtime_error("Native bool return differs");return p->GetPropertyValue(Data(name));}
 UObject* Result(){auto* p=CastField<FObjectPropertyBase>(Prop(L"ReturnValue"));if(!p)throw std::runtime_error("Native object return differs");return p->GetObjectPropertyValue(Data(L"ReturnValue"));}
};
// The engine creates/resolves these handles; the loader's weak-pointer helper
// is incompatible with the game's packed object array.
class WeakObject {
 alignas(8) unsigned char value[8]{};
 static FObjectPropertyBase* Property(){
  auto* p=CastField<FWeakObjectProperty>(Field(Find<UClass>(L"/Script/Dominion.BuildModeComponent"),L"PlayerController"));
  if(!p||p->GetSize()!=8)throw std::runtime_error("Native weak reference property differs");return p;
 }
public:
 UObject* Get()const{return Property()->GetObjectPropertyValue(value);}
 void operator=(UObject* object){
  Call soft(Find<UObject>(L"/Script/Engine.Default__KismetSystemLibrary"),L"Conv_ObjectToSoftObjectReference");soft.Object(L"Object",object);soft.Invoke();
  if(!CastField<FSoftObjectProperty>(soft.Prop(L"ReturnValue"))||soft.Prop(L"ReturnValue")->GetSize()<8)throw std::runtime_error("Native soft reference layout differs");
  std::memcpy(value,soft.Data(L"ReturnValue"),8);
  if(Get()!=object)throw std::runtime_error("Native weak reference identity differs");
 }
};
struct Vec3 {double x,y,z;};
struct alignas(16) Transform {double rotation[4];Vec3 location;double pad;Vec3 scale;double pad2;};
static_assert(sizeof(Transform)==96);
inline const Transform& FacadeTransform(void* facade){
 auto* transform=reinterpret_cast<const Transform*(*)(void*)>((*static_cast<void***>(facade))[20])(facade);
 if(!transform)throw std::runtime_error("Native building transform is absent");return *transform;
}
struct Instanced {UScriptStruct* type;void* data;};
static_assert(sizeof(Instanced)==16);
inline Instanced* SingleComponent(UObject* component){
 auto* host=Field(component->GetClassPrivate(),L"NativeComponentContainer");
 if(host->GetElementSize()<sizeof(Instanced))throw std::runtime_error("Native component host ABI differs");
 auto* value=host->ContainerPtrToValuePtr<Instanced>(component);return value->type&&value->data?value:nullptr;
}
inline Instanced* ComponentInHost(UStruct* type,void* object,UStruct* desiredType){
 auto* host=CastField<FStructProperty>(Field(type,L"NativeComponents"));if(!host)throw std::runtime_error("Native host type differs");
 auto* containers=CastField<FArrayProperty>(Field(host->GetStruct(),L"ComponentContainers"));
 if(!containers||containers->GetInner()->GetElementSize()!=sizeof(Instanced))throw std::runtime_error("Native host array ABI differs");
 auto* array=containers->ContainerPtrToValuePtr<FScriptArray>(host->ContainerPtrToValuePtr<void>(object));
 auto* values=static_cast<Instanced*>(array->GetData());
 for(int i=0;i<array->Num();++i)if(values[i].type&&values[i].data&&values[i].type->IsChildOf(desiredType))return values+i;
 return nullptr;
}
template<class F>void MapPairs(FMapProperty* property,void* owner,F visit){
 auto* map=property->ContainerPtrToValuePtr<FScriptMap>(owner);auto* key=property->GetKeyProp();auto* value=property->GetValueProp();
 auto layout=FScriptMap::GetScriptLayout(key->GetSize(),key->GetMinAlignment(),value->GetSize(),value->GetMinAlignment());
 for(int i=0;i<map->GetMaxIndex();++i)if(map->IsValidIndex(i)){auto* pair=static_cast<unsigned char*>(map->GetData(i,layout));visit(pair,pair+layout.ValueOffset);}
}
}
