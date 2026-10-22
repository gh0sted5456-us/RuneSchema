#pragma once
#include "Core/HelpyPersonalization.h"
#include <chrono>
#include <map>
namespace PS::HelpyMarkSync {
using namespace HelpyPersonalization;
inline std::mutex SyncGate;
struct Transfer{uint64_t Version=0;size_t Next=0;Json Rows=Json::object();std::chrono::steady_clock::time_point Touched;std::string Last;Json Receipt;};
inline std::map<std::string,Transfer> Pending;
inline bool OwnKey(const std::string& key,const std::string& character){
    return (StackKeyValid(key)&&key.substr(6,32)==character)||(TypeKeyValid(key)&&key.substr(5,32)==character);
}
inline Json Labels(const Json& row){
    Json result={{"Favorite",nullptr},{"Junk",nullptr}};
    if(row.contains("Favorite"))result["Favorite"]=row.at("Favorite");
    if(row.contains("Tags")){bool junk=false;for(const auto& tag:row.at("Tags"))if(Fold(tag.get<std::string>())=="junk")junk=true;result["Junk"]=junk;}
    return result;
}
inline Json Export(const Json& request){
    const auto character=Guid(request.at("CharacterGuid").get<std::string>());if(character.empty())throw std::runtime_error("A character identity is required for mark sync");
    Json rows=Json::array();{std::shared_lock lock(Gate);for(const auto& [key,row]:Entries.items())if(OwnKey(key,character))rows.push_back({{"Key",key},{"Labels",Labels(row)}});}
    const auto text=rows.dump();uint64_t version=1469598103934665603ull;for(unsigned char c:text){version^=c;version*=1099511628211ull;}
    size_t offset=request.value("Offset",size_t{});if(request.value("Version",uint64_t{})!=version)offset=0;
    if(offset>rows.size())throw std::runtime_error("Invalid mark page cursor");
    Json batch={{"CharacterGuid",character},{"Version",version},{"Offset",offset},{"Next",offset},{"Done",false},{"Rows",Json::array()}};
    size_t next=offset;for(;next<rows.size();++next){batch["Rows"].push_back(rows[next]);if(batch.dump().size()>3000){batch["Rows"].erase(batch["Rows"].size()-1);break;}}
    if(next==offset&&next<rows.size())throw std::runtime_error("An item identity exceeds the network mark packet bound");
    batch["Next"]=next;batch["Done"]=next==rows.size();return batch;
}
inline Json Receive(const Json& batch){
    if(!batch.is_object()||batch.size()!=6||batch.dump().size()>3200)throw std::runtime_error("Invalid mark packet size");
    const auto character=batch.at("CharacterGuid").get<std::string>();if(Guid(character)!=character)throw std::runtime_error("Invalid mark owner");
    if(!batch.at("Version").is_number_unsigned()||!batch.at("Offset").is_number_unsigned()||!batch.at("Next").is_number_unsigned()||!batch.at("Done").is_boolean())throw std::runtime_error("Invalid mark cursor");
    const auto version=batch.at("Version").get<uint64_t>();const auto offset=batch.at("Offset").get<size_t>(),next=batch.at("Next").get<size_t>();const auto& rows=batch.at("Rows");
    if(!rows.is_array()||rows.size()>32||next!=offset+rows.size()||next>16384||(!batch.at("Done").get<bool>()&&rows.empty()))throw std::runtime_error("Invalid mark page");
    Json decoded=Json::object();for(const auto& row:rows){
        if(!row.is_object()||row.size()!=2)throw std::runtime_error("Invalid mark row");const auto key=row.at("Key").get<std::string>();const auto& labels=row.at("Labels");
        if(!OwnKey(key,character)||decoded.contains(key)||!labels.is_object()||labels.size()!=2)throw std::runtime_error("Foreign or duplicate character mark");
        for(const auto* field:{"Favorite","Junk"})if(!labels.at(field).is_null()&&!labels.at(field).is_boolean())throw std::runtime_error("Invalid Favorite/Junk mark");
        decoded[key]=labels;
    }
    std::lock_guard lock(SyncGate);const auto now=std::chrono::steady_clock::now();
    std::erase_if(Pending,[&](const auto& entry){return now-entry.second.Touched>std::chrono::seconds(30);});
    if(!Pending.contains(character)&&Pending.size()>=64)throw std::runtime_error("Mark sync is busy");
    auto& transfer=Pending[character];if(transfer.Last==batch.dump()){transfer.Touched=now;return transfer.Receipt;}
    if(offset==0)transfer={version,0,Json::object(),now};
    if(transfer.Version!=version||transfer.Next!=offset)throw std::runtime_error("Mark sync cursor changed; restart the character snapshot");
    auto combined=transfer.Rows;for(const auto& [key,value]:decoded.items()){if(combined.contains(key))throw std::runtime_error("Duplicate mark across pages");combined[key]=value;}
    size_t bytes=combined.dump().size();for(const auto& [owner,other]:Pending)if(owner!=character)bytes+=other.Rows.dump().size();
    if(bytes>4*1024*1024)throw std::runtime_error("Mark sync exceeds its memory bound");
    if(batch.at("Done").get<bool>()){
        Json patches=Json::object();{std::shared_lock guard(Gate);for(const auto& [key,value]:Entries.items())if(key.starts_with("remote:")&&OwnKey(key.substr(7),character))patches[key]=nullptr;}
        for(const auto& [key,value]:combined.items())patches["remote:"+key]={{"Favorite",value.at("Favorite")},{"Tags",value.at("Junk").is_null()?Json{}:value.at("Junk").get<bool>()?Json::array({"junk"}):Json::array()}};
        if(!patches.empty()&&!SetMany(patches))throw std::runtime_error(Status);
        transfer.Rows=Json::object();transfer.Next=next;
    }else {transfer.Rows=std::move(combined);transfer.Next=next;transfer.Touched=now;}
    transfer.Last=batch.dump();transfer.Touched=now;transfer.Receipt={{"CharacterGuid",character},{"Version",version},{"Offset",offset},{"Next",next},{"Done",batch.at("Done")}};
    return transfer.Receipt;
}
}
