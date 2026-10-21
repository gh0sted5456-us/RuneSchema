#pragma once
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/CoreUObject/UObject/FStrProperty.hpp>
#include <Unreal/Property/FEnumProperty.hpp>
#include <Unreal/UnrealFlags.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Helpers/Casting.hpp>
#include <Helpers/String.hpp>
#include <nlohmann/json.hpp>
#include <map>
#include <stdexcept>
namespace PS::NativeItemState {
using namespace RC::Unreal;using Json=nlohmann::json;
inline bool Fits(FProperty* p,int size){return p&&p->GetArrayDim()==1&&p->GetOffset_Internal()>=0&&p->GetSize()>0&&p->GetOffset_Internal()<=size&&p->GetSize()<=size-p->GetOffset_Internal();}
inline void Portable(FProperty* p,void* address,int depth=0){
    if(!p||!address||depth>8||p->GetArrayDim()!=1||p->HasAnyPropertyFlags(CPF_InstancedReference|CPF_ContainsInstancedReference|CPF_Deprecated|CPF_EditorOnly))throw std::runtime_error("Unsupported persistent item property");
    if(auto* a=CastField<FArrayProperty>(p)){
        auto* array=static_cast<FScriptArray*>(address);auto* inner=a->GetInner();
        if(!inner||inner->GetOffset_Internal()!=0||inner->GetSize()<1||array->Num()<0||array->Num()>128||(array->Num()&&!array->GetData()))throw std::runtime_error("Persistent item array exceeds its bound");
        for(int i=0;i<array->Num();++i)Portable(inner,static_cast<uint8_t*>(array->GetData())+i*inner->GetSize(),depth+1);return;
    }
    if(auto* s=CastField<FStructProperty>(p)){
        auto* type=s->GetStruct().Get();if(!type||type->GetPropertiesSize()!=p->GetSize())throw std::runtime_error("Persistent item struct layout changed");
        size_t fields=0;for(auto* field:TFieldRange<FProperty>(type,EFieldIterationFlags::Default)){if(++fields>64||!Fits(field,p->GetSize()))throw std::runtime_error("Persistent item struct exceeds its bound");
            Portable(field,field->ContainerPtrToValuePtr<void>(address),depth+1);}return;
    }
    if(auto* o=CastField<FObjectProperty>(p)){
        auto* value=o->GetObjectPropertyValue(address);if(!value)return;const auto path=RC::to_string(value->GetPathName());
        if(!path.starts_with("/Game/")&&!path.starts_with("/Engine/")&&!path.starts_with("/Script/"))throw std::runtime_error("Persistent item references a runtime object");
        if(path.starts_with("/Engine/Transient")||path.find(":")!=path.npos||path.find(".PersistentLevel.")!=path.npos)throw std::runtime_error("Persistent item references a runtime or world object");return;
    }
    if(CastField<FNumericProperty>(p)||CastField<FBoolProperty>(p)||CastField<FStrProperty>(p)||CastField<FNameProperty>(p)||CastField<FEnumProperty>(p))return;
    throw std::runtime_error("Persistent item property needs a verified serializer");
}
inline Json Descriptor(FProperty* p,int depth=0){
    if(!p||depth>8)throw std::runtime_error("Persistent item schema exceeds its bound");
    Json result={{"Kind",RC::to_string(p->GetClass().GetName())},{"Size",p->GetSize()}};
    if(auto* s=CastField<FStructProperty>(p)){result["Struct"]=RC::to_string(s->GetStruct()->GetPathName());auto children=Json::object();
        for(auto* field:TFieldRange<FProperty>(s->GetStruct().Get(),EFieldIterationFlags::Default)){if(children.size()>=64)throw std::runtime_error("Persistent item struct exceeds its bound");children[RC::to_string(field->GetName())]=Descriptor(field,depth+1);}result["Fields"]=std::move(children);}
    if(auto* a=CastField<FArrayProperty>(p))result["Inner"]=Descriptor(a->GetInner(),depth+1);
    if(auto* e=CastField<FEnumProperty>(p)){if(!e->GetEnum())throw std::runtime_error("Persistent item enum unavailable");result["Enum"]=RC::to_string(e->GetEnum()->GetPathName());}
    return result;
}
inline Json NumericBits(FProperty* p,void* address){
    if(auto* n=CastField<FNumericProperty>(p);n&&!n->IsEnum()){
        const auto size=p->GetSize();if(size!=1&&size!=2&&size!=4&&size!=8)throw std::runtime_error("Persistent numeric width changed");
        std::string hex;const auto* bytes=static_cast<const uint8_t*>(address);constexpr char digits[]="0123456789abcdef";
        for(int i=0;i<size;++i){hex+=digits[bytes[i]>>4];hex+=digits[bytes[i]&15];}return hex;
    }
    if(auto* s=CastField<FStructProperty>(p)){auto bits=Json::object();for(auto* field:TFieldRange<FProperty>(s->GetStruct().Get(),EFieldIterationFlags::Default)){
        auto child=NumericBits(field,field->ContainerPtrToValuePtr<void>(address));if(!child.is_null())bits[RC::to_string(field->GetName())]=std::move(child);}return bits.empty()?Json{}:bits;}
    if(auto* a=CastField<FArrayProperty>(p)){auto bits=Json::array();auto* array=static_cast<FScriptArray*>(address);for(int i=0;i<array->Num();++i)
        bits.push_back(NumericBits(a->GetInner(),static_cast<uint8_t*>(array->GetData())+i*a->GetInner()->GetSize()));return bits;}
    return nullptr;
}
inline void ApplyNumericBits(FProperty* p,void* address,const Json& bits){
    if(auto* n=CastField<FNumericProperty>(p);n&&!n->IsEnum()){
        if(!bits.is_string())throw std::runtime_error("Invalid exact numeric state");const auto hex=bits.get<std::string>();const auto size=p->GetSize();
        if((size!=1&&size!=2&&size!=4&&size!=8)||hex.size()!=size_t(size)*2||hex.find_first_not_of("0123456789abcdef")!=hex.npos)throw std::runtime_error("Invalid exact numeric width");
        const auto nibble=[](char c){return c<='9'?c-'0':c-'a'+10;};auto* bytes=static_cast<uint8_t*>(address);
        for(int i=0;i<size;++i)bytes[i]=static_cast<uint8_t>((nibble(hex[i*2])<<4)|nibble(hex[i*2+1]));return;
    }
    if(auto* s=CastField<FStructProperty>(p)){if(bits.is_null())return;if(!bits.is_object())throw std::runtime_error("Invalid exact struct state");
        for(auto* field:TFieldRange<FProperty>(s->GetStruct().Get(),EFieldIterationFlags::Default)){const auto name=RC::to_string(field->GetName());if(bits.contains(name))ApplyNumericBits(field,field->ContainerPtrToValuePtr<void>(address),bits.at(name));}return;}
    if(auto* a=CastField<FArrayProperty>(p)){auto* array=static_cast<FScriptArray*>(address);if(!bits.is_array()||bits.size()!=size_t(array->Num()))throw std::runtime_error("Exact array state changed");
        for(int i=0;i<array->Num();++i)ApplyNumericBits(a->GetInner(),static_cast<uint8_t*>(array->GetData())+i*a->GetInner()->GetSize(),bits[i]);return;}
    if(!bits.is_null())throw std::runtime_error("Unexpected exact numeric state");
}
inline std::map<std::string,FProperty*> Fields(UObject* item){
    if(!item||!item->GetClassPrivate()||item->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))throw std::runtime_error("Current item object unavailable");
    auto* base=UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.Item"));if(!base||!item->IsA(base))throw std::runtime_error("Recovery snapshot requires a native Item");
    std::map<std::string,FProperty*> result;auto* type=item->GetClassPrivate();
    for(auto* p:TFieldRange<FProperty>(type,EFieldIterationFlags::Default))if(p->HasAnyPropertyFlags(CPF_SaveGame)){
        const auto name=RC::to_string(p->GetName());if(result.size()>=64||!Fits(p,type->GetPropertiesSize())||name.size()>128||!result.emplace(name,p).second)throw std::runtime_error("Persistent item field layout is invalid");
        Portable(p,p->ContainerPtrToValuePtr<void>(item));
    }
    if(result.empty())throw std::runtime_error("Item has no persistent fields");return result;
}
inline Json Read(UObject* item){
    Json fields=Json::object();for(const auto& [name,p]:Fields(item)){
        // UE4SS's container export offsets even a null delta; use direct values.
        FString text;p->ExportTextItem(text,p->ContainerPtrToValuePtr<void>(item),nullptr,item,0);
        const auto& chars=text.GetCharArray();if(chars.Num()<0||chars.Num()>8193)throw std::runtime_error("Persistent item text exceeds its bound");
        auto row=Descriptor(p);row["Text"]=RC::to_string(RC::StringType(*text));row["Exact"]=NumericBits(p,p->ContainerPtrToValuePtr<void>(item));fields[name]=std::move(row);
    }
    Json result={{"Format","UEPropertyText2"},{"Class",RC::to_string(item->GetClassPrivate()->GetPathName())},{"ClassSize",item->GetClassPrivate()->GetPropertiesSize()},{"Fields",fields}};
    if(result.dump().size()>64*1024)throw std::runtime_error("Persistent item snapshot exceeds 64 KiB");return result;
}
inline void Apply(UObject* freshItem,const Json& snapshot){
    if(!snapshot.is_object()||snapshot.dump().size()>64*1024||snapshot.value("Format",std::string{})!="UEPropertyText2"||!snapshot.at("Fields").is_object())throw std::runtime_error("Invalid native item snapshot");
    const auto fields=Fields(freshItem);
    auto* references=CastField<FIntProperty>(freshItem->GetClassPrivate()->FindProperty(FName(TEXT("ReferencingInventoriesCount"),FNAME_Find)));
    if(!freshItem->HasAnyFlags(RF_Transient)||!freshItem->GetOuterPrivate()||freshItem->GetOuterPrivate()->GetPathName()!=TEXT("/Engine/Transient")
        ||!Fits(references,freshItem->GetClassPrivate()->GetPropertiesSize())||*references->ContainerPtrToValuePtr<int32_t>(freshItem)!=0)
        throw std::runtime_error("Restore may only initialize a fresh transient item outside all inventories");
    if(snapshot.at("Class")!=RC::to_string(freshItem->GetClassPrivate()->GetPathName())||snapshot.at("ClassSize")!=freshItem->GetClassPrivate()->GetPropertiesSize()||snapshot.at("Fields").size()!=fields.size())throw std::runtime_error("Native item class changed; restore not attempted");
    for(const auto& [name,p]:fields){auto row=snapshot.at("Fields").at(name);const auto text=row.at("Text").get<std::string>();row.erase("Text");row.erase("Exact");
        if(row!=Descriptor(p)||text.size()>8192||text.find('\0')!=text.npos)throw std::runtime_error("Persistent item property contract changed");
    }
    // Only a fresh, unowned item may be passed here. Existing inventory objects
    // must never be rewritten through this recovery serializer.
    for(const auto& [name,p]:fields){const auto text=RC::to_generic_string(snapshot.at("Fields").at(name).at("Text").get<std::string>());
        const auto* end=p->ImportText_InContainer(text.c_str(),freshItem,freshItem,0,nullptr);if(!end||*end)throw std::runtime_error("Native item property import failed");
    }
    // Native text is not lossless for floats or custom DateTime formatting.
    for(const auto& [name,p]:fields){Portable(p,p->ContainerPtrToValuePtr<void>(freshItem));ApplyNumericBits(p,p->ContainerPtrToValuePtr<void>(freshItem),snapshot.at("Fields").at(name).at("Exact"));}
    if(Read(freshItem)!=snapshot)throw std::runtime_error("Native item state round-trip was not exact");
}
}
