#pragma once
#include <nlohmann/json.hpp>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
// J4SSaves keeps mod data in a character save under a top-level envelope:
//   "J4SSaves": {"format":1, "mods":{...}, "sections":{"publisher.mod":[schema, revision, "HEX"]}}
// These helpers read that envelope, write Helpy backup files and stage restore or clean
// edits on a save draft. They never touch the J4SSaves database; the runtime bridge does.
namespace PS::J4SSections {
using Json=nlohmann::json;
inline constexpr std::string_view Envelope="J4SSaves";
inline constexpr size_t MaxPayload=128*1024,MaxSections=4096;
inline constexpr std::string_view BackupKind="helpy-j4ssaves-backup";

// Same rule as J4SSaves: dot-separated lowercase segments of [a-z0-9_-], up to 96 bytes.
inline bool ValidOwner(std::string_view owner){
    if(owner.empty()||owner.size()>96||owner.find('.')==owner.npos)return false;
    bool start=true;
    for(const char c:owner){
        const bool alnum=(c>='a'&&c<='z')||(c>='0'&&c<='9');
        if(alnum||(!start&&(c=='_'||c=='-')))start=false;
        else if(c=='.'&&!start)start=true;
        else return false;
    }
    return !start;
}
inline bool ValidHex(std::string_view hex){
    if(hex.empty()||hex.size()%2||hex.size()>MaxPayload*2)return false;
    for(const char c:hex)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;
    return true;
}
// A section entry as J4SSaves embeds it: [schema, revision, "HEX"].
inline bool ValidEntry(const Json& entry){
    if(!entry.is_array()||entry.size()!=3||!entry[0].is_number_integer()||!entry[1].is_number_integer()||!entry[2].is_string())return false;
    const auto schema=entry[0].get<int64_t>(),revision=entry[1].get<int64_t>();
    return schema>0&&schema<=2147483647&&revision>0&&ValidHex(entry[2].get_ref<const std::string&>());
}

struct Section{std::string Owner;int64_t Schema=0,Revision=0;size_t Bytes=0;bool Valid=false;};
inline const Json* Sections(const Json& save){
    if(!save.is_object())return nullptr;
    const auto envelope=save.find(Envelope);if(envelope==save.end()||!envelope->is_object())return nullptr;
    const auto sections=envelope->find("sections");if(sections==envelope->end()||!sections->is_object())return nullptr;
    return &*sections;
}
inline std::vector<Section> List(const Json& save){
    std::vector<Section> rows;const auto* sections=Sections(save);if(!sections)return rows;
    for(const auto& [owner,entry]:sections->items()){
        if(rows.size()==MaxSections)throw std::runtime_error("J4SSaves envelope has too many sections");
        Section row{owner};
        if(ValidOwner(owner)&&ValidEntry(entry)){row.Schema=entry[0];row.Revision=entry[1];row.Bytes=entry[2].get_ref<const std::string&>().size()/2;row.Valid=true;}
        rows.push_back(std::move(row));
    }
    return rows;
}
inline std::optional<Json> Entry(const Json& save,std::string_view owner){
    const auto* sections=Sections(save);if(!sections)return std::nullopt;
    const auto found=sections->find(std::string(owner));if(found==sections->end()||!ValidEntry(*found))return std::nullopt;
    return *found;
}
inline Json Rows(const std::vector<Section>& sections){
    Json rows=Json::array();
    for(const auto& s:sections)rows.push_back({{"Owner",s.Owner},{"Schema",s.Schema},{"Revision",s.Revision},{"Bytes",s.Bytes},{"Valid",s.Valid}});
    return rows;
}

// Stages one section on a copy of the save: a value sets it, nullopt removes it. An
// envelope is created when restoring into a save that never had one.
inline Json WithSection(Json save,std::string_view owner,const std::optional<Json>& entry){
    if(!ValidOwner(owner))throw std::runtime_error("Invalid J4SSaves section owner");
    if(entry&&!ValidEntry(*entry))throw std::runtime_error("Invalid J4SSaves section entry");
    if(!save.is_object())throw std::runtime_error("Character save is not an object");
    if(!entry&&!save.contains(Envelope))return save;
    auto& envelope=save[std::string(Envelope)];
    if(envelope.is_null()){envelope={{"format",1},{"mods",Json::object()},{"sections",Json::object()}};}
    if(!envelope.is_object())throw std::runtime_error("Unsupported J4SSaves envelope");
    auto& sections=envelope["sections"];if(sections.is_null())sections=Json::object();
    if(!sections.is_object())throw std::runtime_error("Unsupported J4SSaves sections");
    if(entry)sections[std::string(owner)]=*entry;else sections.erase(std::string(owner));
    return save;
}

// Helpy backup file: one character, any number of sections, payloads as embedded hex.
inline Json Backup(const Json& save,const std::string& character,const std::string& name,const std::vector<std::string>& owners,const std::string& createdAt){
    Json sections=Json::object();
    for(const auto& owner:owners){
        auto entry=Entry(save,owner);if(!entry)throw std::runtime_error("Section "+owner+" is not in this save");
        sections[owner]=*entry;
    }
    if(sections.empty())throw std::runtime_error("Choose at least one section to back up");
    return {{"kind",BackupKind},{"version",1},{"character",character},{"characterName",name},{"createdAt",createdAt},{"sections",sections}};
}
inline Json ReadBackup(const Json& backup,const std::string& character){
    if(!backup.is_object()||backup.value("kind",std::string{})!=BackupKind||backup.value("version",0)!=1)throw std::runtime_error("Not a Helpy J4SSaves backup");
    if(backup.value("character",std::string{})!=character)throw std::runtime_error("This backup belongs to a different character");
    const auto sections=backup.find("sections");if(sections==backup.end()||!sections->is_object()||sections->empty()||sections->size()>MaxSections)throw std::runtime_error("Backup has no sections");
    for(const auto& [owner,entry]:sections->items())if(!ValidOwner(owner)||!ValidEntry(entry))throw std::runtime_error("Backup section "+owner+" is damaged");
    return *sections;
}
// Backup file names: <UTC stamp>-<count>.json. The stamp sorts newest last.
inline std::string BackupName(const std::string& stamp,size_t count){
    std::string safe;for(const char c:stamp)if((c>='0'&&c<='9')||c=='T'||c=='Z')safe+=c;
    if(safe.empty())throw std::runtime_error("Backup time is unavailable");
    return safe+"-"+std::to_string(count)+(count==1?"-section":"-sections")+".json";
}
}
