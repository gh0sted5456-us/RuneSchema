#include "Utility/NativeFunctionHook.h"
#include "Generator/NiagaraTest.h"
#include "Generator/NiagaraPreset.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/WeakObjectHandle.h"
#include "SDK/WeakObjectHandle.h"
#include "Unreal/Hooks.hpp"
#include "Unreal/AActor.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/Property/FEnumProperty.hpp"
#include "Helpers/Casting.hpp"
#include "Helpers/String.hpp"
#include "Utility/Logging.h"
#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <cmath>

using namespace RC;
using namespace RC::Unreal;
namespace PS::NiagaraTest {
namespace {
struct RuntimeVector { double X; double Y; double Z; };
struct RuntimeRotator { double Pitch; double Yaw; double Roll; };
static_assert(sizeof(RuntimeVector)==24 && sizeof(RuntimeRotator)==24);
PS::WeakObjectHandle component, player, owner;
PS::WeakObjectHandle expectedParent;
nlohmann::json activePreset;
struct HookEntry { PS::WeakObjectHandle function; int32 id{}; };
std::array<HookEntry,1> hooks;
Hook::GlobalCallbackId endPlayHook=Hook::ERROR_ID;
template<class T=UObject*> T Find(const TCHAR* path) {
    return UECustom::UObjectGlobals::StaticFindObject<T>(nullptr,nullptr,path,false);
}
void Require(bool condition) {
    if(!condition)throw std::runtime_error("Niagara test unavailable: unsupported live function layout or object type.");
}
bool Is(UObject* object,const TCHAR* path) {
    auto* type=Find<UClass*>(path);
    return object && type && object->IsA(type);
}
struct Call {
    UFunction* function;
    std::vector<uint8_t> data;
    unsigned fields{};
    explicit Call(const TCHAR* path):function(Find<UFunction*>(path)) {
        if(!function)throw std::runtime_error("Niagara test: function unavailable: "+to_string(StringType(path)));
        Require(function && function->GetParmsSize()<=512);
        data.resize(function->GetParmsSize());
        for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::None))
            if(field && field->HasAnyPropertyFlags(CPF_Parm))++fields;
    }
    FProperty* Field(const TCHAR* name,size_t size) {
        auto* field=function->FindProperty(FName(name,FNAME_Find));
        const bool valid=field && field->HasAnyPropertyFlags(CPF_Parm) && field->GetArrayDim()==1
            && field->GetElementSize()==size && field->GetOffset_Internal()>=0
            && static_cast<size_t>(field->GetOffset_Internal())+size<=data.size();
        if(!valid)throw std::runtime_error("Niagara test: unsupported parameter: "+to_string(StringType(name)));
        return field;
    }
    void Object(const TCHAR* name,UObject* value) {
        auto* field=CastField<FObjectProperty>(Field(name,sizeof(UObject*)));
        Require(field!=nullptr);
        std::memcpy(data.data()+field->GetOffset_Internal(),&value,sizeof(value));
    }
    void Bool(const TCHAR* name,bool value) {
        auto* field=CastField<FBoolProperty>(Field(name,sizeof(bool)));
        Require(field && field->IsNativeBool() && field->GetByteOffset()==0);
        field->SetPropertyValue(data.data()+field->GetOffset_Internal(),value);
    }
    void Byte(const TCHAR* name) {
        auto* field=Field(name,1);
        if(CastField<FByteProperty>(field))return;
        auto* value=CastField<FEnumProperty>(field);
        Require(value && CastField<FByteProperty>(value->GetUnderlyingProp()));
    }
    void ZeroStruct(const TCHAR* name,const TCHAR* type) {
        auto* field=CastField<FStructProperty>(Field(name,24));
        Require(field && field->GetStruct().Get() && field->GetStruct()->GetFName()==FName(type,FNAME_Add)
            && field->GetStruct()->GetPropertiesSize()==24);
    }
    template<class T> void Struct(const TCHAR* name,const T& value,const TCHAR* type) {
        auto* field=CastField<FStructProperty>(Field(name,sizeof(T)));
        Require(field && field->GetStruct().Get() && field->GetStruct()->GetFName()==FName(type,FNAME_Add)
            && field->GetStruct()->GetPropertiesSize()==sizeof(T));
        std::memcpy(data.data()+field->GetOffset_Internal(),&value,sizeof(value));
    }
    void Name(const TCHAR* name,const TCHAR* value=TEXT("None")) {
        auto* field=CastField<FNameProperty>(Field(name,sizeof(FName)));
        Require(field!=nullptr);
        FName none(value,FNAME_Add);
        std::memcpy(data.data()+field->GetOffset_Internal(),&none,sizeof(none));
    }
    void Invoke(UObject* self) { Require(self!=nullptr);self->ProcessEvent(function,data.data()); }
};
UObject* ObjectResult(UObject* object,const TCHAR* path) {
    Call call(path);
    Require(call.fields==1);
    call.Object(TEXT("ReturnValue"),nullptr);
    auto* field=call.Field(TEXT("ReturnValue"),sizeof(UObject*));
    Require(field->HasAnyPropertyFlags(CPF_ReturnParm));
    call.Invoke(object);
    UObject* result{};
    std::memcpy(&result,call.data.data()+field->GetOffset_Internal(),sizeof(result));
    return result;
}
nlohmann::json Position(UObject* object) {
    Call call(TEXT("/Script/Engine.SceneComponent:K2_GetComponentLocation"));
    Require(call.fields==1);
    call.ZeroStruct(TEXT("ReturnValue"),TEXT("Vector"));
    auto* field=call.Field(TEXT("ReturnValue"),24);
    Require(field->HasAnyPropertyFlags(CPF_ReturnParm));
    call.Invoke(object);
    std::array<double,3> xyz{};
    std::memcpy(xyz.data(),call.data.data()+field->GetOffset_Internal(),24);
    for(auto value:xyz)Require(std::isfinite(value));
    return {{"X",xyz[0]},{"Y",xyz[1]},{"Z",xyz[2]}};
}
void ConfigureEmitter(Call& call,const TCHAR* name,bool enabled) {
    Require(call.fields==2);
    call.Name(TEXT("EmitterName"),name);
    call.Bool(TEXT("bNewEnableState"),enabled);
}
void ValidateDestroy(Call& call) {
    Require(call.fields==1);
    call.Object(TEXT("Object"),player.Get());
}
void BindCleanup() {
    if(endPlayHook==Hook::ERROR_ID) {
        Hook::FCallbackOptions options{};
        options.OwnerModName=TEXT("RuneSchema");
        options.HookName=TEXT("NiagaraTestEndPlay");
        endPlayHook=Hook::RegisterEndPlayPreCallback(
            [](Hook::TCallbackIterationData<void>&,AActor* actor,EEndPlayReason) {
                if(actor!=player.Get() && actor!=owner.Get())return;
                try { Remove(); }
                catch(const std::exception&) {
                    PS::Log<LogLevel::Warning>(TEXT("Niagara test end-play cleanup failed.\n"));
                }
            },options);
        Require(endPlayHook!=Hook::ERROR_ID);
    }
    const TCHAR* paths[]={TEXT("/Script/Engine.PlayerController:ClientRestart")};
    for(size_t i=0;i<hooks.size();++i) {
        if(hooks[i].id)continue;
        auto* function=Find<UFunction*>(paths[i]);
        Require(function!=nullptr);
        const auto id=PS::RegisterNativePreHook(function, [](UnrealScriptFunctionCallableContext& context,void*) {
            if(context.Context!=player.Get() && context.Context!=owner.Get())return;
            try { Remove(); }
            catch(const std::exception&) { PS::Log<LogLevel::Warning>(TEXT("Niagara test cleanup failed; restart the world.\n")); }
        });
        Require(id!=0);
        hooks[i]={PS::WeakObject(function),id};
    }
}
}
void Remove() {
    auto* value=component.Get();
    if(!value)return;
    Call destroy(TEXT("/Script/Engine.ActorComponent:K2_DestroyComponent"));
    ValidateDestroy(destroy);
    component.Reset();
    destroy.Invoke(value);
}
void Reset() {
    Remove();
    player.Reset();
    owner.Reset();
    expectedParent.Reset();
    activePreset=nullptr;
    Unbind();
}
void Unbind() {
    if(endPlayHook!=Hook::ERROR_ID)Hook::UnregisterCallback(endPlayHook);
    endPlayHook=Hook::ERROR_ID;
    for(auto& hook:hooks) {
        if(auto* function=static_cast<UFunction*>(hook.function.Get());function && hook.id)
            function->UnregisterHook(hook.id);
        hook={};
    }
}
std::string Attach(UObject* controller,UObject* pawn,const nlohmann::json& input) {
    const auto preset=NiagaraPreset::Validate(input);
    Require(Is(controller,TEXT("/Script/Engine.PlayerController")) && Is(pawn,TEXT("/Script/Engine.Pawn")));
    const bool mesh=preset["Target"]=="PlayerMesh";
    auto* rootField=CastField<FObjectProperty>(DragonWilds::PropertyHelper::GetPropertyByName(pawn->GetClassPrivate(),mesh?TEXT("Mesh"):TEXT("RootComponent")));
    Require(rootField && rootField->GetArrayDim()==1 && rootField->GetElementSize()==sizeof(UObject*));
    auto* root=rootField->GetObjectPropertyValue(rootField->ContainerPtrToValuePtr<void>(pawn));
    Require(Is(root,TEXT("/Script/Engine.SceneComponent")));
    const auto socket=to_generic_string(preset["Socket"].get<std::string>());
    if(preset["Socket"]!="None") {
        Call exists(TEXT("/Script/Engine.SceneComponent:DoesSocketExist"));
        Require(exists.fields==2);
        exists.Name(TEXT("InSocketName"),socket.c_str());exists.Bool(TEXT("ReturnValue"),false);
        auto* field=CastField<FBoolProperty>(exists.Field(TEXT("ReturnValue"),sizeof(bool)));
        Require(field->HasAnyPropertyFlags(CPF_ReturnParm));
        exists.Invoke(root);
        if(!field->GetPropertyValue(exists.data.data()+field->GetOffset_Internal()))
            throw std::runtime_error("Niagara preset socket/bone was not found on the player mesh.");
    }
    if(!preset["Emitters"].empty()) {
        Call emitter(TEXT("/Script/Engine.FXSystemComponent:SetEmitterEnable"));
        ConfigureEmitter(emitter,TEXT("None"),false);
    }
    Call spawn(TEXT("/Script/Niagara.NiagaraFunctionLibrary:SpawnSystemAttached"));
    Require(spawn.fields==11);
    spawn.Object(TEXT("SystemTemplate"),nullptr);
    spawn.Object(TEXT("AttachToComponent"),root);
    spawn.Name(TEXT("AttachPointName"),socket.c_str());
    const auto& location=preset["LocationOffset"];
    const auto& rotation=preset["RotationOffset"];
    const RuntimeVector locationOffset{location["X"].get<double>(),location["Y"].get<double>(),location["Z"].get<double>()};
    const RuntimeRotator rotationOffset{rotation["Pitch"].get<double>(),rotation["Yaw"].get<double>(),rotation["Roll"].get<double>()};
    spawn.Struct(TEXT("Location"),locationOffset,TEXT("Vector"));
    spawn.Struct(TEXT("Rotation"),rotationOffset,TEXT("Rotator"));
    spawn.Byte(TEXT("LocationType")); // KeepRelativeOffset.
    spawn.Bool(TEXT("bAutoDestroy"),true);
    spawn.Bool(TEXT("bAutoActivate"),true);
    spawn.Byte(TEXT("PoolingMethod")); // None: this test owns destruction.
    spawn.Bool(TEXT("bPreCullCheck"),false);
    spawn.Object(TEXT("ReturnValue"),nullptr);
    Call destroy(TEXT("/Script/Engine.ActorComponent:K2_DestroyComponent"));
    ValidateDestroy(destroy);
    auto* library=Find(TEXT("/Script/Niagara.Default__NiagaraFunctionLibrary"));
    Require(Is(library,TEXT("/Script/Niagara.NiagaraFunctionLibrary")));
    auto* system=DragonWilds::ActorHelper::ResolveObject(to_generic_string(preset["System"].get<std::string>()));
    Require(Is(system,TEXT("/Script/Niagara.NiagaraSystem")));
    Reset();
    try { BindCleanup(); } catch(...) { Reset();throw; }
    player=PS::WeakObject(pawn);owner=PS::WeakObject(controller);
    expectedParent=PS::WeakObject(root);activePreset=preset;
    spawn.Object(TEXT("SystemTemplate"),system);
    spawn.Invoke(library);
    UObject* created{};
    auto* result=spawn.Field(TEXT("ReturnValue"),sizeof(UObject*));
    std::memcpy(&created,spawn.data.data()+result->GetOffset_Internal(),sizeof(created));
    if(!created) {Reset();return "Niagara test: no component created. The effect may be unavailable in this world.";}
    component=PS::WeakObject(created);
    Require(Is(created,TEXT("/Script/Niagara.NiagaraComponent")));
    if(!preset["Emitters"].empty()) {
        try {
            Call emitter(TEXT("/Script/Engine.FXSystemComponent:SetEmitterEnable"));
            for(const auto& [name,enabled]:preset["Emitters"].items()) {
                ConfigureEmitter(emitter,to_generic_string(name).c_str(),enabled.get<bool>());emitter.Invoke(created);
            }
        } catch(...) { Reset();throw; }
    }
    return "Test spawned: "+preset["Name"].get<std::string>()+". Emitter changes requested; allow old particles to fade. Capture attachment to verify. Nothing saved to the game.";
}
nlohmann::json Inspect() {
    using nlohmann::json;
    auto path=[](UObject* value)->json {return value?json(to_string(value->GetPathName())):json(nullptr);};
    auto* value=component.Get();
    json report={{"Kind","RuneSchemaNiagaraAttachment"},{"Version",1},
        {"Component",path(value)},{"Player",path(player.Get())},
        {"ExpectedParent",path(expectedParent.Get())},{"Preset",activePreset},
        {"Note","Manual snapshot; emitter enable requests are not read-back verification. Particle positions and continuous motion are not captured."}};
    if(!value) {report["Status"]="No live test component. Attach a test first.";return report;}
    auto capture=[&](const char* key,auto read) {
        try {report[key]=read();}
        catch(const std::exception& error) {report[key]={{"Error",error.what()}};}
    };
    capture("Attachment",[&] {
        auto* parent=ObjectResult(value,TEXT("/Script/Engine.SceneComponent:GetAttachParent"));
        return json{{"ActualParent",path(parent)},
            {"MatchesExpectedParent",parent && parent==expectedParent.Get()}};
    });
    capture("ComponentOwner",[&] {return path(ObjectResult(value,TEXT("/Script/Engine.ActorComponent:GetOwner")));});
    capture("ComponentWorldPosition",[&] {return Position(value);});
    capture("ExpectedParentWorldPosition",[&] {return Position(expectedParent.Get());});
    return report;
}
}
