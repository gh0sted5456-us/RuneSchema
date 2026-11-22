#include "Runtime/ItemSnapshotService.h"
#include "Core/ItemSnapshot.h"
#include "SDK/Helper/PlayerVitals.h"
#include "SDK/Helper/EquipmentDelivery.h"
#include "SDK/Helper/PlayerStatus.h"
#include "SDK/Helper/ReflectedContract.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "Runtime/NetworkContext.h"
#include "Runtime/NetworkRole.h"
#include "Runtime/RuneSchemaPluginApi.h"
#include "Unreal/UnrealInitializer.hpp"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/Property/FEnumProperty.hpp"
#include "Unreal/UEnum.hpp"
#include <cstring>
#include <format>
#include <memory>
#include <string>
#include <vector>
// runeschema.item.snapshot: reads one exact live item instance and returns its save fields
// losslessly. Read-only. The census rules follow Grand Exchange's proven NativeItemState:
//   - every CPF_SaveGame property on the item's class chain, including SaveGame+Transient ones;
//   - single-element, bounded, in-extent fields only; object references refuse the whole item;
//   - each value is exported and re-imported, and must be Identical to the source, or the whole
//     item is refused (no silent loss); FDateTime uses the exact 8-byte codec.
namespace PS::ItemSnapshotService {
namespace {
using namespace RC::Unreal;
namespace E=EquipmentDelivery;namespace AH=DragonWilds::ActorHelper;namespace C=DragonWilds::ReflectedContract;namespace S=ItemSnapshot;
std::string Narrow(const FString& text){const auto* chars=*text;return chars?RC::to_string(RC::StringType(chars)):std::string{};}
std::string GuidText(const E::Guid& id){return std::format("{:08X}{:08X}{:08X}{:08X}",id[0],id[1],id[2],id[3]);}

// A reflected scratch value, owned by its property for construction and destruction.
struct Scratch{
    FProperty* Property;void* Data;
    explicit Scratch(FProperty* property):Property(property),Data(property->AllocateAndInitializeValue()){if(!Data)throw std::runtime_error("Native property allocation failed");}
    ~Scratch(){try{Property->DestroyAndFreeValue(Data);}catch(...){}}
    Scratch(const Scratch&)=delete;Scratch& operator=(const Scratch&)=delete;
};
bool ExactDateTime(FProperty* property){
    auto* type=CastField<FStructProperty>(property);
    if(!type||!type->GetStruct()||type->GetStruct()->GetPathName()!=RC::to_generic_string(std::string(S::DateTimeStruct)))return false;
    if(property->GetSize()!=8)throw std::runtime_error("Native DateTime layout changed");
    return true;
}
std::string Encode(FProperty* property,const void* data,UObject* owner){
    if(ExactDateTime(property)){uint64_t ticks=0;std::memcpy(&ticks,data,sizeof(ticks));return S::EncodeTicks(ticks);}
    FString text;property->ExportTextItem_Direct(text,data,nullptr,owner,static_cast<int32>(EPropertyPortFlags::PPF_Delimited));
    return Narrow(text);
}
std::unique_ptr<Scratch> Decode(FProperty* property,UObject* owner,const std::string& text){
    if(text.size()>S::MaxValueBytes||text.find('\0')!=std::string::npos)throw std::runtime_error("Native property text exceeds its bound");
    auto value=std::make_unique<Scratch>(property);
    if(ExactDateTime(property)){const auto ticks=S::DecodeTicks(text);std::memcpy(value->Data,&ticks,sizeof(ticks));return value;}
    const auto wide=RC::to_generic_string(text);
    const auto* end=property->ImportText_Direct(wide.c_str(),value->Data,owner,static_cast<int32>(EPropertyPortFlags::PPF_Delimited),nullptr);
    if(!end||*end!=0)throw std::runtime_error("Native property text was not fully consumed");
    return value;
}
// The value type all the way down, so a change inside a struct, enum or container changes the layout.
std::string Shape(FProperty* property,int depth,size_t& budget){
    if(!property||depth>8||++budget>2048)throw std::runtime_error("Native item field type is too deep to describe");
    std::string out=RC::to_string(property->GetClass().GetName())+":"+std::to_string(property->GetElementSize())+"x"+std::to_string(property->GetArrayDim());
    if(auto* structure=CastField<FStructProperty>(property)){
        auto* type=structure->GetStruct().Get();if(!type)throw std::runtime_error("Native item struct type missing");
        out+="{"+RC::to_string(type->GetPathName())+"#"+std::to_string(type->GetPropertiesSize());
        for(auto* member:TFieldRange<FProperty>(type,EFieldIterationFlags::IncludeSuper))
            out+=";"+RC::to_string(member->GetName())+"@"+std::to_string(member->GetOffset_Internal())+"="+Shape(member,depth+1,budget);
        out+="}";
    }else if(auto* enumerated=CastField<FEnumProperty>(property)){
        auto* type=enumerated->GetEnum().Get();out+="<"+(type?RC::to_string(type->GetPathName()):std::string("?"))+">"+Shape(enumerated->GetUnderlyingProp(),depth+1,budget);
    }else if(auto* byte=CastField<FByteProperty>(property)){
        if(auto* type=byte->GetEnum().Get())out+="<"+RC::to_string(type->GetPathName())+">";
    }else if(auto* array=CastField<FArrayProperty>(property))out+="["+Shape(array->GetInner(),depth+1,budget)+"]";
    else if(auto* set=CastField<FSetProperty>(property))out+="("+Shape(set->GetElementProp(),depth+1,budget)+")";
    else if(auto* map=CastField<FMapProperty>(property))out+="{"+Shape(map->GetKeyProp(),depth+1,budget)+"=>"+Shape(map->GetValueProp(),depth+1,budget)+"}";
    if(out.size()>S::MaxShapeBytes)throw std::runtime_error("Native item field type description exceeds its bound");
    return out;
}
S::Field Capture(FProperty* property,UObject* item){
    auto* source=property->ContainerPtrToValuePtr<void>(item);
    S::Field field;field.Name=RC::to_string(property->GetName());
    auto* owner=property->GetOutermostOwner();field.Owner=owner?RC::to_string(owner->GetPathName()):std::string{};
    field.Type=RC::to_string(property->GetClass().GetName());field.CppType=Narrow(property->GetCPPType());
    size_t budget=0;field.Shape=Shape(property,0,budget);
    field.Codec=std::string(ExactDateTime(property)?S::DateTimeCodec:S::TextCodec);
    field.Transient=property->HasAnyPropertyFlags(CPF_Transient);
    field.Value=Encode(property,source,item);
    const auto parsed=Decode(property,item,field.Value);
    if(!property->Identical(source,parsed->Data))throw std::runtime_error("Native item property cannot round-trip without loss: "+field.Name);
    return field;
}
}
LiveItem Resolve(const S::Request& request){
    if(!IsInGameThread())return {nullptr,nullptr,nullptr,nullptr,S::Refusal("Thread","runeschema.item.snapshot must be called on the game thread.")};
    auto* controllerClass=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.DominionPlayerController"));
    if(!controllerClass)return {nullptr,nullptr,nullptr,nullptr,S::Refusal("Player","The game's player controller type is unavailable.")};
    UObject* controller=nullptr;TArray<UObject*> controllers;
    UECustom::UObjectGlobals::GetObjectsOfClass(controllerClass,controllers,true);
    for(auto* candidate:controllers){
        if(!candidate||candidate->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed))||!candidate->GetWorld())continue;
        if(S::NormalizeGuid(PlayerStatusRuntime::CharacterGuid(AH::GetObjectRef(candidate,TEXT("PlayerState"))))!=request.Player)continue;
        if(controller)return {nullptr,nullptr,nullptr,nullptr,S::Refusal("Player","More than one controller has that character GUID.")};
        controller=candidate;
    }
    if(!controller)return {nullptr,nullptr,nullptr,nullptr,S::Refusal("Player","That character is not connected.")};
    // Save fields are complete only where the game is authoritative: single player, host or server.
    if(!Network::OwnsGameplay(Network::Detect(controller).Mode))return {nullptr,nullptr,nullptr,nullptr,S::Refusal("Authority","Item snapshots are read only where the game is authoritative.")};
    UObject* container=nullptr;
    try{
        if(request.Where==S::Scope::Inventory)container=E::Owned(controller,TEXT("InventoryComponent"),TEXT("/Script/Dominion.InventoryComponent"));
        else if(request.Where==S::Scope::Personal)container=E::Owned(controller,TEXT("PersonalInventoryComponent"),TEXT("/Script/Dominion.PersonalInventoryComponent"));
        else container=E::Owned(controller,TEXT("LoadoutComponent"),TEXT("/Script/Dominion.LoadoutComponent"));
    }catch(const std::exception&){return {nullptr,nullptr,nullptr,nullptr,S::Refusal("Container",std::string("The character's ")+std::string(S::Name(request.Where))+" container is unavailable.")};}
    const auto slots=E::Slots(container);
    if(static_cast<size_t>(request.Slot)>=slots.size()||!slots[request.Slot])return {nullptr,nullptr,nullptr,nullptr,S::Refusal("Slot","That slot is empty or out of range.")};
    auto* item=slots[request.Slot];
    auto* base=AH::ResolveClass(TEXT("/Script/Dominion.Item"));
    if(!base||!item->IsA(base)||item->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed)))return {nullptr,nullptr,nullptr,nullptr,S::Refusal("Slot","That slot does not hold a live item instance.")};
    if(GuidText(E::Identity(item))!=request.Instance)return {nullptr,nullptr,nullptr,nullptr,S::Refusal("Instance","The slot holds a different item instance.")};
    auto* data=E::Data(item);
    if(!data||RC::to_string(data->GetPathName())!=request.Item)return {nullptr,nullptr,nullptr,nullptr,S::Refusal("Item","The slot holds a different item definition.")};
    return {controller,container,item,data,{}};
}
nlohmann::json Read(const S::Request& request){
    const auto live=Resolve(request);if(!live.Error.is_null())return live.Error;
    auto* item=live.Item;auto* data=live.Data;
    auto* type=item->GetClassPrivate();std::vector<S::Field> fields;
    for(auto* property:TFieldRange<FProperty>(type,EFieldIterationFlags::IncludeSuper)){
        if(!property->HasAnyPropertyFlags(CPF_SaveGame))continue;
        const auto name=RC::to_string(property->GetName());
        if(fields.size()>=S::MaxFields||!C::Fits(property,type->GetPropertiesSize())||static_cast<size_t>(property->GetSize())>S::MaxValueBytes)
            return S::Refusal("Layout","Unsupported native item property layout: "+name);
        TArray<const FStructProperty*> strong,weak;
        if(property->ContainsObjectReference(strong,EPropertyObjectReferenceType::Strong)||property->ContainsObjectReference(weak,EPropertyObjectReferenceType::Weak))
            return S::Refusal("Layout","Saved item object references need a lossless codec: "+name);
        try{fields.push_back(Capture(property,item));}catch(const std::exception& error){return S::Refusal("Value",error.what());}
    }
    nlohmann::json snapshot;
    try{snapshot=S::Build(RC::to_string(data->GetPathName()),RC::to_string(type->GetPathName()),request.Instance,std::move(fields));}
    catch(const std::exception& error){return S::Refusal("Layout",error.what());}
    snapshot["Ok"]=true;
    snapshot["Observed"]={{"Player",request.Player},{"Scope",S::Name(request.Where)},{"Slot",request.Slot},{"InMemory",true}};
    return snapshot;
}
int32_t Call(const char* request,char* response,uint32_t capacity,uint32_t* size){
    if(!request||!size)return RS_PLUGIN_INVALID_ARGUMENT;
    nlohmann::json reply;
    try{
        nlohmann::json input;
        try{input=nlohmann::json::parse(request);}catch(...){return RS_PLUGIN_INVALID_ARGUMENT;}
        S::Request parsed;
        try{parsed=S::Parse(input);}catch(const std::exception& error){reply=S::Refusal("Request",error.what());}
        if(reply.is_null())reply=Read(parsed);
    }catch(const std::exception& error){reply=S::Refusal("Native",error.what());}
    catch(...){reply=S::Refusal("Native","Native item snapshot failed.");}
    const auto text=reply.dump();
    if(text.size()+1>RUNESCHEMA_PLUGIN_MAX_MESSAGE)return RS_PLUGIN_FAILED;
    *size=static_cast<uint32_t>(text.size()+1);
    if(!response||capacity<*size)return RS_PLUGIN_BUFFER_TOO_SMALL;
    std::memcpy(response,text.c_str(),*size);return RS_PLUGIN_OK;
}
}
