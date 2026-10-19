#include "Generator/CharacterPreview.h"
#include "Core/CharacterWorkbench.h"
#include "Core/CharacterPreviewFrame.h"
#include "SDK/WeakObjectHandle.h"
#include "SDK/Structs/FSoftObjectPtr.h"
#include "SDK/Helper/PropertyHelper.h"
#include "Unreal/Property/FEnumProperty.hpp"
#include "Unreal/UEnum.hpp"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Structs/Custom/FScriptMapHelper.h"
#include "Generator/AssetSearch.h"
#include <Unreal/Property/FMapProperty.hpp>
#include <Unreal/FText.hpp>
#include <Unreal/Property/FTextProperty.hpp>
#include <Unreal/CoreUObject/UObject/FStrProperty.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/UObjectArray.hpp>
#include <Unreal/UFunctionStructs.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/AActor.hpp>
#include <Unreal/World.hpp>
#include <Helpers/Casting.hpp>
#include <Helpers/String.hpp>
#include <map>
#include <memory>
#include <chrono>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <tuple>
#include <charconv>
#include <set>
using namespace RC;using namespace RC::Unreal;using Json=nlohmann::json;
namespace PS::CharacterPreview {namespace {
UObject* Find(const wchar_t* path){return UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,path);}
struct Call {
    UObject* Self;UFunction* Fn;std::vector<uint8_t> Bytes;std::vector<FProperty*> Fields;
    Call(UObject* self,const wchar_t* path):Self(self),Fn(UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path)){
        if(!Self||!Fn||!Fn->HasAnyFunctionFlags(FUNC_Native)||Fn->HasAnyFunctionFlags(FUNC_Net)||Fn->GetParmsSize()>65536)throw std::runtime_error("Missing/non-native/networked function: "+to_string(StringType(path)));
        auto* owner=Fn->GetOuterPrivate();if(!owner||!owner->IsA<UClass>()||!Self->IsA(static_cast<UClass*>(owner)))throw std::runtime_error("Function owner mismatch");
        Bytes.resize(std::max<int>(Fn->GetParmsSize(),1));
        try{for(auto* p:TFieldRange<FProperty>(Fn,EFieldIterationFlags::Default))if(p->HasAnyPropertyFlags(CPF_Parm)){
            if(p->GetOffset_Internal()<0||p->GetSize()<1||p->GetOffset_Internal()+p->GetSize()>Fn->GetParmsSize())throw std::runtime_error("Invalid reflected extent");p->InitializeValue_InContainer(Bytes.data());Fields.push_back(p);
        }}catch(...){Destroy();throw;}
    }
    ~Call(){Destroy();}void Destroy(){for(auto it=Fields.rbegin();it!=Fields.rend();++it)(*it)->DestroyValue_InContainer(Bytes.data());Fields.clear();}
    FProperty* Prop(const wchar_t* name){auto* p=Fn->FindProperty(FName(name,FNAME_Find));if(!p||!p->HasAnyPropertyFlags(CPF_Parm)||p->GetOffset_Internal()<0||p->GetOffset_Internal()+p->GetSize()>Fn->GetParmsSize())throw std::runtime_error("Missing parameter "+to_string(StringType(name)));return p;}
    void* Value(const wchar_t* name){return Prop(name)->ContainerPtrToValuePtr<void>(Bytes.data());}
    Call& Text(const wchar_t* name,const StringType& value){auto* p=Prop(name);if(!p->ImportText_Direct(value.c_str(),Value(name),Self,0,nullptr))throw std::runtime_error("Rejected parameter "+to_string(StringType(name)));return *this;}
    Call& Obj(const wchar_t* name,UObject* value){auto* p=CastField<FObjectProperty>(Prop(name));if(!p||p->GetSize()!=sizeof(value)||value&&(!p->GetPropertyClass()||!value->IsA(p->GetPropertyClass().Get())))throw std::runtime_error("Object parameter changed");std::memcpy(Value(name),&value,sizeof(value));return *this;}
    void Invoke(){Self->ProcessEvent(Fn,Bytes.data());}
    UObject* Object(const wchar_t* name=L"ReturnValue"){auto* p=CastField<FObjectPropertyBase>(Prop(name));if(!p)throw std::runtime_error("Expected object result");return p->GetObjectPropertyValue(Value(name));}
    bool Bool(){auto* p=CastField<FBoolProperty>(Prop(L"ReturnValue"));if(!p)throw std::runtime_error("Expected bool return");return p->GetPropertyValue(Value(L"ReturnValue"));}
    int Integer(){auto* p=CastField<FIntProperty>(Prop(L"ReturnValue"));if(!p)throw std::runtime_error("Expected native int result");return *static_cast<int32_t*>(Value(L"ReturnValue"));}
};
UObject* Load(const StringType& path){if(auto* object=Find(path.c_str()))return object;Call c(Find(L"/Script/Engine.Default__KismetSystemLibrary"),L"/Script/Engine.KismetSystemLibrary:LoadAsset_Blocking");c.Text(L"Asset",path).Invoke();auto* object=c.Object();if(!object)throw std::runtime_error("Asset load returned null: "+to_string(path));return object;}
FProperty* Member(UObject* object,const wchar_t* name){if(!object||!object->GetClassPrivate())throw std::runtime_error("Missing object");auto* p=object->GetClassPrivate()->GetPropertyByNameInChain(name);if(!p||p->GetArrayDim()!=1||p->GetOffset_Internal()<0||p->GetSize()<1||p->GetOffset_Internal()+p->GetSize()>object->GetClassPrivate()->GetPropertiesSize())throw std::runtime_error("Missing member "+to_string(StringType(name)));return p;}
UObject* ObjectMember(UObject* object,const wchar_t* name){auto* p=CastField<FObjectPropertyBase>(Member(object,name));if(!p)throw std::runtime_error("Expected object member");return p->GetObjectPropertyValue(p->ContainerPtrToValuePtr<void>(object));}
void TextMember(UObject* object,const wchar_t* name,const StringType& value){auto* p=Member(object,name);if(!p->ImportText_Direct(value.c_str(),p->ContainerPtrToValuePtr<void>(object),object,0,nullptr))throw std::runtime_error("Member import failed");}
void ObjMember(UObject* object,const wchar_t* name,UObject* value){auto* p=CastField<FObjectProperty>(Member(object,name));if(!p||value&&(!p->GetPropertyClass()||!value->IsA(p->GetPropertyClass().Get())))throw std::runtime_error("Object member mismatch");if(p->GetSize()!=sizeof(value))throw std::runtime_error("Object member extent changed");std::memcpy(p->ContainerPtrToValuePtr<void>(object),&value,sizeof(value));}
StringType Vector(double x,double y,double z){return L"(X="+std::to_wstring(x)+L",Y="+std::to_wstring(y)+L",Z="+std::to_wstring(z)+L")";}
StringType Transform(double x,double y,double z){return L"(Rotation=(X=0,Y=0,Z=0,W=1),Translation="+Vector(x,y,z)+L",Scale3D=(X=1,Y=1,Z=1))";}

