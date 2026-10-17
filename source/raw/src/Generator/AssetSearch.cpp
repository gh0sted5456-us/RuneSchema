#include "Loader/AssetAuthoringMetadata.h"
#include "Generator/AssetSearch.h"
#include "Generator/F2CatalogPlan.h"
#include "Core/ItemCatalogLayout.h"
#include "Core/AlphabeticalItemCatalog.h"
#include "Core/ItemCatalogFilter.h"
#include "Generator/ToolRequest.h"
#include "Generator/HelpyIconTrace.h"
#include "Core/HelpyCacheSources.h"
#include "Core/HelpySourceCache.h"
#include "Core/ItemSource.h"
#include "Core/ItemFamily.h"
#include "Core/BoundedLru.h"
#include "SDK/WeakObjectHandle.h"
#include "Generator/ItemCatalogMetadata.h"
#include "Generator/QuickMenuDecorations.h"
#include "SDK/Helper/CookedAssetLookup.h"
#include "SDK/Helper/RecipeAssetCatalog.h"
#include "SDK/Helper/ItemAppearanceMetadata.h"
#include "Loader/AssetProvenance.h"
#include "Generator/AssetTemplate.h"
#include "Generator/LoaderTemplate.h"
#include "Generator/LoaderSchemas.h"
#include "Core/ConfigFiles.h"
#include "Runtime/HostServices.h"
#include "Utility/Logging.h"
#include <filesystem>
#include "SDK/Helper/StringTableHelper.h"
#include "SDK/Helper/ActorHelper.h"
#include "Unreal/Engine/UDataTable.hpp"
#include "Unreal/Property/FEnumProperty.hpp"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/Helper/ReflectedContract.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Structs/FSoftObjectPtr.h"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/CoreUObject/UObject/UnrealType.hpp"
#include "Unreal/CoreUObject/UObject/FStrProperty.hpp"
#include "Unreal/Property/FTextProperty.hpp"
#include "Unreal/FText.hpp"
#include "Unreal/UObjectArray.hpp"
#include "Unreal/UObjectHashTables.hpp"
#include "Helpers/String.hpp"
#include <cmath>
#include <cctype>
#include <initializer_list>
#include <set>
#include <chrono>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>

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
        if(auto* hardReference=CastField<FObjectPropertyBase>(p)) {
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
std::string CheapText(UObject* object,const char* name) {
    if(!object||!object->GetClassPrivate())return {};
    auto* p=PropertyHelper::GetPropertyByName(object->GetClassPrivate(),to_wstring(name));
    if(!p||p->GetArrayDim()!=1||p->GetOffset_Internal()<0)return {};
    const auto size=object->GetClassPrivate()->GetPropertiesSize();
    if(p->GetElementSize()<=0||p->GetOffset_Internal()>size||p->GetElementSize()>size-p->GetOffset_Internal())return {};
    auto* address=p->ContainerPtrToValuePtr<void>(object);
    if(CastField<FTextProperty>(p))return {};
    if(CastField<FStrProperty>(p)) {
        const auto& chars=static_cast<FString*>(address)->GetCharArray();
        if(chars.Num()<0||chars.Num()>4097||(chars.Num()&&!chars.GetData()))return {};
        return chars.Num()?to_string(StringType(chars.GetData(),chars.Num()-1)):std::string{};
    }
    if(auto* n=CastField<FNameProperty>(p))return to_string(n->GetPropertyValue(address).ToString());
    return {};
}
std::string CachedItemName(UObject* object) {
    struct Entry {WeakObjectHandle Handle;std::string Name;};
    static BoundedLru<std::string,Entry> cache(65536);static uint64_t cacheGeneration=0;
    const auto generation=SpawnToolRequests::Generation.load();
    if(cacheGeneration!=generation){cache.Clear();cacheGeneration=generation;}
    if(!object)return {};
    const auto path=to_string(object->GetPathName());
    if(auto* entry=cache.Find(path);entry&&entry->Handle.Get()==object)return entry->Name;
    auto name=Text(object,"Name");
    if(QuickDecorations::MissingDisplayName(name))name=Text(object,"DisplayName");
    if(QuickDecorations::MissingDisplayName(name))name=QuickDecorations::ReadableAssetName(to_string(object->GetName()));
    Entry entry;entry.Handle.Assign(object);entry.Name=name;
    if(entry.Handle.Get()==object)cache.Put(path,std::move(entry));
    return name;
}
std::string IconReferenceValue(FProperty* p,void* container,int containerSize,int depth=0) {
    if(!p||!container||depth>4||p->GetArrayDim()!=1)return {};
    const auto offset=p->GetOffset_Internal(),size=p->GetElementSize();
    if(offset<0||size<=0||offset>containerSize||size>containerSize-offset)return {};
    auto* address=p->ContainerPtrToValuePtr<void>(container);
    if((CastField<FSoftObjectProperty>(p)||CastField<FSoftClassProperty>(p))&&size==sizeof(UECustom::FSoftObjectPtr)) {
        const auto& path=static_cast<UECustom::FSoftObjectPtr*>(address)->ObjectID;
        const auto package=to_string(path.GetLongPackageFName().ToString()),asset=to_string(path.GetAssetFName().ToString());
        if(package.starts_with('/')&&!asset.empty()&&asset!="None"&&path.SubPathString.GetCharArray().Num()<=1)return package+"."+asset;
    }
    if(auto* objectProperty=CastField<FObjectPropertyBase>(p)) {
        if(auto* linked=objectProperty->GetObjectPropertyValue(address))return to_string(linked->GetPathName());
    }
    if(CastField<FStrProperty>(p)) {
        const auto& chars=static_cast<FString*>(address)->GetCharArray();
        if(chars.Num()>0&&chars.Num()<1025&&chars.GetData()) {
            auto value=to_string(StringType(chars.GetData(),chars.Num()-1));if(value.starts_with('/'))return value;
        }
    }
    if(auto* structure=CastField<FStructProperty>(p);structure&&structure->GetStruct()) {
        static constexpr const char* preferred[]={"ResourceObject","Resource","Texture","Texture2D","Icon","Image","Brush","Asset","Object","Value"};
        auto* type=structure->GetStruct().Get();const auto structSize=size;
        for(const auto* childName:preferred)if(auto* child=PropertyHelper::GetPropertyByName(type,to_wstring(childName))) {
            auto value=IconReferenceValue(child,address,structSize,depth+1);if(!value.empty())return value;
        }
        std::size_t scanned=0;for(auto* child:TFieldRange<FProperty>(type,EFieldIterationFlags::Default)) {
            if(++scanned>48)break;const auto childName=to_string(child->GetName());std::string lower=childName;for(auto& c:lower)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if(lower.find("icon")==std::string::npos&&lower.find("texture")==std::string::npos&&lower.find("image")==std::string::npos&&lower.find("brush")==std::string::npos&&lower.find("resource")==std::string::npos&&lower.find("asset")==std::string::npos)continue;
            auto value=IconReferenceValue(child,address,structSize,depth+1);if(!value.empty())return value;
        }
    }
    return {};
}
std::string PropertyType(FProperty* property) {
    if(!property)return "<missing>";
    try{return PropertyHelper::GetPropertyTypeAsUTF8String(property);}catch(...){return to_string(property->GetClass().GetName());}
}
void TraceItemIcon(UObject* object,const std::string& selectedField,const std::string& selectedType,const std::string& iconPath,const std::string& candidates) {
    if(!object || !PS::HelpyIconTrace::Enabled())return;
    static std::unordered_set<std::string> traced;static std::size_t emitted=0;
    const auto itemPath=to_string(object->GetPathName());if(emitted>=100||!traced.insert(itemPath).second)return;++emitted;
    const auto label=CachedItemName(object);const auto cls=object->GetClassPrivate()?to_string(object->GetClassPrivate()->GetPathName()):std::string("<unknown>");
    PS::HelpyIconTrace::Event("row",itemPath,
        "item='"+label+"' class='"+cls+"' selected='"+selectedField+"' type='"+selectedType+
        "' rowIcon='"+iconPath+"' candidates=["+candidates+"]");
}
std::string ItemIconPath(UObject* object) {
    if(!object||!object->GetClassPrivate())return {};
    static constexpr const char* fields[]={"Icon","ItemIcon","InventoryIcon","DisplayIcon","IconTexture","Thumbnail","CategoryIcon","Image","Brush"};
    const bool trace=PS::HelpyIconTrace::Enabled();
    auto* type=object->GetClassPrivate();const auto size=type->GetPropertiesSize();std::string candidates;std::string selectedField,selectedType,result;
    for(const auto* name:fields)if(auto* p=PropertyHelper::GetPropertyByName(type,to_wstring(name))) {
        const auto propertyType=trace?PropertyType(p):std::string{};const auto path=IconReferenceValue(p,object,size);
        if(!candidates.empty())candidates+="; ";candidates+=std::string(name)+":"+propertyType+(path.empty()?"=<none>":"="+path);
        if(result.empty()&&!path.empty()){result=path;selectedField=name;selectedType=propertyType;}
        // Every resolved icon must publish the native item -> icon dependency,
        // including the normal tracing-off path (clones often cross mounts).
        if(!trace && !result.empty())break;
    }
    if(result.empty()) {
        std::size_t scanned=0;for(auto* p:TFieldRange<FProperty>(type,EFieldIterationFlags::Default)) {
            if(++scanned>128)break;const auto fieldName=to_string(p->GetName());std::string lower=fieldName;for(auto& c:lower)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if(lower.find("icon")==std::string::npos&&lower.find("thumbnail")==std::string::npos&&lower.find("image")==std::string::npos&&lower.find("brush")==std::string::npos)continue;
            const auto propertyType=PropertyType(p);const auto path=IconReferenceValue(p,object,size);
            if(!candidates.empty())candidates+="; ";candidates+=fieldName+":"+propertyType+(path.empty()?"=<none>":"="+path);
            if(!path.empty()){result=path;selectedField=fieldName;selectedType=propertyType;break;}
        }
    }
    TraceItemIcon(object,selectedField.empty()?"<none>":selectedField,selectedType.empty()?"<none>":selectedType,result.empty()?"<none>":result,candidates.empty()?"no icon-like reflected properties":candidates);
    if(!result.empty())PS::HelpyCacheSources::Reference(to_string(object->GetPathName()),result);
    return result;
}
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
    if(objects.Num()<0)throw std::runtime_error("Loaded object roster has an invalid size.");
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

json NativeItemBadges(UObject* object,const std::string& tags) {
    return {{"PowerLevel",Read(object,"PowerLevel")},
        {"Masterwork",QuickDecorations::Token(tags,"masterwork")||ItemBool(object,{"bIsMasterworkItem","bIsMasterwork","IsMasterwork","bMasterwork","Masterwork"})},
        {"MasterworkType",Read(object,"MasterworkType")},
        {"CategoryIcon",ReferencePath(object,"CategoryClassIcon")}};
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
    auto row=json{{"Name",name},{"Path",path},{"InternalName",Text(object,"InternalName")},
        {"PersistenceID",Text(object,"PersistenceID")},{"Icon",ItemIconPath(object)},
        {"RuneSchema",runtimeClone||declared.runeSchema.value_or(AssetMetadata::IsManaged(object))},{"RuntimeClone",runtimeClone},
        {"RuneSchemaManaged",runtimeClone||declared.runeSchema.value_or(AssetMetadata::IsManaged(object))},
        {"DeclaredModded",declared.modded.value_or(false)},{"DeclaredCooked",declared.cooked.value_or(false)},
        {"CloneEligible",cloneEligible&&!AssetMetadata::IsIncomplete(object)},{"CloneReason",cloneReason},{"Modded",AssetMetadata::Encode(declared)},
        {"Available",!AssetMetadata::IsIncomplete(object)&&(!runtimeClone||provenance.value("Registered",false))},
        {"Reason",AssetMetadata::IsIncomplete(object)?"Helpy companion files incomplete; grants and cloning blocked until repaired":runtimeClone&&!provenance.value("Registered",false)?"Runtime clone is not registered with the item subsystem":""},
        {"Consumable",consumable},{"QuestItem",quest},{"Tags",tags},
        {"bSoftDeleted",ItemBool(object,{"bSoftDeleted","IsSoftDeleted","bIsSoftDeleted","IsSoftDelete","bIsSoftDelete"})},
        {"Class",to_string(object->GetClassPrivate()->GetPathName())},
        {"Cooked",CookedAssets::VerifiedLoadedObject(object)},{"AppearanceGroup",ItemAppearanceMetadata::Group(object)}};
    row.update(NativeItemBadges(object,tags));
    auto source=ItemSource::Describe(path,HelpyCacheSources::Snapshot(),HelpySourceCache::NativePluginRoots(),
        runtimeClone?provenance.value("CreatingMod",std::string{}):declared.modded.value_or(false)?AssetMetadata::Owner(object):std::string{},
        runtimeClone||declared.modded.value_or(false));
    if((source.Group==ItemSource::Kind::Vanilla||source.Group==ItemSource::Kind::Expansion)&&!object->HasAnyFlags(RF_WasLoaded))source.Group=ItemSource::Kind::Unknown;
    row.update(ItemSource::Fields(source));return row;
}

namespace {
std::string HelpyBroadItemCategory(UObject* object,const std::string& path,const std::string& name,const std::string& internal,const std::string& tags) {
    std::string classes;std::size_t depth=0;for(UStruct* ancestor=object&&object->GetClassPrivate()?object->GetClassPrivate():nullptr;ancestor&&depth++<24;ancestor=ancestor->GetSuperStruct())classes+=" "+to_string(ancestor->GetName());
    const auto hay=QuickDecorations::Fold(path+" "+name+" "+internal+" "+tags+" "+classes);
    const auto any=[&](std::initializer_list<std::string_view> words){for(const auto word:words)if(hay.find(word)!=std::string::npos)return true;return false;};
    if(any({"quest","questitem"}))return "Quest";
    if(any({"ammo","ammunition","arrow","bolt","bullet","shot"}))return "Ammo";
    if(any({"rune","runes"}))return "Runes";
    if(any({"armor","armour","helmet"," helm","chestplate","cuirass","gauntlet","glove","boots","greaves","shield","wearableequipmentdata"})||QuickDecorations::Token(hay,"cape"))return "Armor";
    if(any({"weapon","sword","dagger","mace","spear","halberd","bow","crossbow","staff","wand","pistol","musket","axe_weapon","battleaxe"}))return "Weapons";
    if(any({"consumable","food","potion","drink","elixir","brew","ration","cooked","meal"}))return "Consumables";
    if(any({"tool","pickaxe","hatchet","fishingrod","fishing_rod","hammer","shovel","sickle"}))return "Tools";
    if(any({"building","buildable","furniture","structure","construction","placeable"}))return "Building";
    if(any({"resource","material","ore","ingot","bar_","_bar","plank","log","timber","hide","leather","fibre","fiber","cloth","stone","crystal","essence","shard","ash","coal"}))return "Resources";
    return "Misc";
}
int HelpyMaxStack(UObject* object) {
    const auto value=Read(object,"MaxStackSize");
    try {if(value.is_number_integer())return std::clamp(value.get<int>(),1,10000);if(value.is_number_unsigned())return std::clamp(static_cast<int>(value.get<unsigned>()),1,10000);if(value.is_number_float())return std::clamp(static_cast<int>(value.get<double>()),1,10000);}catch(...){}
    return 1;
}
std::uint64_t HelpyStableId(const std::string& value) {
    std::uint64_t hash=1469598103934665603ull;
    for(const unsigned char c:value){hash^=c;hash*=1099511628211ull;}return hash;
}
json DescribeItemBasic(UObject* object,const std::string& path,const std::string& name,const std::string& internal,const std::string& tags,const std::string& category) {
    auto persistence=CheapText(object,"PersistenceID");
    auto row=json{{"StableId",HelpyStableId(path)},{"Name",name},{"Path",path},
        {"InternalName",internal},{"PersistenceID",std::move(persistence)},{"Icon",ItemIconPath(object)},
        {"Class",object&&object->GetClassPrivate()?to_string(object->GetClassPrivate()->GetPathName()):std::string{}},
        {"Tags",tags},{"Category",category},{"MaxStackSize",HelpyMaxStack(object)}};
    row.update(NativeItemBadges(object,tags));return row;
}
}

std::vector<std::string> IconReferences(UObject* object) {
    std::set<std::string> paths;if(!Usable(object)||!object->GetClassPrivate())return {};
    auto* type=object->GetClassPrivate();size_t visited=0;
    for(auto* field:TFieldRange<FProperty>(type,EFieldIterationFlags::Default)) {
        if(++visited>256)break;const auto name=F2Catalog::Lower(to_string(field->GetName()));
        if(name.find("icon")==name.npos&&name.find("thumbnail")==name.npos&&name.find("brush")==name.npos&&name!="image")continue;
        const auto path=IconReferenceValue(field,object,type->GetPropertiesSize());
        if(!path.empty()){paths.insert(path);HelpyCacheSources::Reference(to_string(object->GetPathName()),path);}
    }
    return {paths.begin(),paths.end()};
}
json ItemMetadata(const std::string& path) {
    auto* item=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,RC::to_generic_string(path).c_str(),false);
    auto* type=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,TEXT("/Script/Dominion.ItemData"),false);
    if(!Usable(item)||!type||!item->IsA(type))return {{"Path",path},{"Name",QuickDecorations::ReadableAssetName(path)},{"Available",false},{"Reason","Item is not loaded in this session; checked again at delivery."}};
    auto row=DescribeItem(item);row["MaxStackSize"]=HelpyMaxStack(item);
    row["Category"]=HelpyBroadItemCategory(item,path,row.value("Name",std::string{}),row.value("InternalName",std::string{}),row.value("Tags",std::string{}));
    return row;
}
json ItemWindow(const std::string& query,std::size_t offset,std::size_t limit,std::uint64_t startSeed,bool randomOrder,
    const std::string& fieldSet,std::size_t scanBudget,std::uint64_t timeBudgetMicros,bool classIndex) {
    if(limit<1||limit>128)throw std::runtime_error("Catalog window limit must be 1..128 rows.");
    if(query.size()>512)throw std::runtime_error("Catalog query and filter controls exceed 512 characters.");
    if(fieldSet!="BasicRow"&&fieldSet!="Detail")throw std::runtime_error("Unsupported item catalog field projection.");
    scanBudget=std::clamp<std::size_t>(scanBudget,128,8192);timeBudgetMicros=std::clamp<std::uint64_t>(timeBudgetMicros,100,5000);
    json rows=json::array();std::vector<std::string> queryTokens;std::string categoryFilter,searchQuery=query;
    const auto sourceFilter=ItemSource::TakeFilter(searchQuery);
    const auto sourcePlan=HelpyCacheSources::Snapshot();const auto nativePlugins=HelpySourceCache::NativePluginRoots();
    constexpr std::string_view categoryPrefix="__helpycat:";
    if(searchQuery.starts_with(categoryPrefix)){const auto split=searchQuery.find(' ');categoryFilter=AssetTemplate::Fold(searchQuery.substr(categoryPrefix.size(),split==std::string::npos?std::string::npos:split-categoryPrefix.size()));searchQuery=split==std::string::npos?std::string{}:searchQuery.substr(split+1);}
    const auto familyFilter=ItemFamily::TakeFilter(searchQuery);
    if(searchQuery.size()>256)throw std::runtime_error("Catalog search text exceeds 256 characters.");
    if(!searchQuery.empty()){std::istringstream words(searchQuery);std::string word;while(words>>word){word=AssetTemplate::Fold(std::move(word));if(!word.empty())queryTokens.push_back(std::move(word));}}
    const auto matchesTokens=[&](const std::string& candidate){if(queryTokens.empty())return true;const auto folded=AssetTemplate::Fold(candidate);for(const auto& token:queryTokens)if(folded.find(token)==std::string::npos)return false;return true;};
    auto* itemType=Class("assets");if(!itemType)throw std::runtime_error("ItemData class is not ready.");
    const auto objectCount=FUObjectArray::GetNumElements();
    if(objectCount<0||objectCount>4'000'000)throw std::runtime_error("Global object roster exceeds the safe catalog bound.");
    static uint64_t mountedGeneration=0;static bool mountedReady=false;static std::vector<CookedAssets::Record> mountedItems;
    const auto generation=SpawnToolRequests::Generation.load();
    if(!mountedReady||mountedGeneration!=generation) {
        if(mountedGeneration!=generation){mountedReady=false;mountedItems.clear();}
        bool classMetadataReady=false;
        try {
            // Ask the native registry for ItemData (including subclasses),
            // rather than copying/filtering every art/texture/world record.
            // This is metadata only: do not bulk-load the returned items.
            auto paths=RecipeAssetCatalog::MountedPaths(TEXT("ItemData"));
            if(paths.empty())throw std::runtime_error("Native ItemData metadata is not ready");
            std::vector<CookedAssets::Record> discovered;discovered.reserve(paths.size());
            for(auto& path:paths){const auto split=path.rfind('.');const auto name=path.substr(split==path.npos?0:split+1);
                discovered.push_back({std::move(path),name,"/Script/Dominion.ItemData"});}
            mountedItems=std::move(discovered);mountedGeneration=generation;mountedReady=classMetadataReady=true;
        }catch(const std::exception& error){PS::Log<RC::LogLevel::Verbose>(STR("Item class metadata unavailable; using the bounded catalogue fallback: {}\n"),PS::ToWideSafe(error.what()));}
        if(!classMetadataReady&&CookedAssets::TryEnsure()) {
            std::vector<CookedAssets::Record> discovered;
            for(const auto& record:CookedAssets::MetadataSnapshot()) {
                const auto hint=F2Catalog::MountedCandidate(record.path,record.name,record.type);
                if(hint&&hint->kind==F2Catalog::Kind::Item)discovered.push_back(record);
            }
            mountedItems=std::move(discovered);mountedGeneration=generation;mountedReady=true;
        }
    }
    const auto priorityPaths=AssetProvenance::LivePaths();
    const std::unordered_set<std::string> prioritySet(priorityPaths.begin(),priorityPaths.end());
    static std::string indexedScope;static std::vector<WeakObjectHandle> indexedItems;
    const bool useClassIndex=classIndex&&FUObjectHashTables::IsAvailable();
    if(useClassIndex){
        const auto scope=json::array({generation,HelpySourceCache::PublishedFingerprint(),priorityPaths}).dump();
        if(scope!=indexedScope){
            TArray<UObject*> items;UECustom::UObjectGlobals::GetObjectsOfClass(itemType,items,true);
            if(items.Num()<0||items.Num()>AlphabeticalItemCatalog::Cache::MaxRows)throw std::runtime_error("Native item class index exceeds its safe bound");
            indexedItems.clear();indexedItems.reserve(items.Num());
            for(auto* item:items)if(Usable(item))indexedItems.emplace_back(item);
            indexedScope=scope;
        }
    }
    // Use the public class hash only when available. Hosts without it keep
    // the original time-bounded object-array walk, not a synchronous fallback.
    const ItemCatalogLayout layout{{priorityPaths.size(),useClassIndex?indexedItems.size():static_cast<std::size_t>(objectCount),mountedItems.size()}};
    const auto total=layout.Total();
    if(!total)return {{"Rows",std::move(rows)},{"NextOffset",0},{"Start",0},{"HasMore",false},{"Scanned",0}};
    const std::size_t start=randomOrder?static_cast<std::size_t>(startSeed%total):0;
    std::size_t visited=std::min(offset,total),scanned=0;const auto begin=std::chrono::steady_clock::now();
    while(visited<total&&rows.size()<limit&&scanned<scanBudget) {
        if(scanned>0&&std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-begin).count()>=static_cast<long long>(timeBudgetMicros))break;
        const std::size_t index=randomOrder?(start+visited)%total:visited;
        const auto [segment,localIndex]=layout.At(index);
        if(segment==2) {
            ++visited;++scanned;const auto& record=mountedItems[localIndex];
            auto* loaded=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,RC::to_generic_string(record.path).c_str(),false);
            if(Usable(loaded))continue; // already covered by the native UObject slice
            const auto name=QuickDecorations::ReadableAssetName(record.path),category=HelpyBroadItemCategory(nullptr,record.path,name,{},record.type);
            if(!categoryFilter.empty()&&AssetTemplate::Fold(category)!=categoryFilter)continue;
            const auto source=ItemSource::Describe(record.path,sourcePlan,nativePlugins);
            if(!ItemSource::Matches(sourceFilter,source)||!ItemFamily::Matches(familyFilter,name))continue;
            if(!matchesTokens(record.path+" "+name+" "+record.type+" "+category))continue;
            auto entry=json{{"StableId",HelpyStableId(record.path)},{"Path",record.path},{"Name",name},{"Class",record.type},{"Category",category},
                {"Cooked",true},{"Mounted",true},{"Loaded",false},{"Available",true},{"Reason","Mounted ItemData candidate; native fields and inventory compatibility are checked when loaded"}};
            entry.update(ItemSource::Fields(source));rows.push_back(std::move(entry));
            continue;
        }
        UObject* object=nullptr;
        if(segment==0)object=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,RC::to_generic_string(priorityPaths[localIndex]).c_str(),false);
        else if(useClassIndex)object=indexedItems[localIndex].Get();
        else{auto* slot=FUObjectArray::IndexToObject(static_cast<int32>(localIndex));object=slot?slot->GetUObject():nullptr;}
        ++visited;++scanned;
        if(!Usable(object)||!object->IsA(itemType))continue;
        const auto path=to_string(object->GetPathName());if(path.empty())continue;
        if(segment==1&&prioritySet.contains(path))continue;
        const auto internal=CheapText(object,"InternalName");const auto name=CachedItemName(object);const auto tags=ItemTags(object);const auto broadCategory=HelpyBroadItemCategory(object,path,name,internal,tags);
        if(!categoryFilter.empty()&&AssetTemplate::Fold(broadCategory)!=categoryFilter)continue;
        const auto provenance=AssetProvenance::Lookup(object);const auto declared=AssetMetadata::Lookup(object);
        auto source=ItemSource::Describe(path,sourcePlan,nativePlugins,
            provenance.is_object()&&provenance.value("Confirmed",false)?provenance.value("CreatingMod",std::string{}):declared.modded.value_or(false)?AssetMetadata::Owner(object):std::string{},
            declared.modded.value_or(false)||(provenance.is_object()&&provenance.value("Confirmed",false)));
        if((source.Group==ItemSource::Kind::Vanilla||source.Group==ItemSource::Kind::Expansion)&&!object->HasAnyFlags(RF_WasLoaded))source.Group=ItemSource::Kind::Unknown;
        if(!ItemSource::Matches(sourceFilter,source)||!ItemFamily::Matches(familyFilter,name))continue;
        if(!queryTokens.empty()&&!matchesTokens(path+" "+internal+" "+name+" "+tags+" "+broadCategory+" "+to_string(object->GetClassPrivate()->GetName())))continue;
        auto entry=fieldSet=="Detail"||segment==0?DescribeItem(object):DescribeItemBasic(object,path,name,internal,tags,broadCategory);
        if(entry.is_object()){entry["StableId"]=HelpyStableId(path);entry["Category"]=broadCategory;entry.update(ItemSource::Fields(source));rows.push_back(std::move(entry));}
    }
    return {{"Rows",std::move(rows)},{"NextOffset",visited},{"Start",start},{"HasMore",visited<total},
        {"Scanned",scanned},{"BudgetExhausted",visited<total&&scanned>=scanBudget},{"FieldSet",fieldSet}};
}

