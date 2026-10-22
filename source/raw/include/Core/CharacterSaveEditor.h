#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <set>
#include <vector>
#include <map>
#include "Core/CharacterWorkbench.h"

// Independent, copy-only character editing. No file I/O or provider writes.
namespace PS::CharacterSaveEditor {
using Json=nlohmann::json;
inline std::string Guid(const Json& value){
    if(!value.is_string())return {};auto id=value.get<std::string>();
    if(id.size()!=32||id.find_first_not_of("0123456789abcdefABCDEF")!=id.npos||id.find_first_not_of('0')==id.npos)return {};
    for(auto& c:id)if(c>='a'&&c<='f')c=char(c-'a'+'A');return id;
}
inline std::string Identity(const Json& value){
    if(!value.is_object())return {};
    const auto metadata=value.find("meta_data"),legacy=value.find("GUID");
    if(metadata!=value.end()&&metadata->is_object()&&metadata->contains("char_guid")){
        const auto id=Guid(metadata->at("char_guid"));
        if(legacy!=value.end()&&Guid(*legacy)!=id)return {};return id;
    }
    return legacy==value.end()?std::string{}:Guid(*legacy);
}
inline void RequireCharacter(const Json& value){
    if(!value.is_object()||!value.contains("GameProgress")||!value.at("GameProgress").is_object())
        throw std::runtime_error("This editor requires a native GameProgress character JSON payload. World/SPUD and Xbox provider containers need a separate adapter.");
}
inline std::string Escape(std::string text){std::string out;for(char c:text){if(c=='~')out+="~0";else if(c=='/')out+="~1";else out+=c;}return out;}
inline bool Protected(const std::string& path){
    if(path.empty()||path.size()>4096||path.front()!='/')return true;
    Json::json_pointer pointer(path); // Validate escaping even for read-only paths.
    if(!(path.starts_with("/GameProgress/")||path.starts_with("/Customization/")||path.starts_with("/Character/")))return true;
    if(path.starts_with("/GameProgress/QuestProgress/")||path.starts_with("/GameProgress/Journal/"))return true;
    if(path.starts_with("/GameProgress/Inventory/")||path.starts_with("/GameProgress/PersonalInventory/")||path.starts_with("/GameProgress/Loadout/"))return true;
    // Identity, versions, ownership and slot bookkeeping are never generic edits.
    size_t start=1;
    while(start<path.size()){
        const auto end=path.find('/',start);auto key=path.substr(start,end==path.npos?path.npos:end-start);
        std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        if(key.find("guid")!=key.npos||key.find("persistence")!=key.npos||key.find("ownership")!=key.npos||key.find("runeschema")!=key.npos
            ||key=="version"||key=="itemdata"||key=="maxslotindex"||key=="playerinventoryitemindex"||key.ends_with("id")||key.ends_with("ids"))return true;
        if(end==path.npos)break;start=end+1;
    }
    return false;
}
class Session {
    Json original,draft,review;
    uint64_t revision=0,sequence=0;
    std::string receipt;
    static std::pair<std::string,std::string> ItemPointer(const std::string& pointer){
        for(const auto* container:{"Inventory","PersonalInventory"}){const auto prefix=std::string("/GameProgress/")+container+"/";
            if(pointer.starts_with(prefix)){const auto slot=pointer.substr(prefix.size());
                if(slot.empty()||slot.size()>10||slot.find_first_not_of("0123456789")!=slot.npos||std::stoull(slot)>2147483647||std::to_string(std::stoull(slot))!=slot)break;
                return {container,slot};}}
        throw std::runtime_error("Select a native numeric inventory slot, not an arbitrary field");
    }
    void Commit(Json next,uint64_t expected){
        if(!Ready()||expected!=revision)throw std::runtime_error("The editor changed; refresh before editing");
        if(Json::diff(original,next).size()>512)throw std::runtime_error("Export or discard this batch before making more than 512 edits");
        draft=std::move(next);receipt.clear();review=nullptr;++revision;
    }
public:
    void Reset(){original=nullptr;draft=nullptr;review=nullptr;receipt.clear();++revision;}
    void Open(const Json& value){Reset();RequireCharacter(value);original=value;draft=value;}
    bool Ready()const{return !original.is_null();}
    const Json& Original()const{return original;}
    const Json& Draft()const{if(!Ready())throw std::runtime_error("Open a character copy first");return draft;}
    uint64_t Revision()const{return revision;}
    std::string CharacterGuid()const{return Identity(Draft());}
    void SetSkillLevel(const std::string& id,int level,uint64_t expected,const Json& thresholds,int maximum){
        if(!Ready()||expected!=revision||maximum<1||maximum>200||level<1||level>maximum||!thresholds.is_array()||thresholds.size()<size_t(maximum))throw std::runtime_error("Verified native skill thresholds and current revision required");
        int64_t previous=-1;for(int i=0;i<maximum;++i){const auto& value=thresholds[i];if(!value.is_number_integer()||value<0||value>2147483647||value.get<int64_t>()<=previous)throw std::runtime_error("Invalid native skill XP curve");previous=value.get<int64_t>();}
        auto next=draft;auto& skills=next.at("GameProgress").at("Skills").at("Skills");if(!skills.is_array()||skills.size()>128)throw std::runtime_error("Unsupported saved skills");Json* chosen=nullptr;for(auto& skill:skills)if(skill.value("Id",std::string{})==id){if(chosen)throw std::runtime_error("Duplicate skill identity");chosen=&skill;}
        if(!chosen||!chosen->contains("Xp")||!chosen->at("Xp").is_number_integer())throw std::runtime_error("Saved skill XP unavailable");(*chosen)["Xp"]=thresholds[level-1];Commit(std::move(next),expected);
    }
    Json Loadout()const{
        const auto& game=Draft().at("GameProgress");Json rows=Json::array();if(!game.contains("Loadout"))return rows;const auto& loadout=game.at("Loadout");if(!loadout.is_object()||loadout.size()>65)throw std::runtime_error("Unsupported saved loadout");
        for(const auto& [key,value]:loadout.items())if(key!="MaxSlotIndex"){
            if(key.empty()||key.size()>2||key.find_first_not_of("0123456789")!=key.npos||std::stoi(key)>63||!value.is_object())continue;const auto index=value.value("PlayerInventoryItemIndex",-1);if(index<0||index>4096)continue;const auto slot=std::to_string(index);const auto& bag=game.at("Inventory");if(!bag.contains(slot)||!bag[slot].is_object()||!bag[slot].contains("ItemData"))continue;
            rows.push_back({{"LoadoutSlot",std::stoi(key)},{"Slot",slot},{"Pointer","/GameProgress/Inventory/"+slot},{"PersistenceID",bag[slot].at("ItemData")},{"Equipped",true},{"Count",bag[slot].value("Count",Json(1))}});
        }return rows;
    }
    Json Appearance()const{return {{"Name",draft.contains("meta_data")?draft.at("meta_data").value("char_name",std::string{}):std::string{}},{"CharacterGuid",CharacterGuid()},{"Appearance",CharacterWorkbench::Appearance(Draft())},{"Revision",revision},{"BeardColorSupported",false}};}
    void SetAppearance(const std::string& key,const std::string& row,uint64_t expected,const std::set<std::string>& verifiedRows){
        SetAppearanceBatch({{key,row}},expected,{{key,verifiedRows}});
    }
    void SetAppearanceBatch(const std::map<std::string,std::string>& changes,uint64_t expected,const std::map<std::string,std::set<std::string>>& verifiedRows){
        if(!Ready()||expected!=revision||changes.empty()||changes.size()>CharacterWorkbench::Fields.size())throw std::runtime_error("Refresh the native appearance choices and editor revision");
        auto next=draft;
        for(const auto& [key,row]:changes){const auto choices=verifiedRows.find(key);if(choices==verifiedRows.end()||!choices->second.contains(row))throw std::runtime_error("Appearance row is not in the verified native table");const auto& field=CharacterWorkbench::Find(key);(void)CharacterWorkbench::Row(CharacterWorkbench::Appearance(draft),field);next["Customization"]["CustomizationData"][key]["rowName"]=row;}
        Commit(std::move(next),expected);
    }
    void Rename(const std::string& name,uint64_t expected){
        if(!Ready()||expected!=revision||name.empty()||name.size()>96||!draft.contains("meta_data")||!draft["meta_data"].contains("char_name")||!draft["meta_data"]["char_name"].is_string())throw std::runtime_error("Native character name unavailable or invalid");
        for(unsigned char c:name)if(c<32||c==127||c=='/'||c=='\\'||c==':'||c=='"'||c=='<'||c=='>'||c=='|'||c=='?'||c=='*')throw std::runtime_error("Character name contains unsupported characters");
        if(name.front()==' '||name.back()==' '||name.back()=='.')throw std::runtime_error("Remove leading/trailing spaces or dots from the character name");
        auto next=draft;next["meta_data"]["char_name"]=name;Commit(std::move(next),expected);
    }
    // The host constructs native base records or copies an exact same-ItemData
    // saved template. Never trust a UI-supplied record or overwrite a slot.
    void AddRecord(const Json& record,uint64_t expected,const std::string& container="Inventory",int verifiedSlot=-1){
        if(!Ready()||expected!=revision||(container!="Inventory"&&container!="PersonalInventory")||!record.is_object()||!record.contains("ItemData")||!record["ItemData"].is_string()||!record.contains("GUID")||!record["GUID"].is_string())throw std::runtime_error("Verified native item record and current revision required");
        const auto& slots=draft.at("GameProgress").at(container);if(!slots.is_object()||slots.size()>4096||!slots.contains("MaxSlotIndex")||!slots["MaxSlotIndex"].is_number_integer())throw std::runtime_error("Native inventory layout unavailable");
        int64_t index=slots["MaxSlotIndex"].get<int64_t>();if(index< -1||index>=2147483646)throw std::runtime_error("Native inventory index exceeds its bound");
        for(const auto& [key,item]:slots.items())if(key!="MaxSlotIndex"){const auto [unused,slot]=ItemPointer("/GameProgress/"+container+"/"+key);index=std::max(index,std::stoll(slot));}
        for(const auto* c:{"Inventory","PersonalInventory"})if(draft["GameProgress"].contains(c))for(const auto& item:draft["GameProgress"][c])if(item.is_object()&&item.value("GUID",std::string{})==record["GUID"].get<std::string>())throw std::runtime_error("Duplicate item instance GUID");
        const int64_t destination=verifiedSlot>=0?verifiedSlot:index+1;if(destination>4096||slots.contains(std::to_string(destination)))throw std::runtime_error("Native target slot is occupied or exceeds the supported bound");
        auto next=draft;next["GameProgress"][container][std::to_string(destination)]=record;next["GameProgress"][container]["MaxSlotIndex"]=std::max(index,destination);Commit(std::move(next),expected);
    }
    std::string ItemIdentity(const std::string& pointer)const{(void)ItemPointer(pointer);if(!Ready())throw std::runtime_error("Open a character copy first");return draft.at(Json::json_pointer(pointer)).at("ItemData").get<std::string>();}
    Json Inventory(size_t offset=0)const{
        if(!Ready())throw std::runtime_error("Open a character copy first");if(offset>8192)throw std::runtime_error("Inventory offset exceeds bounds");Json rows=Json::array();size_t total=0;
        const auto& game=draft.at("GameProgress");
        for(const auto* container:{"Inventory","PersonalInventory"}){
            if(!game.contains(container))continue;const auto& values=game.at(container);
            if(!values.is_object()||values.size()>4097)throw std::runtime_error("Unsupported native inventory layout");
            std::vector<std::pair<uint64_t,std::string>> slots;
            for(const auto& [slot,item]:values.items())if(slot!="MaxSlotIndex"&&item.is_object()&&item.contains("ItemData")&&item.at("ItemData").is_string()){
                try{const auto pointer=std::string("/GameProgress/")+container+"/"+slot;(void)ItemPointer(pointer);slots.emplace_back(std::stoull(slot),slot);}catch(...){}}
            std::sort(slots.begin(),slots.end());
            for(const auto& [number,slot]:slots){if(++total>8192)throw std::runtime_error("Inventory exceeds editor bounds");if(total<=offset||rows.size()>=100)continue;
                const auto& item=values.at(slot);const auto id=item.at("ItemData").get<std::string>();if(id.empty()||id.size()>2048)continue;
                const bool count=item.contains("Count")&&item.at("Count").is_number_integer()&&item.at("Count")>=1&&item.at("Count")<=10000;
                bool equipped=false;if(std::string_view(container)=="Inventory"&&game.contains("Loadout")&&game.at("Loadout").is_object())
                    for(const auto& value:game.at("Loadout"))if(value.is_object()&&value.contains("PlayerInventoryItemIndex")&&value.at("PlayerInventoryItemIndex").is_number_integer()&&value.at("PlayerInventoryItemIndex")==number)equipped=true;
                rows.push_back({{"Pointer",std::string("/GameProgress/")+container+"/"+slot},{"Container",container},{"Slot",slot},{"PersistenceID",id},
                    {"InstanceID",item.value("GUID",Json{})},{"Count",item.value("Count",Json(1))},{"StackEditable",count},{"Equipped",equipped},
                    {"DisplayName",nullptr},{"AssetPath",nullptr},{"Origin","Unresolved"}});
            }
        }
        return {{"Items",rows},{"Total",total},{"Offset",offset},{"Revision",revision},{"Status","Draft inventory. Originals unchanged; unresolved identities are not evidence of missing content."}};
    }
    void SetCount(const std::string& pointer,int64_t count,uint64_t expected,int64_t verifiedMaximum=0){
        (void)ItemPointer(pointer);if(!Ready()||expected!=revision)throw std::runtime_error("Refresh inventory before editing");
        const auto& item=draft.at(Json::json_pointer(pointer));
        if(count<1||count>10000||!item.is_object()||!item.contains("ItemData")||!item.at("ItemData").is_string()
            ||!item.contains("Count")||!item.at("Count").is_number_integer()||item.at("Count")<1||item.at("Count")>10000)
            throw std::runtime_error("Change an existing native Count field within 1..10000; single-item records have no editable stack field");
        if(count>item.at("Count").get<int64_t>()&&(verifiedMaximum<1||verifiedMaximum>10000||count>verifiedMaximum))
            throw std::runtime_error("Increasing this stack requires an unambiguous loaded ItemData MaxStackSize; decrease it or load its source first");
        auto next=draft;next.at(Json::json_pointer(pointer))["Count"]=count;Commit(std::move(next),expected);
    }
    void RemoveItems(const Json& pointers,uint64_t expected,bool confirmed){
        if(!Ready()||expected!=revision||!confirmed||!pointers.is_array()||pointers.empty()||pointers.size()>128)
            throw std::runtime_error("Review and confirm 1..128 selected items using the current editor revision");
        std::set<std::string> unique,inventorySlots;auto next=draft;
        for(const auto& value:pointers){if(!value.is_string())throw std::runtime_error("Invalid item selection");const auto pointer=value.get<std::string>();
            const auto [container,slot]=ItemPointer(pointer);if(!unique.insert(pointer).second)throw std::runtime_error("Duplicate item selection");
            const auto& item=draft.at(Json::json_pointer(pointer));if(!item.is_object()||!item.contains("ItemData")||!item.at("ItemData").is_string())throw std::runtime_error("Unsupported inventory record");
            next.at("GameProgress").at(container).erase(slot);if(container=="Inventory")inventorySlots.insert(slot);
        }
        auto& game=next.at("GameProgress");if(!inventorySlots.empty()&&game.contains("Loadout")){
            auto& loadout=game.at("Loadout");if(!loadout.is_object()||loadout.size()>256)throw std::runtime_error("Unsupported native equipment links");
            for(auto it=loadout.begin();it!=loadout.end();){const auto& item=it.value();bool remove=false;
                if(item.is_object()&&item.contains("PlayerInventoryItemIndex")){
                    const auto& slot=item.at("PlayerInventoryItemIndex");if(!slot.is_number_integer()||slot<0||slot>2147483647)throw std::runtime_error("Unsupported native equipment reference; no removal staged");
                    remove=inventorySlots.contains(std::to_string(slot.get<int64_t>()));}
                if(remove)it=loadout.erase(it);else ++it;
            }
        }
        Commit(std::move(next),expected);
    }
    Json Browse(const std::string& path="",size_t offset=0)const{
        if(!Ready())throw std::runtime_error("Open a character copy first");
        const auto& value=draft.at(Json::json_pointer(path));Json rows=Json::array();
        if(!value.is_structured())throw std::runtime_error("Browse an object or array, then select a field to edit");
        const auto add=[&](const std::string& key,const Json& child){
            const auto pointer=path+"/"+Escape(key);const bool scalar=child.is_primitive()&&!child.is_null();
            Json row={{"Key",key},{"Pointer",pointer},{"Type",child.type_name()},{"Children",child.is_structured()?child.size():0},{"Editable",scalar&&!(value.is_array()&&child.is_string())&&!Protected(pointer)}};
            if(child.is_primitive()&&child.dump().size()<=8192)row["Value"]=child;
            rows.push_back(std::move(row));
        };
        if(value.is_structured()){size_t index=0;for(auto it=value.begin();it!=value.end();++it,++index){if(index<offset)continue;if(rows.size()==100)break;add(value.is_object()?it.key():std::to_string(index),it.value());}}
        else add(path.substr(path.find_last_of('/')+1),value);
        return {{"Pointer",path},{"Rows",rows},{"Total",value.is_structured()?value.size():1},{"Offset",offset},{"Revision",revision},{"CharacterGuid",CharacterGuid()}};
    }
    void Set(const std::string& path,const Json& value,uint64_t expected){
        receipt.clear();review=nullptr;
        if(!Ready()||expected!=revision)throw std::runtime_error("The editor changed; refresh the field before editing");
        if(Protected(path))throw std::runtime_error("Identity, ownership, version and container structure are read-only");
        auto& target=draft.at(Json::json_pointer(path));
        const bool numeric=target.is_number()&&value.is_number();
        if(!target.is_primitive()||target.is_null()||!value.is_primitive()||value.is_null()
            ||(!numeric&&target.type()!=value.type())||value.dump().size()>8192)
            throw std::runtime_error("Edit an existing scalar using the same value type");
        const auto parent=path.substr(0,path.find_last_of('/'));
        if(target.is_string()&&draft.at(Json::json_pointer(parent)).is_array())throw std::runtime_error("Identity/unlock lists are read-only; use reviewed cleanup instead");
        if(value.is_number_float()&&!std::isfinite(value.get<double>()))throw std::runtime_error("Numbers must be finite");
        if(target.is_number_integer()&&!value.is_number_integer())throw std::runtime_error("This field requires an integer");
        if(target.is_number_unsigned()&&value.is_number_integer()&&!value.is_number_unsigned()&&value.get<int64_t>()<0)throw std::runtime_error("This field requires a nonnegative integer");
        auto next=draft;next.at(Json::json_pointer(path))=value;
        if(Json::diff(original,next).size()>512)throw std::runtime_error("Export or discard this batch before making more than 512 edits");
        draft=std::move(next);++revision;
    }
    Json Preview(const Json& current){
        receipt.clear();if(!Ready()||current!=original)throw std::runtime_error("Source changed; open it again before export");
        review=Json::diff(original,draft);receipt="save-copy-"+std::to_string(++sequence)+"-"+std::to_string(revision);
        Json summary=Json::array();for(const auto& change:review){const auto path=change.at("path").get<std::string>();const auto op=change.at("op").get<std::string>();
            Json row={{"Action",op},{"Field",path},{"Label",op=="remove"?"Remove saved entry":"Change "+path.substr(path.find_last_of('/')+1)}};
            try{const auto& before=original.at(Json::json_pointer(path));if(op=="remove"&&before.is_object()&&before.contains("ItemData"))row["Item"]=before.at("ItemData");else if(before.is_primitive())row["Before"]=before;}catch(...){}
            try{if(path.ends_with("/Count")){const auto& item=original.at(Json::json_pointer(path.substr(0,path.size()-6)));if(item.contains("ItemData")){row["Item"]=item.at("ItemData");row["Label"]="Change stack count";}}}catch(...){}
            if(op=="remove"&&path.starts_with("/GameProgress/Loadout/"))row["Label"]="Unlink equipment slot";
            else if(op=="remove"&&row.contains("Item"))row["Label"]="Remove inventory item";
            if(change.contains("value")&&change.at("value").is_primitive())row["After"]=change.at("value");summary.push_back(std::move(row));}
        return {{"Token",receipt},{"Changes",review},{"Summary",summary},{"Count",review.size()},{"Revision",revision},{"Status","Review the changes. Export creates a new copy; no save is attached or overwritten."}};
    }
    Json Consume(const Json& current,const std::string& token,bool confirmed){
        const bool valid=confirmed&&!receipt.empty()&&receipt==token;receipt.clear();
        if(!valid||!Ready()||current!=original||review!=Json::diff(original,draft))throw std::runtime_error("Export preview expired, source changed or confirmation is missing; review again");
        return draft;
    }
};
}
