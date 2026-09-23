#include "Loader/AssetAuthoringMetadata.h"
#include "Generator/AssetSearch.h"
#include "Generator/ItemCatalogMetadata.h"
#include "Generator/QuickMenuDecorations.h"
#include "SDK/Helper/CookedAssetLookup.h"
#include "SDK/Helper/ItemAppearanceMetadata.h"
#include "Loader/AssetProvenance.h"
#include "Generator/AssetTemplate.h"
#include "Generator/LoaderTemplate.h"
#include "Generator/LoaderSchemas.h"
#include "Core/ConfigFiles.h"
#include "Runtime/HostServices.h"
#include <filesystem>
#include "SDK/Helper/StringTableHelper.h"
#include "SDK/Helper/ActorHelper.h"
#include "Unreal/Engine/UDataTable.hpp"
#include "Unreal/Property/FEnumProperty.hpp"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Structs/FSoftObjectPtr.h"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/Property/FTextProperty.hpp"
#include "Unreal/FText.hpp"
#include "Helpers/String.hpp"
#include <cmath>
#include <initializer_list>
#include <set>

using namespace RC;using namespace RC::Unreal;using namespace DragonWilds;
using nlohmann::json;
namespace PS::AssetSearch {
namespace {
bool Usable(UObject* object) {
    return object && object->GetClassPrivate() && !object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_NeedLoad|RF_NeedPostLoad|RF_NeedInitialization|RF_BeginDestroyed|RF_FinishDestroyed));
}
json Value(FProperty* p,void* container,int size,bool references=true) {
    if(!p || p->GetArrayDim()!=1 || p->GetOffset_Internal()<0)return nullptr;
    if(p->GetElementSize()<=0 || p->GetOffset_Internal()>size || p->GetElementSize()>size-p->GetOffset_Internal())return nullptr;
    auto* address=p->ContainerPtrToValuePtr<void>(container);
    if(CastField<FTextProperty>(p)) {
        auto text=to_string(PropertyHelper::GetTextAsString(*static_cast<FText*>(address)));
        return text.size()<=4096?json(text):json(nullptr);
    }
    if(CastField<FStrProperty>(p)) {
        const auto& chars=static_cast<FString*>(address)->GetCharArray();
        if(chars.Num()<0 || chars.Num()>4097 || (chars.Num() && !chars.GetData()))return nullptr;
        return chars.Num()?json(to_string(StringType(chars.GetData(),chars.Num()-1))):json("");
    }
    if(auto* b=CastField<FBoolProperty>(p))return b->GetPropertyValue(address);
    if(auto* n=CastField<FNumericProperty>(p)) {
        if(n->IsInteger())return n->GetSignedIntPropertyValue(address);
        const auto value=n->GetFloatingPointPropertyValue(address);return std::isfinite(value)?json(value):json(nullptr);
    }
    if(auto* array=CastField<FArrayProperty>(p)) {
        auto* scriptArray=static_cast<FScriptArray*>(address);
        const auto count=scriptArray?scriptArray->Num():-1;
        auto* inner=array->GetInner();
        // Unlockers may use hard RecipeData references or soft asset links.
        // Handle only flat reference arrays; never recurse into arbitrary structs.
        const bool supportedReference = inner &&
            ((CastField<FSoftObjectProperty>(inner) && inner->GetElementSize()==sizeof(UECustom::FSoftObjectPtr))
                || (CastField<FObjectProperty>(inner) && inner->GetElementSize()==sizeof(UObject*)));
        if(!references || count<0 || count>64 || !inner || inner->GetOffset_Internal()!=0
            || !supportedReference
            || (count>0 && !scriptArray->GetData()))return nullptr;
        json values=json::array();
        const auto stride=inner->GetElementSize();
        for(int32 index=0;index<count;++index) {
            auto value=Value(inner,static_cast<uint8*>(scriptArray->GetData())+static_cast<size_t>(index)*stride,
                stride,references);
            if(value.is_null())return nullptr;
            values.push_back(std::move(value));
        }
        return values;
    }
    if(references && CastField<FSoftObjectProperty>(p) && p->GetElementSize()==sizeof(UECustom::FSoftObjectPtr)) {
        const auto& path=static_cast<UECustom::FSoftObjectPtr*>(address)->ObjectID;
        const auto package=to_string(path.GetLongPackageFName().ToString());
        const auto asset=to_string(path.GetAssetFName().ToString());
        if(package.starts_with('/') && path.SubPathString.GetCharArray().Num()<=1)
            return {{"AssetPathName",package+"."+asset},{"SubPathString",""}};
    }
    if(references && p->GetElementSize()==sizeof(UObject*)) {
        if(auto* hardReference=CastField<FObjectProperty>(p)) {
            auto* linked=hardReference->GetObjectPropertyValue(address);
            if(linked && !linked->HasAnyFlags(static_cast<EObjectFlags>(RF_NeedLoad|RF_NeedPostLoad|RF_NeedInitialization|RF_BeginDestroyed|RF_FinishDestroyed)))
                return to_string(linked->GetPathName());
        }
    }
    return nullptr;
}
json Read(UObject* object,const char* name) {
    return Value(PropertyHelper::GetPropertyByName(object->GetClassPrivate(),to_wstring(name)),object,object->GetClassPrivate()->GetPropertiesSize());
}
std::string Text(UObject* object,const char* name) {auto value=Read(object,name);return value.is_string()?value.get<std::string>():std::string{};}
// Read reflected tag structures without guessing offsets or walking arbitrary object graphs.
std::string TagValue(void* container,int size,FProperty* raw) {
    auto* property=CastField<FStructProperty>(raw);
    if(!property||property->GetArrayDim()!=1||!property->GetStruct())return {};
    const auto offset=property->GetOffset_Internal(),stride=property->GetElementSize();
    if(offset<0||stride<=0||offset>size||stride>size-offset)return {};
    auto* tag=CastField<FNameProperty>(PropertyHelper::GetPropertyByName(property->GetStruct().Get(),TEXT("TagName")));
    if(!tag||tag->GetArrayDim()!=1||tag->GetElementSize()!=sizeof(FName)||tag->GetOffset_Internal()<0
        ||tag->GetOffset_Internal()>stride||tag->GetElementSize()>stride-tag->GetOffset_Internal())return {};
    auto* value=property->ContainerPtrToValuePtr<void>(container);
    return to_string(tag->ContainerPtrToValuePtr<FName>(value)->ToString());
}
std::string ItemTags(UObject* object) {
    auto* type=object->GetClassPrivate();
    std::string tags=TagValue(object,type->GetPropertiesSize(),PropertyHelper::GetPropertyByName(type,TEXT("Category")));
    auto* container=CastField<FStructProperty>(PropertyHelper::GetPropertyByName(type,TEXT("ItemFilterTags")));
    if(!container||!container->GetStruct()||container->GetArrayDim()!=1)return tags;
    const auto offset=container->GetOffset_Internal(),size=container->GetElementSize();
    if(offset<0||size<=0||offset>type->GetPropertiesSize()||size>type->GetPropertiesSize()-offset)return tags;
    auto* array=CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(container->GetStruct().Get(),TEXT("GameplayTags")));
    if(!array||array->GetArrayDim()!=1||array->GetOffset_Internal()<0||array->GetElementSize()!=sizeof(FScriptArray)
        ||array->GetOffset_Internal()>size||array->GetElementSize()>size-array->GetOffset_Internal())return tags;
    auto* inner=CastField<FStructProperty>(array->GetInner());
    if(!inner||inner->GetOffset_Internal()!=0||inner->GetArrayDim()!=1||inner->GetElementSize()<=0)return tags;
    auto* data=container->ContainerPtrToValuePtr<void>(object);
    auto* values=array->ContainerPtrToValuePtr<FScriptArray>(data);
    if(!values||values->Num()<0||values->Num()>256||(values->Num()&&!values->GetData()))return tags;
    for(int i=0;i<values->Num();++i) {
        auto value=TagValue(static_cast<uint8_t*>(values->GetData())+static_cast<std::size_t>(i)*inner->GetElementSize(),inner->GetElementSize(),inner);
        if(!value.empty())tags+=" "+value;
    }
    return tags;
}
bool ItemBool(UObject* object,std::initializer_list<const char*> names) {
    for(const auto* name:names) {const auto value=Read(object,name);if(value.is_boolean()&&value.get<bool>())return true;}
    return false;
}
std::string ReferencePath(UObject* object,const char* name) {
    const auto value=Read(object,name);
    if(value.is_string())return value.get<std::string>();
    if(value.is_object() && value.contains("AssetPathName") && value["AssetPathName"].is_string())return value["AssetPathName"].get<std::string>();
    return {};
}
UClass* Class(const std::string& loader) {
    const auto* path=loader=="raw"?TEXT("/Script/Engine.DataTable"):loader=="blueprints"?TEXT("/Script/Engine.BlueprintGeneratedClass"):
        loader=="players"?TEXT("/Script/Engine.PlayerState"):loader=="buildings"?TEXT("/Script/Dominion.BuildingPieceData"):
        TEXT("/Script/Dominion.ItemData");
    auto* cls=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,path);
    if(!cls)throw std::runtime_error("Type unavailable. Enter a world first.");return cls;
}
json Available(UStruct* type) {
    json result=json::object();size_t count=0;
    for(auto* p:TFieldRange<FProperty>(type,EFieldIterationFlags::Default)) {
        if(++count>256)break;
        json field={{"description",PropertyHelper::GetPropertyTypeAsUTF8String(p)}};
        if(CastField<FBoolProperty>(p))field["type"]="boolean";
        else if(auto* n=CastField<FNumericProperty>(p))field["type"]=n->IsInteger()?"integer":"number";
        else if(CastField<FStrProperty>(p)||CastField<FTextProperty>(p)||CastField<FNameProperty>(p))field["type"]="string";
        result[to_string(p->GetName())]=field;
    }return result;
}
std::string PlayerName(UObject* player) {
    ActorHelper::FunctionCall call(player,TEXT("/Script/Engine.PlayerState:GetPlayerName"));call.Invoke();
    const auto value=call.Result<FString>();
    const auto& chars=value.GetCharArray();
    if(chars.Num()<1 || chars.Num()>4097 || !chars.GetData())return {};
    return to_string(StringType(chars.GetData(),chars.Num()-1));
}
}
json Search(const std::string& query,const std::string& loader) {
    LoaderTemplate::Help(loader);
    if(query.size()>96 || AssetTemplate::Fold(query).size()<2)throw std::runtime_error("Enter at least two letters or digits.");
    json rows=json::array();
    if(AuthoredStarters::Supports(loader)) {
        const auto root=std::filesystem::path(PS::HostServices::WorkingDirectory())/"Mods/RuneSchema/mods";
        size_t files=0,bytes=0,scanned=0,reportBytes=0;
        for(const auto& mod:std::filesystem::directory_iterator(root)) {
            if(++scanned>512)throw std::runtime_error("Mod search exceeds 512 folders.");
            if(mod.is_symlink() || !mod.is_directory())continue;
            const auto folder=mod.path()/loader;if(!std::filesystem::exists(folder)||std::filesystem::is_symlink(folder))continue;
            for(const auto& file:std::filesystem::directory_iterator(folder)) {
                if(++files>256)throw std::runtime_error("Definition search exceeds 256 files.");
                if(file.is_symlink() || !file.is_regular_file() || (file.path().extension()!=".json"&&file.path().extension()!=".jsonc"))continue;
                const auto text=PS::ConfigFiles::Read(file.path(),262144);
                if((bytes+=text.size())>4*1024*1024)throw std::runtime_error("Definition search exceeds 4 MiB.");
                size_t nodes=0;
                const auto doc=json::parse(text,[&](int depth,json::parse_event_t,json&){if(depth>24||++nodes>32768)throw std::runtime_error("Definition structure exceeds limits.");return true;},true,true);
                for(auto entry:AuthoredStarters::Entries(doc,loader,query,mod.path().filename().string())) {
                    const auto target=entry.at("Target").get<std::string>();
                    if((reportBytes+=entry.dump().size())>1024*1024)throw std::runtime_error("Search results exceed 1 MiB; narrow the query.");
                    entry["Authored"]=true;entry["Path"]=file.path().string();entry["Key"]=file.path().string()+":"+target;entry["Name"]=target;
                    rows.push_back(std::move(entry));if(rows.size()>=100)return rows;
                }
            }
        }return rows;
    }
    if(loader=="strings") {
        struct Enough{};
        try {StringTableHelper::ForEachEntry([&](UObject* table,FString& value) {
            if(value.Len()<1 || value.Len()>4096)return;
            const auto text=to_string(StringType(*value,value.Len())),path=to_string(table->GetPathName());
            if(!AssetTemplate::Matches(query,path+" "+text))return;
            rows.push_back({{"Path",path},{"Key",path+":"+text},{"Name",text},{"Text",text},{"Table",to_string(table->GetName())}});
            if(rows.size()>=100)throw Enough{};
        },256,65536);}catch(const Enough&){}
        return rows;
    }
    TArray<UObject*> objects;UECustom::UObjectGlobals::GetObjectsOfClass(Class(loader),objects,true);
    if(objects.Num()>32768)throw std::runtime_error("Loaded item limit exceeded.");
    size_t visitedRows=0;
    for(auto* object:objects) {
        if(!Usable(object))continue;
        const auto path=to_string(object->GetPathName());
        const auto target=to_string(object->GetName());
        if(loader=="raw") {
            auto* table=static_cast<UDataTable*>(object);
            if(table->GetRowMap().Num()<0 || (visitedRows+=table->GetRowMap().Num())>65536)throw std::runtime_error("Row search exceeds 65536 rows; use a smaller loaded data set.");
            for(const auto& [key,rowData]:table->GetRowMap()) {
                const auto row=to_string(key.ToString());
                if(!AssetTemplate::Matches(query,path+" "+row))continue;
                rows.push_back({{"Path",path},{"Key",path+":"+row},{"Name",row},{"Row",row},{"Target",target}});
                if(rows.size()>=100)return rows;
            }continue;
        }
        if(loader=="blueprints" || loader=="players") {
            const auto name=loader=="players"?PlayerName(object):target;
            if(name.empty() || !AssetTemplate::Matches(query,path+" "+name))continue;
            rows.push_back({{"Path",path},{"Key",path},{"Name",name},{"Target",target}});
            if(rows.size()>=100)break;continue;
        }
        const auto internal=Text(object,"InternalName");auto name=Text(object,"Name");
        if(name.empty())name=Text(object,"DisplayName");
        if(!AssetTemplate::Matches(query,path+" "+internal+" "+name))continue;
        rows.push_back({{"Path",path},{"Key",path},{"Name",name},{"InternalName",internal},
            {"CloneProvenance",AssetProvenance::Lookup(object)}});
        if(rows.size()==100)break;
    }
    return rows;
}

