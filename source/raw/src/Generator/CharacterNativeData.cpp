#include "Generator/CharacterNativeData.h"
#include "Core/CharacterWorkbench.h"
#include "Core/CharacterSkillEdit.h"
#include "SDK/Helper/EquipmentDelivery.h"
#include "SDK/WeakObjectHandle.h"
#include "Unreal/World.hpp"
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
#include <Helpers/Casting.hpp>
#include <Helpers/String.hpp>
#include <map>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <charconv>
#include <set>
#include <format>
// Native character data read by Helpy's character sheet: appearance choices,
// skill levels and the inventory page layout. Game-thread only; nothing here
// spawns actors or touches a live player pawn.
using namespace RC;using namespace RC::Unreal;using Json=nlohmann::json;
namespace PS::CharacterNativeData {namespace {
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
std::vector<std::string> Rows(const CharacterWorkbench::Field& field){
    const auto table=Load(to_wstring(CharacterWorkbench::Table(field)));
    Call names(Find(L"/Script/Engine.Default__DataTableFunctionLibrary"),L"/Script/Engine.DataTableFunctionLibrary:GetDataTableRowNames");names.Obj(L"Table",table).Invoke();
    auto* prop=CastField<FArrayProperty>(names.Prop(L"OutRowNames"));auto* inner=prop?CastField<FNameProperty>(prop->GetInner()):nullptr;auto* a=static_cast<FScriptArray*>(names.Value(L"OutRowNames"));
    if(!inner||inner->GetSize()!=sizeof(FName)||!a||a->Num()<0||a->Num()>1024||(a->Num()&&!a->GetData()))throw std::runtime_error("Native customization rows changed");
    std::vector<std::string> result;for(int i=0;i<a->Num();++i){auto text=to_string(reinterpret_cast<FName*>(a->GetData())[i].ToString());if(CharacterWorkbench::Identifier(text))result.push_back(text);}
    const auto order=[](const std::string& value){const auto at=value.find_first_of("0123456789");int number=2147483647;if(at!=value.npos){const auto parsed=std::from_chars(value.data()+at,value.data()+value.size(),number);if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size())number=2147483647;}return std::tuple{value.substr(0,at),number,value};};
    std::sort(result.begin(),result.end(),[&](const auto& a,const auto& b){return order(a)<order(b);});return result;
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
Json SaveSkillLevel(UObject* controller,const Json& request){
    namespace C=DragonWilds::ReflectedContract;
    const auto edit=CharacterSkillEdit::Parse(request);
    EquipmentDelivery::Call authority(controller,L"/Script/Engine.Actor:HasAuthority",1);authority.Invoke();
    EquipmentDelivery::Call character(controller,L"/Script/Dominion.DominionPlayerControllerBase:GetCharacterGuid",1);character.Invoke();const auto guid=character.Identity();
    CharacterSkillEdit::RequireOwner(edit,std::format("{:08X}{:08X}{:08X}{:08X}",guid[0],guid[1],guid[2],guid[3]),authority.Bool());
    auto* pawn=DragonWilds::ActorHelper::GetObjectRef(controller,L"Pawn");if(!pawn||!controller->GetWorld()||pawn->GetWorld()!=controller->GetWorld())throw std::runtime_error("Load the selected character into a world before saving skills");
    const Json probe={{"GameProgress",{{"Skills",{{"Skills",Json::array({{{"Id",edit.Skill},{"Xp",0}}})}}}}}};
    const auto native=Skills(probe);const auto& row=native.at("Items").at(0);
    if(!row.value("Resolved",false)||edit.Level>row.at("MaxLevel").get<int>())throw std::runtime_error("The skill or requested level is unavailable in the native game");
    const int desired=native.at("Thresholds").at(edit.Level-1).get<int>();
    auto* data=Load(to_wstring(row.at("Path").get<std::string>()));
    auto* component=EquipmentDelivery::Owned(controller,L"SkillComponent",L"/Script/Dominion.SkillComponent");
    auto* field=CastField<FArrayProperty>(Member(component,L"Skills"));auto* inner=field?CastField<FStructProperty>(field->GetInner()):nullptr;
    auto* type=inner?inner->GetStruct().Get():nullptr;
    if(!field||field->GetSize()!=sizeof(FScriptArray)||!type||type->GetPropertiesSize()<1||type->GetPropertiesSize()>512||type->GetPathName()!=L"/Script/Dominion.Skill"||inner->GetElementSize()!=type->GetPropertiesSize())throw std::runtime_error("Native skills array contract changed");
    auto* xp=CastField<FIntProperty>(type->FindProperty(FName(L"CurrentXP",FNAME_Find)));auto* skill=CastField<FObjectProperty>(type->FindProperty(FName(L"SkillData",FNAME_Find)));
    if(!C::Fits(xp,type->GetPropertiesSize())||xp->GetSize()!=sizeof(int32_t)||!C::Fits(skill,type->GetPropertiesSize())||skill->GetSize()!=sizeof(UObject*)||!skill->GetPropertyClass()||!data->IsA(skill->GetPropertyClass().Get()))throw std::runtime_error("Native skill XP fields changed");
    auto* array=field->ContainerPtrToValuePtr<FScriptArray>(component);if(!array||array->Num()<1||array->Num()>128||!array->GetData())throw std::runtime_error("Native skills are not loaded");
    int32_t* target=nullptr;for(int i=0;i<array->Num();++i){auto* record=static_cast<uint8_t*>(array->GetData())+size_t(i)*inner->GetElementSize();if(skill->GetObjectPropertyValue(skill->ContainerPtrToValuePtr<void>(record))==data){if(target)throw std::runtime_error("Native skill identity is ambiguous");target=xp->ContainerPtrToValuePtr<int32_t>(record);}}
    if(!target||*target<0)throw std::runtime_error("Selected character has no valid native skill XP");
    Call notify(component,L"/Script/Dominion.SkillComponent:OnRep_Skills");
    auto* previous=CastField<FArrayProperty>(notify.Prop(L"PreviousSkills"));auto* previousInner=previous?CastField<FStructProperty>(previous->GetInner()):nullptr;
    if(C::Count(notify.Fn)!=1||!C::Input(previous,notify.Fn)||!previousInner||previousInner->GetStruct().Get()!=type)throw std::runtime_error("Native skill notification contract changed");
    previous->CopyCompleteValue(notify.Value(L"PreviousSkills"),array);
    Call levelCheck(component,L"/Script/Dominion.SkillComponent:GetSkillLevel");levelCheck.Obj(L"SkillData",data);(void)levelCheck.Integer();if(C::Count(levelCheck.Fn)!=2)throw std::runtime_error("Native skill level getter changed");
    auto* persistence=DragonWilds::ActorHelper::GetObjectRef(controller,L"PersistenceSubsystem");Call save(persistence,L"/Script/Dominion.PersistenceSubsystem:SavePlayerState");
    if(C::Count(save.Fn)!=1||!C::Input(save.Prop(L"DomPC"),save.Fn))throw std::runtime_error("Native character save contract changed");save.Obj(L"DomPC",controller);
    const WeakObjectHandle controllerIdentity(controller),pawnIdentity(pawn),componentIdentity(component),worldIdentity(controller->GetWorld());
    const auto stillOwned=[&]{return controllerIdentity.Get()==controller&&pawnIdentity.Get()==pawn&&componentIdentity.Get()==component&&worldIdentity.Get()==controller->GetWorld()
        &&DragonWilds::ActorHelper::GetObjectRef(controller,L"Pawn")==pawn&&DragonWilds::ActorHelper::GetObjectRef(controller,L"SkillComponent")==component;};
    if(!stillOwned())throw std::runtime_error("Character changed before applying skills");
    const int before=*target;
    // Notify with an owned copy of the previous array; never serialize an old full-character draft.
    if(before!=desired){*target=desired;notify.Invoke();}
    if(!stillOwned())throw std::runtime_error("Character changed during skill update; save was not requested");
    auto* current=field->ContainerPtrToValuePtr<FScriptArray>(component);
    if(!current||current->Num()<1||current->Num()>128||!current->GetData())throw std::runtime_error("Native skills changed during update");
    int matched=0;for(int i=0;i<current->Num();++i){auto* record=static_cast<uint8_t*>(current->GetData())+size_t(i)*inner->GetElementSize();if(skill->GetObjectPropertyValue(skill->ContainerPtrToValuePtr<void>(record))==data){++matched;if(*xp->ContainerPtrToValuePtr<int32_t>(record)!=desired)throw std::runtime_error("Native skill XP read-back disagrees; inspect the character before retrying");}}
    if(matched!=1)throw std::runtime_error("Native skill identity changed during update");
    levelCheck.Invoke();if(levelCheck.Integer()!=edit.Level)throw std::runtime_error("Skill changed but native level read-back disagrees; inspect the character before retrying");
    Json result={{"CharacterGuid",edit.Character},{"SkillId",edit.Skill},{"Level",edit.Level},{"XP",desired},{"Applied",true},{"SaveRequested",false}};
    try{if(!stillOwned())throw std::runtime_error("Character changed before its native save request");save.Invoke();result["SaveRequested"]=true;result["Status"]="Skill applied; native character save requested on its owning platform.";}
    catch(const std::exception& error){result["Status"]=std::string("Skill applied, but the native save request failed: ")+error.what();}
    return result;
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
}
