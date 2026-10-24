#pragma once
#include "SDK/Helper/EquipmentDelivery.h"
#include <Unreal/UObjectGlobals.hpp>
namespace PS {
struct NativeInventoryWidgets {
    using Object=RC::Unreal::UObject;
    static bool Alive(Object* object){using namespace RC::Unreal;return object&&!object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed));}
    static Object* Ref(Object* object,const TCHAR* name){
        using namespace RC::Unreal;namespace E=EquipmentDelivery;if(!Alive(object))return nullptr;
        auto* p=CastField<FObjectProperty>(object->GetClassPrivate()->GetPropertyByNameInChain(name));
        if(!E::C::Fits(p,object->GetClassPrivate()->GetPropertiesSize())||p->GetSize()!=sizeof(Object*)||!p->GetPropertyClass())throw std::runtime_error("Native inventory widget field changed");
        auto* value=p->GetObjectPropertyValue(p->ContainerPtrToValuePtr<void>(object));
        if(value&&(!Alive(value)||!value->IsA(p->GetPropertyClass().Get())))throw std::runtime_error("Native inventory widget reference unavailable");return value;
    }
    static bool Bool(Object* object,const TCHAR* name){
        using namespace RC::Unreal;auto* p=Alive(object)?CastField<FBoolProperty>(object->GetClassPrivate()->GetPropertyByNameInChain(name)):nullptr;
        if(!p||!EquipmentDelivery::C::Fits(p,object->GetClassPrivate()->GetPropertiesSize()))throw std::runtime_error("Native inventory widget flag changed");return p->GetPropertyValue(p->ContainerPtrToValuePtr<void>(object));
    }
    static void Remove(Object* object,bool detach=true)noexcept{if(!Alive(object))return;if(detach)try{EquipmentDelivery::Call(object,TEXT("/Script/UMG.Widget:RemoveFromParent"),0).Invoke();}catch(...){}if(object->IsRootSet())object->ClearRootSet();}
    static Object* Node(Object* tree,const TCHAR* path){
        using namespace RC::Unreal;auto* type=EquipmentDelivery::AH::ResolveClass(path);if(!type)throw std::runtime_error("Native action widget class unavailable");
        FStaticConstructObjectParameters parameters(type,tree);parameters.SetFlags=RF_Transient;
        auto* result=UObjectGlobals::StaticConstructObject<Object*>(parameters);if(!result)throw std::runtime_error("Native action widget could not be constructed");return result;
    }
    static void CopyStyle(Object* target,const TCHAR* method,const TCHAR* argument,Object* source,const TCHAR* field,const TCHAR* expected,const nlohmann::json& patch=nlohmann::json::object()){
        using namespace RC::Unreal;namespace E=EquipmentDelivery;
        auto* p=source?CastField<FStructProperty>(source->GetClassPrivate()->GetPropertyByNameInChain(field)):nullptr;
        if(!p||!E::C::Fits(p,source->GetClassPrivate()->GetPropertiesSize())||!p->GetStruct()||p->GetStruct()->GetPathName()!=expected)throw std::runtime_error("Native action style contract changed");
        E::AH::FunctionCall call(target,method);call.StructArg(argument,p,p->ContainerPtrToValuePtr<void>(source),patch).Invoke();
    }
    static Object* Add(Object* parent,Object* child){EquipmentDelivery::Call add(parent,TEXT("/Script/UMG.PanelWidget:AddChild"),2);add.Object(TEXT("Content"),child).Invoke();auto* result=add.Object();if(!result)throw std::runtime_error("Native action layout rejected its child");return result;}
};
}