json DescribeItem(UObject* object) {
    if(!Usable(object))return nullptr;
    const auto path=to_string(object->GetPathName());
    auto name=Text(object,"Name");if(QuickDecorations::MissingDisplayName(name))name=Text(object,"DisplayName");
    if(QuickDecorations::MissingDisplayName(name))name=QuickDecorations::ReadableAssetName(to_string(object->GetName()));
    const auto provenance=AssetProvenance::Lookup(object);
    const bool runtimeClone=provenance.is_object()&&provenance.value("Kind",std::string{})=="RuneSchemaAssetClone"
        &&provenance.value("Confirmed",false);
    const auto declared=AssetMetadata::Lookup(object);
    const bool cooked=CookedAssets::VerifiedLoadedObject(object);
    std::string cloneReason;
    const bool cloneEligible=AssetMetadata::Allowed(declared,cooked,runtimeClone,
        provenance.is_object()&&provenance.value("Registered",false),QuickDecorations::ModPath(path),
        AssetMetadata::HasInstalledDefinition(object),false,cloneReason);
    const auto tags=ItemTags(object);
    std::string classes;std::size_t depth=0;
    for(UStruct* ancestor=object->GetClassPrivate();ancestor&&depth++<64;ancestor=ancestor->GetSuperStruct())classes+=" "+to_string(ancestor->GetName());
    const auto classText=QuickDecorations::Fold(classes);
    const bool consumable=QuickDecorations::Token(tags,"consumable")||classText.find("consumable")!=std::string::npos;
    const bool quest=QuickDecorations::Token(tags,"quest")||ItemBool(object,{"bIsQuestItem","IsQuestItem","bQuestItem"});
    // Do not infer quality from PowerLevel or a user-editable display name.
    const bool masterwork=QuickDecorations::Token(tags,"masterwork")||ItemBool(object,{"bIsMasterworkItem","bIsMasterwork","IsMasterwork","bMasterwork","Masterwork"});
    return {{"Name",name},{"Path",path},{"InternalName",Text(object,"InternalName")},
        {"PersistenceID",Text(object,"PersistenceID")},{"Icon",ReferencePath(object,"Icon")},
        {"RuneSchema",runtimeClone||declared.runeSchema.value_or(AssetMetadata::IsManaged(object))},{"RuntimeClone",runtimeClone},
        {"RuneSchemaManaged",runtimeClone||declared.runeSchema.value_or(AssetMetadata::IsManaged(object))},
        {"DeclaredModded",declared.modded.value_or(false)},{"DeclaredCooked",declared.cooked.value_or(false)},
        {"CloneEligible",cloneEligible&&!AssetMetadata::IsIncomplete(object)},{"CloneReason",cloneReason},{"Modded",AssetMetadata::Encode(declared)},
        {"Available",!AssetMetadata::IsIncomplete(object)&&(!runtimeClone||provenance.value("Registered",false))},
        {"Reason",AssetMetadata::IsIncomplete(object)?"Helpy companion files incomplete; grants and cloning blocked until repaired":runtimeClone&&!provenance.value("Registered",false)?"Runtime clone is not registered with the item subsystem":""},
        {"Masterwork",masterwork},{"Consumable",consumable},{"QuestItem",quest},
        {"CategoryIcon",ReferencePath(object,"CategoryClassIcon")},{"Tags",tags},
        {"bSoftDeleted",ItemBool(object,{"bSoftDeleted","IsSoftDeleted","bIsSoftDeleted","IsSoftDelete","bIsSoftDelete"})},
        {"Class",to_string(object->GetClassPrivate()->GetPathName())},
        {"Cooked",CookedAssets::VerifiedLoadedObject(object)},{"AppearanceGroup",ItemAppearanceMetadata::Group(object)},
        {"PowerLevel",Read(object,"PowerLevel")}};
}