json AlphabeticalItemWindow(const std::string& query,const std::string& cursor,std::size_t limit,const std::string& fieldSet){
    if(fieldSet!="BasicRow"&&fieldSet!="Detail")throw std::runtime_error("Unsupported item catalog field projection.");
    // Two consumers must not evict each other's in-progress source snapshot.
    static AlphabeticalItemCatalog::Cache basicCache,detailCache;
    auto& cache=fieldSet=="Detail"?detailCache:basicCache;
    // Authored runtime items change independently of mounted content. Their
    // exact live path list invalidates this index without chasing transient
    // UObject counts (which change constantly while a menu is open).
    const ItemCatalogFilter::Query filter(query);
    const auto scope=json::array({SpawnToolRequests::Generation.load(),HelpySourceCache::PublishedFingerprint(),AssetProvenance::LivePaths(),fieldSet}).dump();
    cache.Scope(scope);return cache.ReadRecovering(json::array({query,fieldSet}).dump(),cursor,limit,[&](size_t offset){
        return ItemWindow({},offset,128,0,false,fieldSet,4096,1800,true);
    },filter);
}
json ItemRoster() {
    json rows=json::array();
    auto* itemType=Class("assets");if(!itemType)throw std::runtime_error("ItemData class is not ready.");
    TArray<UObject*> objects;UECustom::UObjectGlobals::GetObjectsOfClass(itemType,objects,true);
    if(objects.Num()<0)throw std::runtime_error("Loaded item roster has an invalid size.");
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
            if(CastField<FTextProperty>(field) && !ReflectedContract::Text(field))continue;
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
