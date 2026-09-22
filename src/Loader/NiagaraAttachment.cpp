#include "Loader/NiagaraAttachment.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Helper/ActorHelper.h"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Helpers/Casting.hpp"
#include "Helpers/String.hpp"
#include <Windows.h>
#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>

using namespace RC;
using namespace RC::Unreal;
namespace DragonWilds::NiagaraAttachment {
namespace {
struct RuntimeLinearColor { float R; float G; float B; float A; };
struct RuntimeVector { double X; double Y; double Z; };
struct RuntimeRotator { double Pitch; double Yaw; double Roll; };
static_assert(sizeof(RuntimeLinearColor)==16);
static_assert(sizeof(RuntimeVector)==24);
static_assert(sizeof(RuntimeRotator)==24);
template<class T=UObject*> T Find(const TCHAR* path) {
    return UECustom::UObjectGlobals::StaticFindObject<T>(nullptr,nullptr,path,false);
}
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
class Call {
    UFunction* function{};
    std::vector<uint8_t> data;
public:
    explicit Call(const TCHAR* path):function(Find<UFunction*>(path)) {
        Require(function,"Niagara attachment function was unavailable");
        Require(function->GetParmsSize()<=512,"Niagara attachment parameter layout was too large");
        data.resize(function->GetParmsSize());
    }
    FProperty* Field(const TCHAR* name,size_t size) {
        auto* field=function->FindProperty(FName(name,FNAME_Find));
        const bool valid=field && field->HasAnyPropertyFlags(CPF_Parm)
            && field->GetArrayDim()==1 && field->GetElementSize()==size
            && field->GetOffset_Internal()>=0
            && static_cast<size_t>(field->GetOffset_Internal())+size<=data.size();
        Require(valid,"Niagara attachment parameter layout was unsupported");
        return field;
    }
    void Object(const TCHAR* name,UObject* value) {
        auto* field=CastField<FObjectProperty>(Field(name,sizeof(UObject*)));
        Require(field,"Niagara attachment object parameter was unsupported");
        std::memcpy(data.data()+field->GetOffset_Internal(),&value,sizeof(value));
    }
    void Bool(const TCHAR* name,bool value) {
        auto* field=CastField<FBoolProperty>(Field(name,sizeof(bool)));
        Require(field && field->IsNativeBool() && field->GetByteOffset()==0,
            "Niagara attachment boolean parameter was unsupported");
        field->SetPropertyValue(data.data()+field->GetOffset_Internal(),value);
    }
    void Byte(const TCHAR* name) { (void)Field(name,1); }
    void Name(const TCHAR* name,const TCHAR* value) {
        auto* field=CastField<FNameProperty>(Field(name,sizeof(FName)));
        Require(field,"Niagara attachment name parameter was unsupported");
        const FName socket(value,FNAME_Add);
        std::memcpy(data.data()+field->GetOffset_Internal(),&socket,sizeof(socket));
    }
    template<class T> void Struct(const TCHAR* name,const T& value,const TCHAR* type) {
        auto* field=CastField<FStructProperty>(Field(name,sizeof(T)));
        Require(field && field->GetStruct().Get() && field->GetStruct()->GetFName()==FName(type,FNAME_Add),
            "Niagara attachment transform parameter was unsupported");
        std::memcpy(data.data()+field->GetOffset_Internal(),&value,sizeof(value));
    }
    FProperty* ReturnObject(const TCHAR* name) {
        auto* field=Field(name,sizeof(UObject*));
        Require(field->HasAnyPropertyFlags(CPF_ReturnParm),"Niagara attachment return value was unsupported");
        return field;
    }
    UObject* Result(FProperty* field) const {
        UObject* value{};
        std::memcpy(&value,data.data()+field->GetOffset_Internal(),sizeof(value));
        return value;
    }
    void Invoke(UObject* self) { Require(self,"Niagara attachment owner was unavailable");self->ProcessEvent(function,data.data()); }
};
}
bool CanRenderLocally() {
    static const bool available=[] {
        std::array<wchar_t,32768> path{};
        const auto length=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
        return length && length<path.size()
            && !std::wstring_view(path.data(),length).ends_with(L"RSDragonwildsServer-Win64-Shipping.exe");
    }();
    return available;
}
void ApplyParameters(UObject* component,const nlohmann::json& effect) {
    if(!component || !effect.contains("Parameters"))return;
    for(const auto& [name,value]:effect.at("Parameters").items()) {
        const auto parameter=to_generic_string(name);
        const auto variable=FName(parameter.c_str(),FNAME_Add);
        if(value.is_boolean()) {
            ActorHelper::FunctionCall(component,TEXT("/Script/Niagara.NiagaraComponent:SetVariableBool"))
                .Arg(TEXT("InVariableName"),variable).Arg(TEXT("InValue"),value.get<bool>()).Invoke();
        } else if(value.is_number_integer()) {
            ActorHelper::FunctionCall(component,TEXT("/Script/Niagara.NiagaraComponent:SetVariableInt"))
                .Arg(TEXT("InVariableName"),variable).Arg(TEXT("InValue"),value.get<int32_t>()).Invoke();
        } else if(value.is_number()) {
            ActorHelper::FunctionCall(component,TEXT("/Script/Niagara.NiagaraComponent:SetVariableFloat"))
                .Arg(TEXT("InVariableName"),variable).Arg(TEXT("InValue"),value.get<float>()).Invoke();
        } else if(value.is_object() && value.contains("R")) {
            const RuntimeLinearColor color{value.at("R").get<float>(),value.at("G").get<float>(),
                value.at("B").get<float>(),value.at("A").get<float>()};
            ActorHelper::FunctionCall(component,TEXT("/Script/Niagara.NiagaraComponent:SetVariableLinearColor"))
                .Arg(TEXT("InVariableName"),variable).Arg(TEXT("InValue"),color).Invoke();
        } else {
            const RuntimeVector vector{value.at("X").get<double>(),value.at("Y").get<double>(),
                value.at("Z").get<double>()};
            ActorHelper::FunctionCall(component,TEXT("/Script/Niagara.NiagaraComponent:SetVariableVec3"))
                .Arg(TEXT("InVariableName"),variable).Arg(TEXT("InValue"),vector).Invoke();
        }
    }
}
void ApplyEmitters(UObject* component,const nlohmann::json& effect) {
    if(!component || !effect.contains("Emitters"))return;
    for(const auto& [name,enabled]:effect.at("Emitters").items()) {
        const auto emitter=FName(to_generic_string(name).c_str(),FNAME_Add);
        ActorHelper::FunctionCall(component,TEXT("/Script/Engine.FXSystemComponent:SetEmitterEnable"))
            .Arg(TEXT("EmitterName"),emitter).Arg(TEXT("bNewEnableState"),enabled.get<bool>()).Invoke();
    }
}
void Destroy(UObject* component);
UObject* Attach(UObject* owner,UObject* sceneComponent,const nlohmann::json& effect) {
    if(!CanRenderLocally())return nullptr;
    Require(owner && sceneComponent,"Niagara attachment target was unavailable");
    auto* sceneClass=Find<UClass*>(TEXT("/Script/Engine.SceneComponent"));
    auto* systemClass=Find<UClass*>(TEXT("/Script/Niagara.NiagaraSystem"));
    Require(sceneClass && sceneComponent->IsA(sceneClass),"Niagara attachment target was not a scene component");
    const auto systemPath=effect.at("System").get<std::string>();
    auto* system=ActorHelper::ResolveObject(to_generic_string(systemPath));
    Require(system && systemClass && system->IsA(systemClass),"Niagara system could not be resolved");
    auto* library=Find(TEXT("/Script/Niagara.Default__NiagaraFunctionLibrary"));
    Require(library,"Niagara function library was unavailable");
    Call spawn(TEXT("/Script/Niagara.NiagaraFunctionLibrary:SpawnSystemAttached"));
    spawn.Object(TEXT("SystemTemplate"),system);
    spawn.Object(TEXT("AttachToComponent"),sceneComponent);
    const auto socket=effect.value("Socket",std::string("None"));
    spawn.Name(TEXT("AttachPointName"),to_generic_string(socket).c_str());
    const auto& location=effect.value("LocationOffset",nlohmann::json::object());
    const auto& rotation=effect.value("RotationOffset",nlohmann::json::object());
    const RuntimeVector locationOffset{location.value("X",0.0),location.value("Y",0.0),location.value("Z",0.0)};
    const RuntimeRotator rotationOffset{rotation.value("Pitch",0.0),rotation.value("Yaw",0.0),rotation.value("Roll",0.0)};
    spawn.Struct(TEXT("Location"),locationOffset,TEXT("Vector"));
    spawn.Struct(TEXT("Rotation"),rotationOffset,TEXT("Rotator"));
    spawn.Byte(TEXT("LocationType"));
    spawn.Bool(TEXT("bAutoDestroy"),true);
    spawn.Bool(TEXT("bAutoActivate"),effect.value("AutoActivate",true));
    spawn.Byte(TEXT("PoolingMethod"));
    spawn.Bool(TEXT("bPreCullCheck"),false);
    auto* result=spawn.ReturnObject(TEXT("ReturnValue"));
    spawn.Invoke(library);
    auto* component=spawn.Result(result);
    if(component) {
        try { ApplyEmitters(component,effect);ApplyParameters(component,effect); }
        catch(...) { try { Destroy(component); } catch(...) {} throw; }
    }
    return component;
}
void Destroy(UObject* component) {
    if(!component)return;
    Call destroy(TEXT("/Script/Engine.ActorComponent:K2_DestroyComponent"));
    auto* object=destroy.Field(TEXT("Object"),sizeof(UObject*));
    (void)object;destroy.Object(TEXT("Object"),component);destroy.Invoke(component);
}
}