json ItemRoster() {
    json rows=json::array();
    TArray<UObject*> objects;UECustom::UObjectGlobals::GetObjectsOfClass(Class("assets"),objects,true);
    if(objects.Num()<0||objects.Num()>32768)throw std::runtime_error("Loaded item roster exceeds its safe bound.");
    std::set<std::string> paths;
    for(auto* object:objects) {
        if(!Usable(object))continue;
        const auto path=to_string(object->GetPathName());
        if(!paths.insert(path).second)continue;
        auto entry=DescribeItem(object);if(entry.is_object())rows.push_back(std::move(entry));
    }
    std::sort(rows.begin(),rows.end(),[](const json& left,const json& right){
        return left.value("Name",std::string{})<right.value("Name",std::string{});
    });
    return rows;
}
json Capture(const json& entry,const std::string& loader,bool readValues) {
    LoaderTemplate::Help(loader);
    if(AuthoredStarters::Supports(loader)) {
        const auto fields=loader=="enums"?json{{"Values",entry.at("Body")}}:entry.at("Body");
        return {{"Values",readValues?fields:json::object()},{"Available",AuthoredStarters::Reference(fields)}};
    }
    const auto path=entry.at("Path").get<std::string>();
    auto* object=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,to_wstring(path).c_str());
    if(loader=="strings") {
        if(!Usable(object))throw std::runtime_error("String table unloaded. Search again.");
        return {{"Values",{{"Replacement",entry.at("Text")}}},{"Available",{{"Replacement",{{"type","string"}}}}}};
    }
    if(!Usable(object) || !object->IsA(Class(loader)))throw std::runtime_error("Object unloaded. Search again.");
    if(loader=="players") {
        if(PlayerName(object)!=entry.at("Name").get<std::string>())throw std::runtime_error("Player changed. Search again.");
        return {{"Values",{{"Nameplate",{{"Mode","Name"},{"Client","Yes"},{"Server","Yes"}}}}},
            {"Available",JsonSchemaGenerator::LoaderSchemas().at("players").at("items").at("properties")}};
    }
    UStruct* type=object->GetClassPrivate();void* data=object;
    if(loader=="raw" || loader=="blueprints") {
        TArray<UObject*> matches;UECustom::UObjectGlobals::GetObjectsOfClass(Class(loader),matches,true);
        if(matches.Num()>32768)throw std::runtime_error("Object inventory exceeds capture limit.");
        size_t count=0;for(auto* match:matches)if(Usable(match)&&match->GetFName()==object->GetFName())++count;
        if(count!=1)throw std::runtime_error("Duplicate short names; this loader cannot select the target unambiguously.");
        if(loader=="raw") {
            auto* table=static_cast<UDataTable*>(object);type=table->GetRowStruct().Get();
            data=table->FindRowUnchecked(FName(to_wstring(entry.at("Row").get<std::string>()),FNAME_Find));
        }else {type=static_cast<UClass*>(object);data=static_cast<UClass*>(object)->GetClassDefaultObject();}
        if(!type || !data)throw std::runtime_error("Selected row/defaults unavailable.");
        json fields=json::object();size_t countFields=0;
        if(readValues)for(auto* p:TFieldRange<FProperty>(type,EFieldIterationFlags::Default)) {
            if(++countFields>256 || fields.size()>=64)break;
            if(p->HasAnyPropertyFlags(static_cast<EPropertyFlags>(CPF_Transient|CPF_Deprecated)))continue;
            auto value=Value(p,data,type->GetPropertiesSize(),false);
            if(!value.is_null())fields[to_string(p->GetName())]=std::move(value);
        }
        return {{"Values",fields},{"Available",Available(type)}};
    }
    json fields=json::object();
    if(readValues)for(const auto* name:loader=="buildings"?std::vector<const char*>{"DisplayName","Description","DisplayIcon","ConstructionXPOnBuild"}:
        std::vector<const char*>{"Name","FlavourText","Icon","MaxStackSize","Weight","bGoIntoHotbarOnPickup","bDropOnDeath",
            "RecipesToUnlock","BuildingPieceToUnlock","ArmorWeight","bIsMasterworkItem","MasterworkType",
            "BuffDatas","GrantedEffects","bSoftDeleted"}) {
        auto value=Read(object,name);if(!value.is_null())fields[name]=std::move(value);
    }
    return {{"Values",fields},{"Available",Available(type)}};
}
json Reference(const json& entry,const std::string& loader) {
    LoaderTemplate::Help(loader);
    if(AuthoredStarters::Supports(loader) || loader=="strings" || loader=="players") {
        auto captured=Capture(entry,loader,true);
        if(loader=="players")captured["Values"]={{"PlayerName",entry.at("Name")}};
        return ReferenceExport::Build(loader,entry,captured["Values"],captured["Available"]);
    }
    auto* object=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,to_wstring(entry.at("Path").get<std::string>()).c_str());
    if(!Usable(object) || !object->IsA(Class(loader)))throw std::runtime_error("Object unloaded; search again.");
    UStruct* type=object->GetClassPrivate();void* data=object;
    if(loader=="raw") {
        auto* table=static_cast<UDataTable*>(object);type=table->GetRowStruct().Get();
        data=table->FindRowUnchecked(FName(to_wstring(entry.at("Row").get<std::string>()),FNAME_Find));
    } else if(loader=="blueprints") {type=static_cast<UClass*>(object);data=static_cast<UClass*>(object)->GetClassDefaultObject();}
    if(!type || !data)throw std::runtime_error("Record data unavailable.");
    json values=json::object();size_t count=0;bool truncated=false;
    for(auto* field:TFieldRange<FProperty>(type,EFieldIterationFlags::Default)) {
        if(++count>256){truncated=true;break;}
        const auto name=to_string(field->GetName());values[name]=nullptr;
        const auto size=field->GetElementSize(),offset=field->GetOffset_Internal();
        if(field->GetArrayDim()!=1 || size<=0 || offset<0 || offset>type->GetPropertiesSize()
            || size>type->GetPropertiesSize()-offset
            || field->HasAnyPropertyFlags(static_cast<EPropertyFlags>(CPF_Transient|CPF_Deprecated)))continue;
        auto* address=field->ContainerPtrToValuePtr<void>(data);
        if(auto* ref=CastField<FObjectProperty>(field);ref && size==sizeof(UObject*)) {
            auto* linked=ref->GetObjectPropertyValue(address);
            if(linked)values[name]=to_string(linked->GetPathName());
        } else if(CastField<FNameProperty>(field) && size==sizeof(FName)) {
            values[name]=to_string(static_cast<FName*>(address)->ToString());
        } else {
            if(CastField<FTextProperty>(field) && size!=sizeof(FText))continue;
            if(CastField<FStrProperty>(field) && size!=sizeof(FString))continue;
            if(auto* boolean=CastField<FBoolProperty>(field);boolean
                && (!boolean->IsNativeBool() || size!=sizeof(bool) || boolean->GetByteOffset()!=0))continue;
            if(auto* number=CastField<FNumericProperty>(field);number
                && size!=1 && size!=2 && size!=4 && size!=8)continue;
            if(auto* number=CastField<FNumericProperty>(field);number
                && (CastField<FByteProperty>(field) || CastField<FUInt16Property>(field)
                    || CastField<FUInt32Property>(field) || CastField<FUInt64Property>(field))) {
                values[name]=number->GetUnsignedIntPropertyValue(address);continue;
            }
            values[name]=Value(field,data,type->GetPropertiesSize());
        }
    }
    auto source=entry;source["ReflectedType"]=to_string(type->GetPathName());
    auto report=ReferenceExport::Build(loader,source,values,Available(type));
    if(auto provenance=AssetProvenance::Lookup(object);!provenance.is_null()) {
        report["Origin"]="Confirmed RuneSchema asset clone";
        report["CloneProvenance"]=std::move(provenance);
    }
    report["Truncated"]=truncated || report["Truncated"].get<bool>();
    report["CaptureLimits"]="256 reflected fields; arrays are expanded only when their elements are supported references or scalar values; no struct expansion, asset loading or continuous sampling.";
    return report;
}
}