using Clock=std::chrono::steady_clock;
CharacterPreviewFrame::Channel channel;
PS::WeakObjectHandle worldLease,targetLease,componentLease;
std::vector<PS::WeakObjectHandle> actors;
UObject *World=nullptr,*Preview=nullptr,*Capture=nullptr,*Target=nullptr,*Component=nullptr;
PS::WeakObjectHandle itemVisual;
std::array<PS::WeakObjectHandle,2> rigLights;
constexpr std::array PreviewMeshes{L"InvisMeshComponent",L"InvisHairMeshComponent",L"BodyMeshComponent",L"HeadMeshComponent",L"HairZone1MeshComponent",L"FacialHairZone1MeshComponent",L"BodyDefaultOutfitMeshComponent",L"LegsDefaultOutfitMeshComponent",L"HeadOutfitMeshComponent",L"BodyOutfitMeshComponent",L"LegsOutfitMeshComponent",L"CapeOutfitMeshComponent",L"TrinketOutfitMeshComponent",L"OffhandPropMeshComponent"};
std::string mode,lastAppearance,lastEquipment,lastItem;
bool pending=false,dirty=true;int lastAngle=999,lastPitch=999,lastZoom=0,followups=0;
Clock::time_point due{},next{},followAt{};
Clock::time_point freezeAt{};
bool awaitingPose=false;
int lastAnimation=-1,lastPlaying=-1;
bool bodyFramed=false;double bodyCenter=1090.,bodyDistance=480.;
uint64_t session=0;
struct Cached {std::vector<unsigned char> Pixels;uint64_t Used=0;};
std::map<std::tuple<int,int,int>,Cached> angles;size_t cacheBytes=0;uint64_t cacheClock=0;
std::vector<unsigned char> pixels;
void Invalidate(){angles.clear();cacheBytes=0;lastAngle=lastPitch=999;dirty=true;}
void Status(const std::string& value){if(channel.Lock()){auto* data=channel.Get();std::snprintf(data->Status,sizeof(data->Status),"%s",value.c_str());channel.Unlock();}}
void DestroyPreviewActor(){
    if(Preview){auto* old=Preview;std::erase_if(actors,[old](auto& lease){return lease.Get()==old;});Call(old,L"/Script/Engine.Actor:K2_DestroyActor").Invoke();}
    Preview=nullptr;itemVisual.Reset();pending=false;followups=0;awaitingPose=false;bodyFramed=false;lastAnimation=lastPlaying=-1;lastAppearance.clear();lastEquipment.clear();Invalidate();
}
void DestroyScene(){
    if(auto* target=targetLease.Get()){try{Call(Find(L"/Script/Engine.Default__KismetRenderingLibrary"),L"/Script/Engine.KismetRenderingLibrary:ReleaseRenderTarget2D").Obj(L"TextureRenderTarget",target).Invoke();if(target->IsRootSet())target->ClearRootSet();}catch(...){}}
    targetLease.Reset();componentLease.Reset();itemVisual.Reset();for(auto& light:rigLights)light.Reset();
    for(auto it=actors.rbegin();it!=actors.rend();++it)if(auto* actor=it->Get())try{Call(actor,L"/Script/Engine.Actor:K2_DestroyActor").Invoke();}catch(...){}
    actors.clear();worldLease.Reset();World=Preview=Capture=Target=Component=nullptr;pending=false;followups=0;awaitingPose=false;bodyFramed=false;lastAnimation=lastPlaying=-1;lastAppearance.clear();lastEquipment.clear();lastItem.clear();mode.clear();Invalidate();
}
UObject* Spawn(const wchar_t* path,double x,double y,double z){
    auto* cls=Load(path);if(!cls||!cls->IsA<UClass>())throw std::runtime_error("Native preview class unavailable");
    Call begin(Find(L"/Script/Engine.Default__GameplayStatics"),L"/Script/Engine.GameplayStatics:BeginDeferredActorSpawnFromClass");
    begin.Obj(L"WorldContextObject",World).Obj(L"ActorClass",cls).Text(L"SpawnTransform",Transform(x,y,z)).Text(L"CollisionHandlingOverride",L"AlwaysSpawn").Text(L"TransformScaleMethod",L"OverrideRootScale").Invoke();
    auto* actor=begin.Object();if(!actor)throw std::runtime_error("Transient preview creation failed");actor->SetFlags(RF_Transient);
    PS::WeakObjectHandle handle;handle.Assign(actor);if(handle.Get()!=actor)throw std::runtime_error("Preview ownership unavailable");actors.push_back(handle);
    Call(actor,L"/Script/Engine.Actor:SetReplicates").Text(L"bInReplicates",L"False").Invoke();
    Call(actor,L"/Script/Engine.Actor:SetActorEnableCollision").Text(L"bNewActorEnableCollision",L"False").Invoke();
    Call finish(Find(L"/Script/Engine.Default__GameplayStatics"),L"/Script/Engine.GameplayStatics:FinishSpawningActor");
    finish.Obj(L"Actor",actor).Text(L"SpawnTransform",Transform(x,y,z)).Text(L"TransformScaleMethod",L"OverrideRootScale").Invoke();if(finish.Object()!=actor)throw std::runtime_error("Transient preview construction failed");return actor;
}
void Setup(){
    std::vector<UObject*> worlds;UObjectGlobals::FindAllOf(STR("World"),worlds);
    for(auto* candidate:worlds)if(candidate&&!candidate->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed))&&candidate->GetPathName()==L"/Game/Maps/L_FrontEnd.L_FrontEnd"){if(World)throw std::runtime_error("Ambiguous frontend preview world");World=candidate;}
    if(!World)throw std::runtime_error("Native preview currently requires the main menu. Live-world preview is not yet verified.");
    worldLease.Assign(World);
    Capture=Spawn(L"/Script/Engine.SceneCapture2D",20480,20000,1090);Component=ObjectMember(Capture,L"CaptureComponent2D");componentLease.Assign(Component);
    Call create(Find(L"/Script/Engine.Default__KismetRenderingLibrary"),L"/Script/Engine.KismetRenderingLibrary:CreateRenderTarget2D");
    create.Obj(L"WorldContextObject",World).Text(L"Width",L"512").Text(L"Height",L"512").Text(L"Format",L"RTF_RGBA8_SRGB").Text(L"ClearColor",L"(R=0.015,G=0.018,B=0.02,A=1)").Text(L"bAutoGenerateMipMaps",L"False").Text(L"bSupportUAVs",L"False").Invoke();
    Target=create.Object();if(!Target)throw std::runtime_error("Native render target unavailable");targetLease.Assign(Target);Target->SetRootSet();
    ObjMember(Component,L"TextureTarget",Target);TextMember(Component,L"bCaptureEveryFrame",L"False");TextMember(Component,L"bCaptureOnMovement",L"False");TextMember(Component,L"PrimitiveRenderMode",L"PRM_UseShowOnlyList");TextMember(Component,L"CaptureSource",L"SCS_FinalColorLDR");TextMember(Component,L"FOVAngle",L"28");TextMember(Component,L"PostProcessBlendWeight",L"0");
    size_t lightIndex=0;for(const auto& point:std::vector<std::array<double,4>>{{20135,19955,1220,12000},{20120,20090,1190,8000}}){auto* light=Spawn(L"/Script/Engine.PointLight",point[0],point[1],point[2]);rigLights[lightIndex++].Assign(light);auto* component=ObjectMember(light,L"LightComponent");Call(component,L"/Script/Engine.SceneComponent:SetMobility").Text(L"NewMobility",L"Movable").Invoke();Call(component,L"/Script/Engine.LightComponent:SetIntensity").Text(L"NewIntensity",std::to_wstring(point[3])).Invoke();TextMember(component,L"AttenuationRadius",L"1000");}
    pixels.reserve(CharacterPreviewFrame::Bytes);
}
std::vector<std::string> Rows(const CharacterWorkbench::Field& field){
    const auto table=Load(to_wstring(CharacterWorkbench::Table(field)));
    Call names(Find(L"/Script/Engine.Default__DataTableFunctionLibrary"),L"/Script/Engine.DataTableFunctionLibrary:GetDataTableRowNames");names.Obj(L"Table",table).Invoke();
    auto* prop=CastField<FArrayProperty>(names.Prop(L"OutRowNames"));auto* inner=prop?CastField<FNameProperty>(prop->GetInner()):nullptr;auto* a=static_cast<FScriptArray*>(names.Value(L"OutRowNames"));
    if(!inner||inner->GetSize()!=sizeof(FName)||!a||a->Num()<0||a->Num()>1024||(a->Num()&&!a->GetData()))throw std::runtime_error("Native customization rows changed");
    std::vector<std::string> result;for(int i=0;i<a->Num();++i){auto text=to_string(reinterpret_cast<FName*>(a->GetData())[i].ToString());if(CharacterWorkbench::Identifier(text))result.push_back(text);}
    const auto order=[](const std::string& value){const auto at=value.find_first_of("0123456789");int number=2147483647;if(at!=value.npos){const auto parsed=std::from_chars(value.data()+at,value.data()+value.size(),number);if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size())number=2147483647;}return std::tuple{value.substr(0,at),number,value};};
    std::sort(result.begin(),result.end(),[&](const auto& a,const auto& b){return order(a)<order(b);});return result;
}
UObject* Soft(UObject* owner,const wchar_t* field){
    auto* p=Member(owner,field);if(!CastField<FSoftObjectProperty>(p)&&!CastField<FSoftClassProperty>(p))throw std::runtime_error("Native visual reference type changed");
    if(p->GetSize()!=sizeof(UECustom::FSoftObjectPtr))throw std::runtime_error("Native visual reference extent changed");
    const auto& value=*p->ContainerPtrToValuePtr<UECustom::FSoftObjectPtr>(owner);const auto package=to_string(value.ObjectID.GetLongPackageFName().ToString()),asset=to_string(value.ObjectID.GetAssetFName().ToString());
    if(package.empty()||package=="None"||asset.empty()||asset=="None")return nullptr;
    if(!package.starts_with('/')||value.ObjectID.SubPathString.GetCharArray().Num()>1)throw std::runtime_error("Unsupported native visual subobject path");return Load(to_wstring(package+"."+asset));
}
std::string NativeSlot(UObject* item){
    auto* field=Member(item,L"Slot");UEnum* enumeration=nullptr;int64_t value=0;
    if(auto* p=CastField<FEnumProperty>(field);p&&p->GetUnderlyingProperty()){enumeration=p->GetEnum();value=p->GetUnderlyingProperty()->GetSignedIntPropertyValue(p->ContainerPtrToValuePtr<void>(item));}
    else if(auto* p=CastField<FByteProperty>(field)){enumeration=p->GetEnum().Get();value=p->GetUnsignedIntPropertyValue(p->ContainerPtrToValuePtr<void>(item));}
    if(!enumeration)throw std::runtime_error("Native wearable slot unavailable");auto text=to_string(enumeration->GetNameByValue(value).ToString());const auto colon=text.rfind("::");return colon==text.npos?text:text.substr(colon+2);
}
void Item(const std::string& path){
    auto* data=Load(to_wstring(path));auto* type=Load(L"/Script/Dominion.ItemData");if(!type->IsA<UClass>()||!data->IsA(static_cast<UClass*>(type)))throw std::runtime_error("Item Preview requires native ItemData");
    UObject* visual=nullptr;bool skeletal=false;
    // Held equipment's presentation mesh is more specific than ItemData's
    // generic pickup mesh (which can be an intentionally crude placeholder).
    if(data->GetClassPrivate()->GetPropertyByNameInChain(L"HeldEquipmentActorClass"))if(auto* cls=Soft(data,L"HeldEquipmentActorClass");cls&&cls->IsA<UClass>()){
        auto* defaults=static_cast<UClass*>(cls)->GetClassDefaultObject().Get();auto* mesh=defaults?DragonWilds::ActorHelper::GetObjectRef(defaults,L"MeshComponent"):nullptr;
        if(mesh){visual=DragonWilds::ActorHelper::GetObjectRef(mesh,L"SkeletalMesh");skeletal=visual!=nullptr;}
    }
    if(!visual)visual=Soft(data,L"StaticMesh");
    if(!visual)throw std::runtime_error("This item has no verified native display mesh. Its gameplay Blueprint was not spawned.");
    Preview=Spawn(skeletal?L"/Script/Engine.SkeletalMeshActor":L"/Script/Engine.StaticMeshActor",20000,20000,1100);
    auto* mesh=ObjectMember(Preview,skeletal?L"SkeletalMeshComponent":L"StaticMeshComponent");if(!mesh)throw std::runtime_error("Native display mesh component unavailable");
    Call(mesh,L"/Script/Engine.SceneComponent:SetMobility").Text(L"NewMobility",L"Movable").Invoke();
    if(skeletal)Call(mesh,L"/Script/Engine.SkeletalMeshComponent:SetSkeletalMesh").Obj(L"NewMesh",visual).Text(L"bReinitPose",L"True").Invoke();
    else{Call set(mesh,L"/Script/Engine.StaticMeshComponent:SetStaticMesh");set.Obj(L"NewMesh",visual).Invoke();if(!set.Bool())throw std::runtime_error("Native item mesh rejected");}
    itemVisual.Assign(mesh);lastItem=path;
}
std::array<double,7> Bounds(UObject* component){
    Call bounds(Find(L"/Script/Engine.Default__KismetSystemLibrary"),L"/Script/Engine.KismetSystemLibrary:GetComponentBounds");bounds.Obj(L"Component",component).Invoke();std::array<double,7> out{};
    for(const auto* param:{L"Origin",L"BoxExtent"}){auto* prop=CastField<FStructProperty>(bounds.Prop(param));if(!prop||!prop->GetStruct())throw std::runtime_error("Preview bounds changed");int i=0;for(const auto* axis:{L"X",L"Y",L"Z"}){auto* p=CastField<FNumericProperty>(prop->GetStruct()->FindProperty(FName(axis,FNAME_Find)));if(!p||!p->IsFloatingPoint())throw std::runtime_error("Preview vector changed");double value=p->GetFloatingPointPropertyValue(p->ContainerPtrToValuePtr<void>(bounds.Value(param)));if(!std::isfinite(value))throw std::runtime_error("Nonfinite preview bounds");if(param==std::wstring(L"Origin"))out[i++]=value;else{out[4+i++]=std::abs(value);out[3]=std::max(out[3],std::abs(value));}}}
    return out;
}
void FrameBody(){
    double low=100000.,high=-100000.;for(size_t i=2;i<PreviewMeshes.size();++i)if(auto* mesh=ObjectMember(Preview,PreviewMeshes[i]);mesh&&ObjectMember(mesh,L"SkeletalMesh")){
        const auto b=Bounds(mesh);if(b[2]<900||b[2]>1500||b[6]<=0||b[6]>300)continue;low=std::min(low,b[2]-b[6]);high=std::max(high,b[2]+b[6]);
    }
    if(high-low>=50&&high-low<=500){bodyCenter=(low+high)*.5;bodyDistance=std::max(480.,(high-low)*.5/std::tan(14.*3.141592653589793/180.)*1.15);}else{bodyCenter=1090.;bodyDistance=480.;}bodyFramed=true;
}
void StandardPose(){
    auto* animation=ObjectMember(Preview,L"AnimationAssetToPlay");auto* animationSkeleton=animation?ObjectMember(animation,L"Skeleton"):nullptr;
    for(const auto* name:PreviewMeshes)if(auto* mesh=ObjectMember(Preview,name)){
        TextMember(mesh,L"bPauseAnims",L"False");
        TextMember(mesh,L"VisibilityBasedAnimTickOption",L"AlwaysTickPoseAndRefreshBones");
        auto* asset=ObjectMember(mesh,L"SkeletalMesh");
        if(animationSkeleton&&asset&&ObjectMember(asset,L"Skeleton")==animationSkeleton){
            Call(mesh,L"/Script/Engine.SkeletalMeshComponent:PlayAnimation").Obj(L"NewAnimToPlay",animation).Text(L"bLooping",L"True").Invoke();
            Call(mesh,L"/Script/Engine.SkeletalMeshComponent:SetPlayRate").Text(L"Rate",L"0").Invoke();
            Call(mesh,L"/Script/Engine.SkeletalMeshComponent:SetPosition").Text(L"InPos",L"0").Text(L"bFireNotifies",L"False").Invoke();
        }
    }
    // Let the native leader/follower update run before freezing all parts.
    awaitingPose=true;freezeAt=Clock::now()+std::chrono::milliseconds(100);lastAnimation=lastPlaying=-1;
}
void Playback(bool playing){
    auto* animation=ObjectMember(Preview,L"AnimationAssetToPlay");auto* skeleton=animation?ObjectMember(animation,L"Skeleton"):nullptr;
    if(!skeleton)throw std::runtime_error("No verified native preview animation is available");
    for(const auto* name:PreviewMeshes)if(auto* mesh=ObjectMember(Preview,name)){
        auto* asset=ObjectMember(mesh,L"SkeletalMesh");if(asset&&ObjectMember(asset,L"Skeleton")==skeleton)Call(mesh,L"/Script/Engine.SkeletalMeshComponent:SetPlayRate").Text(L"Rate",playing?L"1":L"0").Invoke();
        TextMember(mesh,L"bPauseAnims",playing?L"False":L"True");
    }
}
void Read(){
    Call read(Find(L"/Script/Engine.Default__KismetRenderingLibrary"),L"/Script/Engine.KismetRenderingLibrary:ReadRenderTarget");read.Obj(L"WorldContextObject",World).Obj(L"TextureRenderTarget",Target).Text(L"bNormalize",L"False").Invoke();if(!read.Bool())throw std::runtime_error("Native preview readback failed");
    auto* p=CastField<FArrayProperty>(read.Prop(L"OutSamples"));auto* inner=p?CastField<FStructProperty>(p->GetInner()):nullptr;auto* a=static_cast<FScriptArray*>(read.Value(L"OutSamples"));
    if(!inner||!inner->GetStruct()||inner->GetSize()!=4||!a||a->Num()!=512*512||!a->GetData())throw std::runtime_error("Native pixel array changed");
    int n=0;for(const auto* name:{L"B",L"G",L"R",L"A"}){auto* c=CastField<FNumericProperty>(inner->GetStruct()->FindProperty(FName(name,FNAME_Find)));if(!c||!c->IsInteger()||c->GetOffset_Internal()!=n++||c->GetSize()!=1||c->GetArrayDim()!=1)throw std::runtime_error("Native pixel layout changed");}
    pixels.resize(CharacterPreviewFrame::Bytes);const auto* bgra=static_cast<const unsigned char*>(a->GetData());for(size_t i=0;i<pixels.size();i+=4){pixels[i]=bgra[i+2];pixels[i+1]=bgra[i+1];pixels[i+2]=bgra[i];pixels[i+3]=255;}
    if(lastAnimation!=1){constexpr size_t limit=96*1024*1024;while(cacheBytes+pixels.size()>limit&&!angles.empty()){auto old=std::min_element(angles.begin(),angles.end(),[](const auto&a,const auto&b){return a.second.Used<b.second.Used;});cacheBytes-=old->second.Pixels.size();angles.erase(old);}auto key=std::tuple{lastAngle,lastPitch,lastZoom};if(auto old=angles.find(key);old!=angles.end()){cacheBytes-=old->second.Pixels.size();angles.erase(old);}angles.emplace(key,Cached{pixels,++cacheClock});cacheBytes+=pixels.size();}
    if(channel.Lock()){std::memcpy(channel.Get()->Pixels,pixels.data(),pixels.size());++channel.Get()->Serial;std::snprintf(channel.Get()->Status,256,lastPlaying==1?"Native preview animation · preview actors only":"Native game preview · 512 px · idle frames reused");channel.Unlock();}
}
} // namespace
Json Choices(){
    Json choices=Json::array();for(const auto& field:CharacterWorkbench::Fields)choices.push_back({{"Key",field.Key},{"Label",field.Label},{"Table",CharacterWorkbench::Table(field)},{"Rows",Rows(field)}});
    Json swatches=Json::object();std::string warning;
    try{
        auto* cls=Load(L"/Game/UI/MainMenu/CharacterCreate/Data/DA_CharacterOptionData.DA_CharacterOptionData_C");if(!cls->IsA<UClass>())throw std::runtime_error("Native character option class unavailable");auto* data=static_cast<UClass*>(cls)->GetClassDefaultObject().Get();
        auto* map=CastField<FMapProperty>(Member(data,L"CharacterOptionData"));if(!map)throw std::runtime_error("Native option map changed");auto* values=CastField<FStructProperty>(map->GetValueProp());if(!values||!values->GetStruct())throw std::runtime_error("Native option groups changed");
        auto* options=CastField<FArrayProperty>(values->GetStruct()->FindProperty(FName(L"OptionData",FNAME_Find)));auto* option=options?CastField<FStructProperty>(options->GetInner()):nullptr;if(!option||!option->GetStruct())throw std::runtime_error("Native option array changed");
        UECustom::FScriptMapHelper helper(map,map->ContainerPtrToValuePtr<void>(data));size_t groups=0;
        helper.ForEachPair([&](void*,void* value){if(++groups>32)throw std::runtime_error("Native option groups exceed bound");auto* array=options->ContainerPtrToValuePtr<FScriptArray>(value);if(!array||array->Num()<0||array->Num()>512||(array->Num()&&!array->GetData()))throw std::runtime_error("Native option count changed");
            auto* handle=CastField<FStructProperty>(option->GetStruct()->FindProperty(FName(L"DataHandle",FNAME_Find)));auto* color=CastField<FStructProperty>(option->GetStruct()->FindProperty(FName(L"Color",FNAME_Find)));if(!handle||!color||!handle->GetStruct()||!color->GetStruct()||color->GetStruct()->GetPathName()!=L"/Script/CoreUObject.Color")throw std::runtime_error("Native swatch contract changed");
            auto* table=CastField<FObjectPropertyBase>(handle->GetStruct()->FindProperty(FName(L"DataTable",FNAME_Find)));auto* row=CastField<FNameProperty>(handle->GetStruct()->FindProperty(FName(L"RowName",FNAME_Find)));if(!table||!row)throw std::runtime_error("Native swatch row handle changed");
            for(int i=0;i<array->Num();++i){auto* item=static_cast<uint8_t*>(array->GetData())+size_t(i)*option->GetElementSize();auto* h=handle->ContainerPtrToValuePtr<void>(item);auto* t=table->GetObjectPropertyValue(table->ContainerPtrToValuePtr<void>(h));if(!t)continue;const auto name=to_string(row->ContainerPtrToValuePtr<FName>(h)->ToString());const auto path=to_string(t->GetPathName());
                for(const auto& field:CharacterWorkbench::Fields)if(path==CharacterWorkbench::Table(field)&&(std::string_view(field.Key)=="SkinTone"||std::string_view(field.Key)=="EyeColor"||std::string_view(field.Key)=="HairColor"||std::string_view(field.Key)=="EyebrowColor")){
                    Json rgba=Json::array();auto* c=color->ContainerPtrToValuePtr<void>(item);for(const auto* channel:{L"R",L"G",L"B",L"A"}){auto* p=CastField<FByteProperty>(color->GetStruct()->FindProperty(FName(channel,FNAME_Find)));if(!p)throw std::runtime_error("Native swatch channel changed");rgba.push_back(*p->ContainerPtrToValuePtr<uint8_t>(c));}swatches[field.Key][name]=rgba;
                }
            }
        });
    }catch(const std::exception& e){warning=e.what();}
    return {{"Fields",choices},{"Swatches",swatches},{"SwatchWarning",warning},{"BeardColorSupported",false},{"Status","Appearance rows and swatch colors read from the game's native character-creation options."}};
}
std::string LoadoutSlot(int index){auto* object=Find(L"/Script/Dominion.ELoadoutSlot");if(!object||!object->IsA<UEnum>()||index<0||index>63)throw std::runtime_error("Native loadout slot unavailable");auto text=to_string(static_cast<UEnum*>(object)->GetNameByValue(index).ToString());const auto colon=text.rfind("::");if(colon!=text.npos)text=text.substr(colon+2);if(text.empty()||text=="None"||text.ends_with("_MAX"))throw std::runtime_error("Unknown native loadout slot");return text;}
Json Skills(const Json& draft){
    const auto& saved=draft.at("GameProgress").at("Skills").at("Skills");if(!saved.is_array()||saved.size()>128)throw std::runtime_error("Native saved skill layout unavailable");
    TArray<UObject*> objects;auto* type=Load(L"/Script/Dominion.SkillData");if(!type->IsA<UClass>())throw std::runtime_error("Native skill class unavailable");UECustom::UObjectGlobals::GetObjectsOfClass(static_cast<UClass*>(type),objects,true);if(objects.Num()<0||objects.Num()>512)throw std::runtime_error("Native skill registry exceeds bound");
    std::map<std::string,UObject*> identities;std::set<std::string> ambiguous;
    for(auto* object:objects)if(object&&!object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed))){auto* id=CastField<FStrProperty>(Member(object,L"PersistenceID"));if(!id)continue;const auto& chars=id->ContainerPtrToValuePtr<FString>(object)->GetCharArray();if(chars.Num()<2||chars.Num()>129||!chars.GetData())continue;const auto key=to_string(StringType(chars.GetData(),chars.Num()-1));if(!identities.emplace(key,object).second)ambiguous.insert(key);}
    Json thresholds=Json::array();int highest=1;for(const auto& [id,object]:identities)if(!ambiguous.contains(id)){Call maximum(object,L"/Script/Dominion.SkillData:GetMaxLevel");maximum.Invoke();const int value=maximum.Integer();if(value<1||value>200)throw std::runtime_error("Native skill maximum exceeds supported bound");highest=std::max(highest,value);}
    auto* settings=Load(L"/Script/Dominion.Default__SkillSettings");auto* handle=CastField<FStructProperty>(Member(settings,L"XPCurveRowHandle"));
    if(!handle||!handle->GetStruct()||handle->GetStruct()->GetPathName()!=L"/Script/Engine.CurveTableRowHandle")throw std::runtime_error("Native skill XP curve handle changed");
    auto* tableField=CastField<FObjectProperty>(handle->GetStruct()->FindProperty(FName(L"CurveTable",FNAME_Find)));auto* rowField=CastField<FNameProperty>(handle->GetStruct()->FindProperty(FName(L"RowName",FNAME_Find)));
    if(!tableField||!rowField)throw std::runtime_error("Native skill XP curve fields unavailable");auto* memory=handle->ContainerPtrToValuePtr<void>(settings);auto* table=tableField->GetObjectPropertyValue(tableField->ContainerPtrToValuePtr<void>(memory));const auto rowName=rowField->ContainerPtrToValuePtr<FName>(memory)->ToString();
    if(!table||rowName==L"None")throw std::runtime_error("Native skill XP curve is not configured");
    int previous=-1;for(int level=1;level<=highest;++level){Call xp(Find(L"/Script/Engine.Default__DataTableFunctionLibrary"),L"/Script/Engine.DataTableFunctionLibrary:EvaluateCurveTableRow");xp.Obj(L"CurveTable",table).Text(L"RowName",rowName).Text(L"InXY",std::to_wstring(level)).Invoke();
        auto* valueField=CastField<FFloatProperty>(xp.Prop(L"OutXY"));auto* resultField=CastField<FByteProperty>(xp.Prop(L"OutResult"));if(!valueField||!resultField||resultField->GetUnsignedIntPropertyValue(xp.Value(L"OutResult"))!=0)throw std::runtime_error("Native skill XP curve evaluation failed");const auto reading=*static_cast<float*>(xp.Value(L"OutXY"));
        if(!std::isfinite(reading)||reading<0||reading>2147483000.f||std::abs(double(reading)-std::round(double(reading)))>.0001)throw std::runtime_error("Native XP curve returned unsupported threshold");const int value=int(reading);if(level>1&&value<=previous)throw std::runtime_error("Native XP thresholds unavailable at level "+std::to_string(level)+": "+std::to_string(previous)+" -> "+std::to_string(value));thresholds.push_back(value);previous=value;}
    Json rows=Json::array();for(size_t index=0;index<saved.size();++index){const auto& record=saved[index];const auto id=record.at("Id").get<std::string>();const auto xp=record.at("Xp").get<int64_t>();if(xp<0||xp>2147483647)throw std::runtime_error("Saved XP exceeds native int bound");Json row={{"Index",index},{"Id",id},{"XP",xp},{"Resolved",false},{"Name","Unresolved skill"}};
        if(identities.contains(id)&&!ambiguous.contains(id)){auto* skill=identities.at(id);Call maximum(skill,L"/Script/Dominion.SkillData:GetMaxLevel");maximum.Invoke();const int max=maximum.Integer();int level=1;for(int n=1;n<=max;++n)if(xp>=thresholds[n-1].get<int64_t>())level=n;
            auto* name=CastField<FTextProperty>(Member(skill,L"Name"));const auto title=name?to_string(DragonWilds::PropertyHelper::GetTextAsString(*name->ContainerPtrToValuePtr<FText>(skill))):to_string(skill->GetName());row.update({{"Resolved",true},{"Name",title},{"Path",to_string(skill->GetPathName())},{"Icon",PS::AssetSearch::PresentationIcon(skill)},{"Level",level},{"MaxLevel",max}});
        }rows.push_back(row);
    }return {{"Items",rows},{"Thresholds",thresholds},{"Native",true}};
}
Json InventoryLayout(){
    Json result=Json::array();auto* type=Load(L"/Script/Dominion.PagedInventoryWithLogComponent");if(!type->IsA<UClass>())throw std::runtime_error("Native paged inventory class unavailable");TArray<UObject*> components;UECustom::UObjectGlobals::GetObjectsOfClass(static_cast<UClass*>(type),components,true);
    if(components.Num()<0||components.Num()>512)throw std::runtime_error("Native inventory layout inventory exceeds its bound");
    for(auto* component:components)try{
        if(!component||component->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed)))continue;
        auto* pages=CastField<FArrayProperty>(Member(component,L"InventoryPages"));auto* structure=pages?CastField<FStructProperty>(pages->GetInner()):nullptr;
        auto* quick=CastField<FIntProperty>(Member(component,L"NumberOfQuickActionSlots"));auto* maximum=CastField<FIntProperty>(Member(component,L"MaxSlotCount"));
        if(!structure||!structure->GetStruct()||!quick||!maximum)continue;auto* array=pages->ContainerPtrToValuePtr<FScriptArray>(component);if(!array||array->Num()<1||array->Num()>16||!array->GetData())continue;
        int offset=*quick->ContainerPtrToValuePtr<int32_t>(component),capacity=*maximum->ContainerPtrToValuePtr<int32_t>(component);if(offset<0||offset>32||capacity<0||capacity>4096)continue;if(!capacity)capacity=4096; // Before component initialization the authoritative page descriptors still bound every insertion.
        Json candidate=Json::array();if(offset)candidate.push_back({{"Group","Quick slots"},{"Start",0},{"Count",offset}});
        for(int i=0;i<array->Num();++i){auto* row=static_cast<uint8_t*>(array->GetData())+i*structure->GetElementSize();auto* slots=CastField<FIntProperty>(structure->GetStruct()->FindProperty(FName(L"NumSlots",FNAME_Find)));auto* slotType=CastField<FEnumProperty>(structure->GetStruct()->FindProperty(FName(L"SlotType",FNAME_Find)));if(!slots||!slotType||!slotType->GetEnum()||!slotType->GetUnderlyingProperty())throw std::runtime_error("Native inventory page contract changed");const int count=*slots->ContainerPtrToValuePtr<int32_t>(row);if(count<1||count>512||offset+count>capacity)throw std::runtime_error("Native inventory page bounds changed");
            auto type=to_string(slotType->GetEnum()->GetNameByValue(slotType->GetUnderlyingProperty()->GetSignedIntPropertyValue(slotType->ContainerPtrToValuePtr<void>(row))).ToString());auto lower=type;for(auto& c:lower)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));const std::string group=lower.find("rune")!=lower.npos?"Runes":(lower.find("ammo")!=lower.npos||lower.find("ammunition")!=lower.npos)?"Ammo":lower.find("quest")!=lower.npos?"Quest items":"Inventory";
            candidate.push_back({{"Group",group},{"Start",offset},{"Count",count},{"NativeType",type}});offset+=count;
        }
        if(result.empty())result=candidate;else if(result!=candidate)throw std::runtime_error("Mounted inventory layouts disagree; insertion requires a concrete verified layout");
    }catch(const std::exception& error){if(std::string(error.what()).find("disagree")!=std::string::npos)throw;}
    if(result.empty())throw std::runtime_error("Native inventory page layout is not loaded. Open the game inventory once, then refresh; no website slot limits are assumed.");return result;
}
Json Render(const Json& appearance,const Json& request){
    const auto wanted=request.value("View",std::string("Head"));if(wanted!="Head"&&wanted!="Body"&&wanted!="Item")throw std::runtime_error("Invalid native preview view");
    const auto equipment=request.value("Equipment",Json::object());if(!equipment.is_object()||equipment.size()>8)throw std::runtime_error("Preview outfit exceeds its bound");
    try{
        const auto path=request.value("Path",std::string{});if(wanted=="Item"&&(path.empty()||path.size()>1024||!path.starts_with('/')||path.find_first_of("\r\n\t'")!=path.npos))throw std::runtime_error("Choose an Item Bay entry with a canonical native asset path");
        if(!mode.empty()&&((mode=="Item")!=(wanted=="Item")||(wanted=="Item"&&lastItem!=path)))DestroyScene();
        if(!World||!worldLease.Get()){DestroyScene();Setup();}
        if(channel.Name().empty())channel.Open("Local\\RuneSchema.CharacterPreview."+std::to_string(GetCurrentProcessId())+"."+std::to_string(GetTickCount64())+"."+std::to_string(++session),true);
        if(wanted=="Item"){if(!Preview)Item(path);mode=wanted;due=Clock::now()+std::chrono::milliseconds(150);Invalidate();}
        else{
        const auto text=CharacterWorkbench::NativeText(appearance);const auto e=equipment.dump();
        // ApplyFullCustomization reinitializes the leader/follower meshes. Reusing
        // an already frozen actor leaves hair/head followers at different poses.
        // Recreate only this cosmetic actor on an edit, never on rotation/zoom.
        if(Preview&&!lastAppearance.empty()&&(lastAppearance!=text||lastEquipment!=e))DestroyPreviewActor();
        if(!Preview){Preview=Spawn(L"/Game/Gameplay/Frontend/BP_PlayerCharacterPreview.BP_PlayerCharacterPreview_C",20000,20000,1000);if(auto* rotating=ObjectMember(Preview,L"Rotating"))Call(rotating,L"/Script/Engine.ActorComponent:SetComponentTickEnabled").Text(L"bEnabled",L"False").Invoke();Call(Preview,L"/Script/Engine.Actor:SetActorTickEnabled").Text(L"bEnabled",L"False").Invoke();}
        if(lastAppearance!=text||lastEquipment!=e){Call(Preview,L"/Script/Dominion.PlayerCharacterPreview:ApplyFullCustomization").Text(L"CustomizationData",to_wstring(text)).Text(L"bInIsNewCharacter",L"False").Invoke();
            std::wstring loadout=L"(";for(const auto& [slot,path]:equipment.items()){const auto value=path.get<std::string>();if(!value.starts_with('/')||value.find_first_of("\r\n\t'")!=value.npos)throw std::runtime_error("Invalid native wearable path");auto* item=Load(to_wstring(value));auto* type=Load(L"/Script/Dominion.WearableEquipmentData");if(!type||!type->IsA<UClass>()||!item->IsA(static_cast<UClass*>(type)))throw std::runtime_error("Preview outfit requires wearable ItemData; held items are not equipped or used");if(NativeSlot(item)!=slot)throw std::runtime_error("This wearable does not match the selected native equipment slot");if(loadout.size()>1)loadout+=L",";loadout+=L"WearableEquipmentData'"+item->GetPathName()+L"'";}if(equipment.empty()){auto* tunic=Load(L"/Game/Gameplay/Character/Player/Equipment/Body/ITEM_Armour_T1_Body_Adventurers.ITEM_Armour_T1_Body_Adventurers");loadout+=L"WearableEquipmentData'"+tunic->GetPathName()+L"'";}loadout+=L")";
            // Keep a complete, predictable base outfit in unfilled body/leg slots.
            loadout.pop_back();for(const auto* slot:{"Body","Legs"})if(!equipment.contains(slot)&&!(equipment.empty()&&std::string_view(slot)=="Body")){
                const auto name=std::string("/Game/Gameplay/Character/Player/Equipment/")+slot+"/ITEM_Armour_T1_"+slot+"_Adventurers.ITEM_Armour_T1_"+slot+"_Adventurers";
                auto* base=Load(to_wstring(name));if(loadout.size()>1)loadout+=L",";loadout+=L"WearableEquipmentData'"+base->GetPathName()+L"'";
            }loadout+=L")";
            Call(Preview,L"/Script/Dominion.PlayerCharacterPreview:HandleCharacterLoadoutChanged").Text(L"CustomizationLoadout",loadout).Text(L"bInIsNewCharacter",L"False").Invoke();lastAppearance=text;lastEquipment=e;followups=2;followAt=Clock::now()+std::chrono::milliseconds(900);due=Clock::now()+std::chrono::milliseconds(2000);Invalidate();}
        if(mode!=wanted){mode=wanted;Invalidate();}TextMember(Preview,L"bHideHeadWearable",wanted=="Head"?L"True":L"False");
        }
        Call(Component,L"/Script/Engine.SceneCaptureComponent:ShowOnlyActorComponents").Obj(L"InActor",Preview).Text(L"bIncludeFromChildActors",L"True").Invoke();Status("Preparing native appearance… last ready frame retained");
        if(channel.Lock()){channel.Get()->Heartbeat=GetTickCount64();channel.Get()->Stop=0;channel.Unlock();}
        const auto animation=wanted=="Item"?nullptr:ObjectMember(Preview,L"AnimationAssetToPlay");
        return {{"Channel",channel.Name()},{"View",mode},{"Path",path},{"AnimationName",animation?"Native preview · "+to_string(animation->GetName()):std::string{}},{"Native",true},{"Width",512},{"Height",512},{"Status","Native preview requested. No player, inventory or save was modified."}};
    }catch(...){Reset();throw;}
}
void Tick(){
    if(channel.Name().empty())return;
    try{
        if(!worldLease.Get()||!componentLease.Get()||!targetLease.Get()){Reset();return;}
        int yaw=0,zoom=100,pitch=0,animation=0,playing=0;bool stop=false;if(!channel.Lock())return;auto* data=channel.Get();yaw=data->Yaw;zoom=data->Zoom;pitch=data->Pitch;animation=data->Animation==1?1:0;playing=animation&&data->Playing==1?1:0;stop=data->Stop||GetTickCount64()-data->Heartbeat>2500;channel.Unlock();if(stop){Reset();return;}
        const auto now=Clock::now();if(pending){Read();pending=false;}
        if(followups&&now>=followAt){Invalidate();--followups;followAt=now+std::chrono::milliseconds(800);StandardPose();}
        if(awaitingPose&&now>=freezeAt){for(const auto* name:PreviewMeshes)if(auto* mesh=ObjectMember(Preview,name))TextMember(mesh,L"bPauseAnims",L"True");awaitingPose=false;Invalidate();}
        if(now<due||now<next||awaitingPose)return;
        if(mode!="Item"&&(animation!=lastAnimation||playing!=lastPlaying)){
            const bool restore=animation==0&&lastAnimation==1;
            if(animation==1)Playback(playing!=0);
            if(restore){StandardPose();lastAnimation=animation;lastPlaying=playing;return;}
            lastAnimation=animation;lastPlaying=playing;Invalidate();
        }
        if(mode!="Item"&&playing)dirty=true;
        yaw=std::clamp(yaw/2*2,-180,180);zoom=std::clamp(zoom,70,150);
        pitch=std::clamp(pitch/2*2,-40,40);if(!dirty&&yaw==lastAngle&&pitch==lastPitch&&zoom==lastZoom)return;lastAngle=yaw;lastPitch=pitch;lastZoom=zoom;dirty=false;
        if(const auto cached=angles.find({yaw,pitch,zoom});!animation&&cached!=angles.end()){cached->second.Used=++cacheClock;if(channel.Lock()){std::memcpy(channel.Get()->Pixels,cached->second.Pixels.data(),CharacterPreviewFrame::Bytes);++channel.Get()->Serial;channel.Unlock();}next=now+std::chrono::milliseconds(33);return;}
        Call(Preview,L"/Script/Engine.Actor:K2_SetActorRotation").Text(L"NewRotation",L"(Pitch=0,Yaw="+std::to_wstring(yaw-90)+L",Roll=0)").Text(L"bTeleportPhysics",L"True").Invoke();
        if(mode=="Body"&&!bodyFramed)FrameBody();const auto bounds=Bounds(mode=="Item"?itemVisual.Get():ObjectMember(Preview,L"HeadMeshComponent"));const double distance=(mode=="Item"?std::clamp(bounds[3]*4.5,15.,2000.):mode=="Head"?105.:bodyDistance)*100./zoom,z=mode=="Item"?bounds[2]:mode=="Head"?bounds[2]+2:bodyCenter;
        if(mode=="Item")for(size_t i=0;i<rigLights.size();++i)if(auto* light=rigLights[i].Get()){
            Call(light,L"/Script/Engine.Actor:K2_SetActorLocation").Text(L"NewLocation",Vector(bounds[0]+distance*.65,bounds[1]+distance*(i?.45:-.45),bounds[2]+distance*.45)).Text(L"bSweep",L"False").Text(L"bTeleport",L"True").Invoke();
            auto* component=ObjectMember(light,L"LightComponent");const double intensity=std::clamp((i?8000.:12000.)*std::pow(distance/480.,2),8000.,1000000.);Call(component,L"/Script/Engine.LightComponent:SetIntensity").Text(L"NewIntensity",std::to_wstring(intensity)).Invoke();TextMember(component,L"AttenuationRadius",std::to_wstring(std::clamp(distance*4.,1000.,10000.)));
        }
        const double orbit=pitch*3.141592653589793/180.;Call(Capture,L"/Script/Engine.Actor:K2_SetActorLocation").Text(L"NewLocation",Vector((mode=="Item"?bounds[0]:20000)+distance*std::cos(orbit),mode=="Item"?bounds[1]:20000,z+distance*std::sin(orbit))).Text(L"bSweep",L"False").Text(L"bTeleport",L"True").Invoke();Call(Capture,L"/Script/Engine.Actor:K2_SetActorRotation").Text(L"NewRotation",L"(Pitch="+std::to_wstring(-1-pitch)+L",Yaw=180,Roll=0)").Text(L"bTeleportPhysics",L"True").Invoke();
        Call(Component,L"/Script/Engine.SceneCaptureComponent2D:CaptureScene").Invoke();pending=true;next=now+std::chrono::milliseconds(33);
    }catch(const std::exception& e){Status(e.what());DestroyScene();}
}
void Reset(){DestroyScene();channel.Close();}
}
