#pragma once
#include "Core/HelpyPersonalization.h"
#include "Core/HelpyScrap.h"
#include <chrono>
#include <functional>
#include <map>

namespace PS::HelpyStorage {
using Json=nlohmann::json;
inline constexpr size_t MaximumBytes=4*1024*1024,MaximumItems=1024,MaximumItemBytes=64*1024;
enum class DeleteResult { Removed, Retained, Unknown };
enum class RestoreResult { Confirmed, NotAttempted, Unknown };
inline std::mutex Gate;
inline int64_t Now(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
inline bool Token(const std::string& value){return !value.empty()&&value.size()<=96&&value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_")==value.npos;}
inline void Item(const Json& value){
    if(!value.is_object()||value.size()>32||value.dump().size()>MaximumItemBytes)throw std::runtime_error("Stored item exceeds its schema bounds");
    const auto path=value.value("AssetPath",std::string{});const auto identity=value.value("PersistenceID",std::string{});
    if(path.size()>4096||!path.starts_with('/')||path.starts_with("//")||path.find("..")!=path.npos||!HelpyPersonalization::Text(path,4096)
        ||identity.empty()||!HelpyPersonalization::Text(identity,256)||HelpyPersonalization::Guid(value.value("InstanceGuid",std::string{})).empty())throw std::runtime_error("Stored item identity unavailable");
    const auto count=value.value("Count",int64_t{});if(count<1||count>1000000)throw std::runtime_error("Stored item count is invalid");
    if(!value.contains("NativeItem")||!value["NativeItem"].is_object()||value["NativeItem"].size()>64)throw std::runtime_error("Native item recovery payload is unavailable");
    const auto& native=value["NativeItem"];
    if(native.value("ItemData",std::string{})!=identity||native.value("Count",int64_t{})!=count
        ||HelpyPersonalization::Guid(native.value("GUID",std::string{}))!=HelpyPersonalization::Guid(value.at("InstanceGuid").get<std::string>()))throw std::runtime_error("Item recovery envelope disagrees with native item identity");
    // A recovery record contains one item, never a complete character/world.
    for(const char* field:{"GameProgress","Customization","World","Appearance"})if(value["NativeItem"].contains(field))throw std::runtime_error("Character or world data is not an item payload");
}
inline void ScrapPlan(const Json& plan,const Json& item,const std::string& character){
    if(!plan.is_object()||plan.dump().size()>256*1024||HelpyPersonalization::Guid(plan.value("PlayerGuid",std::string{}))!=character
        ||plan.value("Path",std::string{})!=item.value("AssetPath",std::string{})||!HelpyScrap::Path(plan.value("Path",std::string{}))
        ||plan.value("Quantity",int64_t{})!=item.value("Count",int64_t{})||plan.value("Delivery",std::string{})!="WorldDrops"
        ||!plan.contains("Refunds")||!plan["Refunds"].is_array())throw std::runtime_error("Scrap receipt does not match the selected stack");
    const auto key=HelpyPersonalization::StackKey(plan);
    if(key.empty()||key.substr(39,32)!=HelpyPersonalization::Guid(item.value("InstanceGuid",std::string{})))throw std::runtime_error("Scrap instance identity changed");
    std::vector<HelpyScrap::Amount> amounts;for(const auto& row:plan["Refunds"]){if(!row.at("Count").is_number_integer())throw std::runtime_error("Invalid refund count");amounts.push_back({row.at("Path"),row.at("Count")});}
    HelpyScrap::Amounts(amounts);
}
inline void ScrapDrops(const Json& plan,const Json& drops,bool confirmed){
    if(!drops.is_array()||drops.size()>128||drops.dump().size()>256*1024)throw std::runtime_error("Invalid scrap drop receipt");
    std::map<std::string,int64_t> expected,actual;std::set<std::string> actors;
    for(const auto& row:plan.at("Refunds"))expected[HelpyPersonalization::Fold(row.at("Path"))]=row.at("Count");
    for(const auto& row:drops){const auto path=HelpyPersonalization::Fold(row.at("Path")),actor=row.at("Actor").get<std::string>();
        if(!row.at("Count").is_number_integer()||!actor.starts_with('/')||actor.starts_with("//")||actor.find("..")!=actor.npos||actor.find('\\')!=actor.npos
            ||!HelpyPersonalization::Text(actor,4096)||!actors.insert(actor).second||!expected.contains(path))throw std::runtime_error("Invalid native drop identity");
        const int64_t count=row.at("Count");if(count<1||count>expected.at(path)||actual[path]>expected.at(path)-count)throw std::runtime_error("Native refund exceeds its plan");actual[path]+=count;
    }
    if(confirmed&&actual!=expected)throw std::runtime_error("Native drops do not match the complete refund");
}
inline Json Validate(Json document,const std::string& character){
    if(!document.is_object()||document.size()!=5||document.value("Format",std::string{})!="HelpyStorage"||document.value("Version",0)!=1
        ||document.value("CharacterGuid",std::string{})!=character||!document.contains("Items")||!document["Items"].is_object()
        ||document["Items"].size()>MaximumItems||!document.contains("Revision")||!document["Revision"].is_number_unsigned()||document.dump().size()>MaximumBytes)throw std::runtime_error("Invalid or foreign Helpy Storage file");
    for(const auto& [id,entry]:document["Items"].items()){
        if(!Token(id)||!entry.is_object()||entry.size()>8)throw std::runtime_error("Invalid storage entry");Item(entry.at("Item"));
        const auto state=entry.value("State",std::string{});if(state!="pending-delete"&&state!="stored"&&state!="not-deleted"&&state!="restore-pending"&&state!="restored"&&state!="unconfirmed"
            &&state!="pending-scrap"&&state!="scrap-deleted"&&state!="scrapped"&&state!="scrap-retained"&&state!="scrap-unconfirmed")throw std::runtime_error("Invalid recovery state");
        if(state.find("scrap")!=state.npos){ScrapPlan(entry.at("ScrapPlan"),entry.at("Item"),character);if(entry.contains("Drops"))ScrapDrops(entry.at("ScrapPlan"),entry.at("Drops"),state=="scrapped");}
        if(entry.contains("RestoreAttempt")&&!Token(entry.at("RestoreAttempt").get<std::string>()))throw std::runtime_error("Invalid restore attempt");
    }return document;
}
inline Json Present(const Json& document){
    Json rows=Json::array();for(const auto& [id,entry]:document.at("Items").items()){
        const auto state=entry.at("State").get<std::string>();if(state=="restored"||state=="not-deleted"||state.find("scrap")!=state.npos)continue;
        auto row=entry.at("Item");row["Path"]=row.at("AssetPath");row["StorageId"]=id;row["StorageState"]=state;row["Restorable"]=state=="stored";rows.push_back(std::move(row));
    }return {{"CharacterGuid",document.at("CharacterGuid")},{"Revision",document.at("Revision")},{"Items",rows}};
}
inline Json ReadFile(const std::filesystem::path& file,const std::string& guid){
    const auto character=HelpyPersonalization::Guid(guid);if(character.empty())throw std::runtime_error("Select a character with a stable GUID");
    HelpyPersonalization::Safe(file);return Present(Validate(Json::parse(ConfigFiles::Read(file,MaximumBytes)),character));
}
class Store {
    std::filesystem::path file;std::string character;
    Json ReadUnlocked()const{
        HelpyPersonalization::Safe(file);if(!std::filesystem::exists(file))return {{"Format","HelpyStorage"},{"Version",1},{"CharacterGuid",character},{"Revision",uint64_t(0)},{"Items",Json::object()}};
        return Validate(Json::parse(ConfigFiles::Read(file,MaximumBytes)),character);
    }
    Json Update(const std::function<void(Json&)>& change){
        // Native Delete must never wait on a second UI/game process to save a copy.
        std::unique_lock lock(Gate,std::try_to_lock);if(!lock.owns_lock())throw std::runtime_error("Helpy Storage is busy; no recovery copy was committed");
        uint64_t hash=1469598103934665603ull;for(unsigned char c:HelpyPersonalization::Fold(std::filesystem::absolute(file).generic_string())){hash^=c;hash*=1099511628211ull;}
        const auto name=L"Local\\RuneSchema-HelpyStorage-"+std::to_wstring(hash);HANDLE handle=CreateMutexW(nullptr,FALSE,name.c_str());if(!handle)throw std::runtime_error("Storage lock unavailable");
        const auto acquired=WaitForSingleObject(handle,0);if(acquired!=WAIT_OBJECT_0&&acquired!=WAIT_ABANDONED){CloseHandle(handle);throw std::runtime_error("Storage busy in another process");}
        struct Unlock{HANDLE Value;~Unlock(){ReleaseMutex(Value);CloseHandle(Value);}}unlock{handle};
        auto document=ReadUnlocked();change(document);auto& revision=document["Revision"];if(revision.get<uint64_t>()==UINT64_MAX)throw std::runtime_error("Storage revision exhausted");revision=revision.get<uint64_t>()+1;
        document=Validate(std::move(document),character);ConfigFiles::Write(file,document.dump(2));return document;
    }
public:
    Store(const std::filesystem::path& directory,const std::string& guid):character(HelpyPersonalization::Guid(guid)){
        if(character.empty())throw std::runtime_error("A stable character GUID is required for Helpy Storage");file=directory/(character+".json");HelpyPersonalization::Safe(file);
    }
    const std::filesystem::path& File()const{return file;}
    Json Read()const{std::lock_guard lock(Gate);return ReadUnlocked();}
    Json Prepare(const std::string& id,const Json& item,const Json& plan=nullptr){
        if(!Token(id))throw std::runtime_error("Invalid delete capture identity");Item(item);
        return Update([&](Json& document){auto& entries=document["Items"];if(entries.contains(id)){if(plan.is_null()&&entries[id]["Item"]==item&&entries[id]["State"]=="pending-delete")return;throw std::runtime_error("Capture identity already used; no automatic retry");}
            for(auto it=entries.begin();it!=entries.end();)if((*it)["State"]=="restored"||(*it)["State"]=="not-deleted"||(*it)["State"]=="scrapped"||(*it)["State"]=="scrap-retained")it=entries.erase(it);else ++it;
            const auto instance=HelpyPersonalization::Guid(item.at("InstanceGuid").get<std::string>());
            for(const auto& [key,entry]:entries.items())if(entry["State"]!="restored"&&entry["State"]!="not-deleted"
                &&HelpyPersonalization::Guid(entry.at("Item").at("InstanceGuid").get<std::string>())==instance)
                throw std::runtime_error("This item already has an active or uncertain recovery record");
            if(entries.size()>=MaximumItems)throw std::runtime_error("Helpy Storage is full; native deletion remains available");
            entries[id]={{"Item",item},{"State",plan.is_null()?"pending-delete":"pending-scrap"},{"CapturedAt",Now()}};
            if(!plan.is_null())entries[id]["ScrapPlan"]=plan;
        });
    }
    Json PrepareDelete(const std::string& id,const Json& item){return Prepare(id,item);}
    Json PrepareScrap(const std::string& id,const Json& item,const Json& plan){
        ScrapPlan(plan,item,character);
        return Prepare(id,item,plan);
    }
    Json ObserveScrap(const std::string& id,DeleteResult result){return Update([&](Json& document){auto& entry=document["Items"].at(id);
        if(entry.at("State")!="pending-scrap")throw std::runtime_error("Stale scrap deletion receipt");
        entry["State"]=result==DeleteResult::Removed?"scrap-deleted":result==DeleteResult::Retained?"scrap-retained":"scrap-unconfirmed";entry["ObservedAt"]=Now();});}
    Json FinishScrap(const std::string& id,bool confirmed,const Json& drops){
        return Update([&](Json& document){auto& entry=document["Items"].at(id);if(entry.at("State")!="scrap-deleted")throw std::runtime_error("Scrap deletion was not confirmed");
            ScrapDrops(entry.at("ScrapPlan"),drops,confirmed);
            entry["State"]=confirmed?"scrapped":"scrap-unconfirmed";entry["Drops"]=drops;entry["ObservedAt"]=Now();});
    }
    Json ObserveDelete(const std::string& id,DeleteResult result){return Update([&](Json& document){auto& entry=document["Items"].at(id);if(entry.at("State")!="pending-delete")return;
        entry["State"]=result==DeleteResult::Removed?"stored":result==DeleteResult::Retained?"not-deleted":"unconfirmed";entry["ObservedAt"]=Now();});}
    Json BeginRestore(const std::string& id,const std::string& attempt,uint64_t expectedRevision,bool alreadyPresent){
        if(!Token(attempt)||alreadyPresent)throw std::runtime_error("Restore is blocked: invalid request or item already present");
        return Update([&](Json& document){if(document["Revision"]!=expectedRevision)throw std::runtime_error("Storage changed; refresh before restoring");auto& entry=document["Items"].at(id);
            if(entry.at("State")!="stored")throw std::runtime_error("Only confirmed deleted items can be restored");entry["State"]="restore-pending";entry["RestoreAttempt"]=attempt;});
    }
    Json FinishRestore(const std::string& id,const std::string& attempt,RestoreResult result){return Update([&](Json& document){auto& entry=document["Items"].at(id);
        if(entry.at("State")!="restore-pending"||entry.value("RestoreAttempt",std::string{})!=attempt)throw std::runtime_error("Stale or duplicate restore receipt");
        entry["State"]=result==RestoreResult::Confirmed?"restored":result==RestoreResult::NotAttempted?"stored":"unconfirmed";entry["ObservedAt"]=Now();});}
    Json Rows()const{return Present(Read());}
};
}
