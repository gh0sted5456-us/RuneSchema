#pragma once
#include "Core/SaveCleanup.h"
#include "Core/CacheBundleIO.h"
#include "Core/ContentSource.h"
#include "Core/RuneSchemaDataRoot.h"
#include <span>

namespace PS::SaveRecovery {
using Json=nlohmann::json;
inline constexpr size_t MaximumBytes=64*1024*1024,MaximumRecords=65536;
inline std::string Character(const Json& source){
    SaveCleanup::RequireCharacter(source);const auto guid=source.at("meta_data").at("char_guid").get<std::string>();
    if(guid.empty()||guid.size()>128||std::any_of(guid.begin(),guid.end(),[](unsigned char c){return c<32||c==127;}))throw std::runtime_error("Recovery requires a stable character GUID");return HelpyCacheSources::Lower(guid);
}
inline bool BindingValid(const Json& binding){return binding.is_object()&&binding.size()==3&&binding.contains("Asset")&&binding["Asset"].is_string()
    &&binding.contains("Class")&&binding["Class"].is_string()&&binding.contains("Owner")&&binding["Owner"].is_string()
    &&!HelpyCacheSources::AssetKey(binding["Asset"].get<std::string>()).empty()&&binding["Asset"].get_ref<const std::string&>().size()<=2048
    &&binding["Class"].get_ref<const std::string&>().starts_with('/')&&binding["Class"].get_ref<const std::string&>().size()<=2048
    &&binding["Owner"].get_ref<const std::string&>().starts_with("mods/")&&ContentSource::Valid(binding["Owner"].get<std::string>());}
inline Json Binding(const SaveCleanup::RegistrySnapshot::Binding& value){return {{"Asset",value.Asset},{"Class",value.Class},{"Owner",value.Owner}};}
inline Json New(const std::string& runtime,const std::string& guid){return {{"Version",1},{"Format","RuneSchemaRecovery"},{"Runtime",runtime},{"CharacterGuid",guid},{"Bindings",Json::object()},{"Records",Json::object()}};}
inline void Validate(const Json& archive,const std::string& runtime,const std::string& guid){
    if(!archive.is_object()||archive.value("Version",0)!=1||archive.value("Format",std::string{})!="RuneSchemaRecovery"
        ||archive.at("Runtime")!=runtime||archive.at("CharacterGuid")!=guid||!archive.at("Bindings").is_object()||archive.at("Bindings").size()>MaximumRecords
        ||!archive.at("Records").is_object()||archive.at("Records").size()>MaximumRecords||archive.dump().size()>MaximumBytes)throw std::runtime_error("Recovery archive identity/layout is invalid");
    for(const auto& [key,binding]:archive.at("Bindings").items())if(key.empty()||key.size()>1152||!BindingValid(binding))throw std::runtime_error("Recovery binding is invalid");
    for(const auto& [key,row]:archive.at("Records").items())if(!CacheBundleIO::Digest(key)||!row.is_object()||row.dump().size()>262144
        ||!row.at("Kind").is_string()||!row.at("Id").is_string()||!row.at("Slot").is_string()||!row.at("Identity").is_string()||!row.at("State").is_string()
        ||row.at("Id").get_ref<const std::string&>().size()>1024||row.at("Slot").get_ref<const std::string&>().size()>128||!row.contains("Value")
        ||(!row.at("Binding").is_null()&&!BindingValid(row.at("Binding"))))throw std::runtime_error("Recovery record is invalid");
}
inline const Json* Section(const Json& save,const std::string& kind){
    const auto& game=save.at("GameProgress");
    if(kind=="Inventory"||kind=="PersonalInventory"||kind=="Loadout"){const auto found=game.find(kind);return found!=game.end()&&found->is_object()?&*found:nullptr;}
    const char* parent=(kind=="ItemsPickedUp"||kind=="MilestoneMaterialsPickedUp"||kind=="RecipesUnlocked"||kind=="RecipesNew")?"Progress":
        (kind=="Quests"||kind=="QuestTracked"||kind=="QuestLocations")?"QuestProgress":(kind=="UnlockedEntries"||kind=="UnreadEntries")?"Journal":nullptr;
    if(!parent||!game.contains(parent)||!game[parent].is_object())return nullptr;const auto found=game[parent].find(kind);return found==game[parent].end()?nullptr:&*found;
}
inline Json& Destination(Json& save,const std::string& kind){
    auto& game=save.at("GameProgress");if(kind=="Inventory"||kind=="PersonalInventory"||kind=="Loadout")return game.at(kind);
    const char* parent=(kind=="ItemsPickedUp"||kind=="MilestoneMaterialsPickedUp"||kind=="RecipesUnlocked"||kind=="RecipesNew")?"Progress":
        (kind=="Quests"||kind=="QuestTracked"||kind=="QuestLocations")?"QuestProgress":(kind=="UnlockedEntries"||kind=="UnreadEntries")?"Journal":nullptr;
    if(!parent)throw std::runtime_error("Unsupported recovery section");return game.at(parent).at(kind);
}
inline std::string Id(const Json& value,const std::string& kind){
    if(value.is_string())return value.get<std::string>();if(!value.is_object())return {};
    const auto* key=kind=="Quests"?"QuestId":kind=="QuestLocations"?"QuestLocationId":"ItemData";const auto found=value.find(key);return found!=value.end()&&found->is_string()?found->get<std::string>():std::string{};
}
inline bool Present(const Json* section,const std::string& kind,const std::string& id){
    if(!section)return false;if(kind=="QuestTracked")return section->is_string()&&section->get<std::string>()==id;
    if(!section->is_array())return false;for(const auto& row:*section)if(Id(row,kind)==id)return true;return false;
}
inline std::string Identity(const std::string& kind,const std::string& id){
    const auto* type=kind=="RecipesUnlocked"||kind=="RecipesNew"?"Recipe":kind=="Quests"||kind=="QuestTracked"?"Quest":kind=="UnlockedEntries"||kind=="UnreadEntries"?"Journal":"Item";
    return id.empty()?std::string{}:std::string(type)+":"+id;
}
struct Plan {Json Save,Archive;size_t Captured=0,Restored=0,Blocked=0;};
inline Plan Prepare(const Json& source,const Json& cleaned,Json archive,const SaveCleanup::RegistrySnapshot& registry,const std::string& runtime){
    const auto guid=Character(source);if(Character(cleaned)!=guid)throw std::runtime_error("Recovery character identity changed");Validate(archive,runtime,guid);
    if(!registry.Ready())throw std::runtime_error("Recovery requires complete stable native registries");Plan result{cleaned,std::move(archive)};auto& history=result.Archive["Bindings"];auto& records=result.Archive["Records"];
    for(const auto& [key,value]:registry.Bindings){auto binding=Binding(value);if(BindingValid(binding))history[key]=std::move(binding);}
    const auto capture=[&](const std::string& kind,const std::string& slot,const Json& value,std::string id,std::string identity={}){
        if(identity.empty())identity=Identity(kind,id);const auto key=CacheBundleIO::Hash(kind+"|"+slot+"|"+id+"|"+value.dump());
        if(!records.contains(key))++result.Captured;
        records[key]={{"Kind",kind},{"Id",id},{"Slot",slot},{"Value",value},{"Identity",identity},{"Binding",history.value(identity,Json{})},{"State","Pending"}};
    };
    for(const auto* section:{"Inventory","PersonalInventory","Loadout"}){const auto* before=Section(source,section),*after=Section(cleaned,section);if(!before||!after||!after->is_object())continue;
        for(const auto& [slot,value]:before->items())if(!after->contains(slot)){
            auto id=Id(value,section);if(std::string_view(section)=="Loadout"&&id.empty()&&value.is_object()&&value.value("PlayerInventoryItemIndex",Json{}).is_number_integer()){
                const auto* inventory=Section(source,"Inventory");const auto index=std::to_string(value["PlayerInventoryItemIndex"].get<int>());if(inventory&&inventory->contains(index))id=Id(inventory->at(index),"Inventory");}
            capture(section,slot,value,id);}}
    for(const auto* kind:{"ItemsPickedUp","MilestoneMaterialsPickedUp","RecipesUnlocked","RecipesNew","Quests","QuestTracked","QuestLocations","UnlockedEntries","UnreadEntries"}){
        const auto* before=Section(source,kind),*after=Section(cleaned,kind);if(!before||!after)continue;
        if(std::string_view(kind)=="QuestTracked"){if(before->is_string()&&!before->get_ref<const std::string&>().empty()&&after->is_string()&&after->get_ref<const std::string&>().empty())capture(kind,{},*before,before->get<std::string>());continue;}
        if(!before->is_array()||!after->is_array())continue;for(const auto& value:*before){const auto id=Id(value,kind);if(id.empty()||Present(after,kind,id))continue;
            std::string identity;if(std::string_view(kind)=="QuestLocations"){
                const auto* quests=Section(source,"Quests");if(quests&&quests->is_array())for(const auto& quest:*quests)if(quest.is_object()&&quest.value("QuestInts",Json{}).is_array())for(const auto& variable:quest["QuestInts"])
                    if(variable.is_object()&&variable.value("QuestVariableName",Json{})=="RuneSchema.Location:"+id&&variable.value("QuestVariableValue",Json{})==DragonWilds::Quests::OwnershipVersion){
                        const auto candidate=Identity("Quests",Id(quest,"Quests"));if(!identity.empty()&&identity!=candidate)throw std::runtime_error("Ambiguous recovery quest location");identity=candidate;}
                if(identity.empty())identity="UnverifiedLocation:"+id;
            }
            capture(kind,{},value,id,identity);
        }
    }
    if(source.value("Customization",Json{}).is_object()&&cleaned.value("Customization",Json{}).is_object()){
        const auto& before=source["Customization"];const auto& after=cleaned["Customization"];
        if(before.value("CustomizationData",Json{}).is_object()&&after.value("CustomizationData",Json{}).is_object())
            for(const auto& [field,value]:before["CustomizationData"].items())if(!after["CustomizationData"].contains(field)||after["CustomizationData"][field]!=value)
                capture("Appearance",field,value,{},"Appearance:"+field); // Archived only until a table/row restoration adapter is verified.
    }
    std::set<std::string> restoredInventorySlots;
    for(int pass=0;pass<2;++pass)for(auto& [key,row]:records.items()){
        const auto recordKind=row["Kind"].get<std::string>();
        const bool dependent=recordKind=="Loadout"||recordKind=="QuestTracked"||recordKind=="QuestLocations";
        if(dependent!=(pass==1))continue;
        if(row["State"]!="Pending"||row["Binding"].is_null())continue;const auto identity=row["Identity"].get<std::string>();const auto binding=registry.Bindings.find(identity);
        if(binding==registry.Bindings.end()||Binding(binding->second)!=row["Binding"])continue;
        const auto colon=identity.find(':');if(colon==identity.npos)continue;const auto type=identity.substr(0,colon),nativeId=identity.substr(colon+1);
        const bool known=type=="Item"?registry.Items.contains(nativeId):type=="Recipe"?registry.Recipes.contains(nativeId):type=="Quest"?registry.QuestsComplete&&registry.Quests.contains(nativeId):type=="Journal"&&registry.JournalsComplete&&registry.Journals.contains(nativeId);
        if(!known)continue;const auto kind=row["Kind"].get<std::string>(),id=row["Id"].get<std::string>(),slot=row["Slot"].get<std::string>();const auto* live=Section(source,kind);if(!live)continue;
        if(kind!="QuestLocations"&&identity!=Identity(kind,id))throw std::runtime_error("Recovery kind/native identity mismatch");
        auto& target=Destination(result.Save,kind);const auto& value=row["Value"];
        if(kind=="Inventory"||kind=="PersonalInventory"||kind=="Loadout"){
            if(!live->is_object()||!target.is_object()||!value.is_object()||slot.empty())continue;
            if(target.contains(slot)){row["State"]=target.at(slot)==value?"ObservedNative":"BlockedOccupiedSlot";++result.Blocked;continue;}
            bool elsewhere=false;if(kind!="Loadout")for(const auto* section:{"Inventory","PersonalInventory"})if(const auto* entries=Section(source,section))for(const auto& entry:*entries)if(Id(entry,section)==id)elsewhere=true;
            if(elsewhere){row["State"]="BlockedExistingItem";++result.Blocked;continue;}
            if(kind=="Loadout"){
                if(!value.value("PlayerInventoryItemIndex",Json{}).is_number_integer())continue;const auto* inventory=Section(result.Save,"Inventory");const auto index=std::to_string(value["PlayerInventoryItemIndex"].get<int>());
                if(!restoredInventorySlots.contains(index)||!inventory||!inventory->contains(index)||Id(inventory->at(index),"Inventory")!=id)continue;
            }else if(Id(value,kind)!=id)throw std::runtime_error("Recovery item identity mismatch");
            target[slot]=value;
            if(kind=="Inventory")restoredInventorySlots.insert(slot);
        }else if(kind=="QuestTracked"){
            if(!live->is_string()||!live->get_ref<const std::string&>().empty()||!Present(Section(result.Save,"Quests"),"Quests",id)){row["State"]="BlockedNativeProgress";++result.Blocked;continue;}target=value;
        }else{
            if(!target.is_array()||Id(value,kind)!=id)continue;if(Present(live,kind,id)){row["State"]="ObservedNative";continue;}
            if(kind=="Quests"){if(!value.is_object()||!value.value("QuestInts",Json{}).is_array())continue;try{SaveCleanup::CheckPending(value);}catch(...){continue;}}
            if(kind=="QuestLocations"&&!Present(Section(result.Save,"Quests"),"Quests",nativeId))continue;
            if(!Present(&target,kind,id))target.push_back(value);
        }
        row["State"]="ClaimedForNativeLoad";++result.Restored;
    }
    Validate(result.Archive,runtime,guid);return result;
}
struct Lock {
    HANDLE Handle=INVALID_HANDLE_VALUE;
    explicit Lock(const std::filesystem::path& path){CacheBundleIO::Safe(path);std::filesystem::create_directories(path.parent_path());Handle=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(Handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Recovery archive is in use; save unchanged");}
    ~Lock(){if(Handle!=INVALID_HANDLE_VALUE)CloseHandle(Handle);}Lock(const Lock&)=delete;Lock& operator=(const Lock&)=delete;
};
inline std::filesystem::path Directory(const std::string& runtime){const auto key=RuneSchemaData::RuntimeKey(runtime);if(key!="steam"&&key!="gamepass"&&key!="epic"&&key!="server")throw std::runtime_error("Recovery requires an identified runtime");return RuneSchemaData::Directory()/"core"/"recovery"/key;}
inline Plan Update(const std::filesystem::path& directory,const std::string& runtime,const Json& source,const Json& cleaned,const SaveCleanup::RegistrySnapshot& registry){
    const auto guid=Character(source);const auto file=directory/"characters"/(CacheBundleIO::Hash(guid)+".runeschema-save");Lock lock(file.wstring()+L".lock");
    const auto before=std::filesystem::exists(file)?CacheBundleIO::Read(file,MaximumBytes):std::string{};
    auto archive=before.empty()?New(runtime,guid):Json::parse(before,[](int depth,Json::parse_event_t,Json&){if(depth>64)throw std::runtime_error("Recovery archive nesting exceeds its limit; native save unchanged");return true;});
    auto result=Prepare(source,cleaned,std::move(archive),registry,runtime);
    if(source["meta_data"].value("char_name",Json{}).is_string())result.Archive["CharacterName"]=source["meta_data"]["char_name"].get<std::string>().substr(0,256);
    const auto bytes=result.Archive.dump();
    if(before!=bytes){if(!before.empty())CacheBundleIO::Publish(file.wstring()+L".previous",before);CacheBundleIO::Publish(file,bytes);
        if(CacheBundleIO::Read(file,MaximumBytes)!=bytes)throw std::runtime_error("Recovery archive writeback failed; native save unchanged");}
    return result;
}
inline void ArchiveWorld(const std::filesystem::path& directory,const std::string& runtime,const std::string& slot,std::span<const uint8_t> bytes,const std::vector<std::string>& ids){
    if(bytes.empty()||bytes.size()>MaximumBytes||slot.empty()||slot.size()>1024||ids.size()>MaximumRecords)throw std::runtime_error("World recovery exceeds its archive bounds; world unchanged");
    const std::string_view original(reinterpret_cast<const char*>(bytes.data()),bytes.size());const auto digest=CacheBundleIO::Hash(original);
    const auto folder=directory/"worlds"/CacheBundleIO::Hash(slot);Lock lock(folder/"archive.lock");const auto file=folder/(digest+".world");
    if(std::filesystem::exists(file)){if(CacheBundleIO::Hash(CacheBundleIO::Read(file,MaximumBytes))!=digest)throw std::runtime_error("World recovery archive is invalid; world unchanged");}
    else {CacheBundleIO::Publish(file,std::string(original));if(CacheBundleIO::Hash(CacheBundleIO::Read(file,MaximumBytes))!=digest)throw std::runtime_error("World recovery archive did not verify; world unchanged");}
    const Json manifest={{"Version",1},{"Format","RuneSchemaWorldRecovery"},{"Runtime",runtime},{"Slot",slot},{"OriginalSHA256",digest},{"Bytes",bytes.size()},{"RemovedPersistenceIds",ids},{"AutomaticRestore",false}};
    CacheBundleIO::Publish(folder/(digest+".json"),manifest.dump());
}
}
