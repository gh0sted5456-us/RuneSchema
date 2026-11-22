#pragma once
#include "Core/ConfigFiles.h"
#include "Core/HelpyFlagSignature.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>
// Helpy delivery flags, shared by Helpy and the RuneSchema core (keep both copies identical).
// Player-driven Helpy deliveries are on by default. They stop for a character when:
//   - the player turns them off for that character in Helpy (the user opt-out), or
//   - a registered mod has a valid, signed noPlayerDeliveries flag on the character or world.
// A mod lifts its pause with a newer "on": false statement. With "until" (unix seconds) the lift is
// temporary: after that moment, by this PC's clock, the pause is back without another statement.
// A character exception ("deliveryPauseException", target character, "on": true) exempts one character
// from that same mod's pauses: its character pause and its pause on any world. Nothing else changes:
// the player's own opt-out and every other mod's pauses still apply, and other characters in the same
// world stay paused. An exception is always a lease: it needs "at" and "until", at most 30 days apart,
// and ends by itself at "until" by this PC's clock. A newer "on": false revokes it; on a tied seq the
// revocation wins, so a replayed or reordered allowance can never override a revocation.
// Deliveries a server makes through Helpy to another player are never blocked here.
// Folder: <Helpy shared folder>/flags/<mod id>/{mod.json,flags.json}, plus helpy-state.json,
// which Helpy owns and mods may read (user opt-outs, per-mod choices, pinned keys).
namespace PS::HelpyDeliveryFlags {
namespace fs=std::filesystem;
using Json=nlohmann::json;
inline constexpr std::string_view StatementPrefix="helpy-flag/1\n";
inline constexpr std::string_view PauseFlag="noPlayerDeliveries";
inline constexpr std::string_view PauseExceptionFlag="deliveryPauseException";
inline constexpr int64_t MaxExceptionSeconds=30LL*24*60*60;
inline constexpr size_t MaxMods=64,MaxStatements=4096,MaxStatementBytes=2048,MaxFlagsFileBytes=1024*1024,MaxStateBytes=1024*1024;
inline constexpr size_t MaxEvents=64;

// Mods Helpy recognises by key, so replacing a mod.json cannot lift that mod's flags.
// Fingerprint = "sha256:" + lower-case hex SHA-256 of the SPKI DER public key.
struct KnownMod{std::string_view Id,Name,Fingerprint;};
inline const std::vector<KnownMod>& Registry(){
    static const std::vector<KnownMod> known{
        // The main Bazaar's key (Grand Exchange assets/network.json, "main.fingerprint").
        {"grandexchange","Grand Exchange","sha256:ca226cec0400bbb4e78143e16031dbdb9383863960a5b7aee01abcd97735cb07"},
    };
    return known;
}

// 32 upper-case hex digits; braces and dashes are accepted on input. Empty when invalid.
inline std::string NormalizeGuid(std::string_view value){
    std::string out;
    for(const unsigned char c:value){
        if(c=='-'||c=='{'||c=='}')continue;
        if(!std::isxdigit(c))return {};
        out.push_back(static_cast<char>(std::toupper(c)));
    }
    return out.size()==32&&out!=std::string(32,'0')?out:std::string{};
}
inline bool ValidModId(std::string_view id){
    if(id.empty()||id.size()>40)return false;
    return std::all_of(id.begin(),id.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_';});
}

// Until: end of a temporary lift (an "on": false pause statement) or of a character exception (an
// "on": true exception statement, where it is required); 0 when there is no end.
// Lapsed: a temporary lift that has ended, so the flag pauses again.
struct Flag{std::string Mod,Target,Id,Name,Message;bool On=false,Lapsed=false;uint64_t Seq=0;int64_t At=0,Until=0;};
struct ModStatus{
    std::string Id,Name,Url,Fingerprint;
    enum class Trust{Registry,FirstSeen,Pinned,Mismatch,Invalid} Key=Trust::Invalid;
    bool AllowPause=true;std::string Problem;size_t Valid=0,Rejected=0;
    bool Trusted()const{return Key==Trust::Registry||Key==Trust::FirstSeen||Key==Trust::Pinned;}
};
struct Snapshot{
    std::vector<ModStatus> Mods;
    std::vector<Flag> Active;       // Newest valid flags that pause deliveries, from trusted mods the player allows.
    std::vector<Flag> Declined;     // Same, from mods the player chose not to honour.
    std::vector<Flag> Lifted;       // Temporary lifts still in force ("on": false with a future "until").
    std::vector<Flag> Excepted;     // Character exceptions in force: that mod's pauses skip that character.
    Json State=Json::object();
    std::vector<std::string> Problems;
};
struct Block{std::string Mod,ModName,Target,Id,Message;bool User=false;};

inline fs::path StatePath(const fs::path& folder){return folder/"helpy-state.json";}
inline Json EmptyState(){return {{"version",1},{"characters",Json::object()},{"mods",Json::object()},{"events",Json::array()}};}
inline Json ReadState(const fs::path& folder){
    std::error_code error;
    if(!fs::is_regular_file(StatePath(folder),error))return EmptyState();
    try{
        auto state=Json::parse(ConfigFiles::Read(StatePath(folder),MaxStateBytes));
        if(!state.is_object()||state.value("version",0)!=1)return EmptyState();
        for(const char* key:{"characters","mods"})if(!state.contains(key)||!state[key].is_object())state[key]=Json::object();
        if(!state.contains("events")||!state["events"].is_array())state["events"]=Json::array();
        return state;
    }catch(...){return EmptyState();}
}
inline void WriteState(const fs::path& folder,Json state){
    auto& events=state["events"];
    if(events.size()>MaxEvents)events.erase(events.begin(),events.begin()+static_cast<std::ptrdiff_t>(events.size()-MaxEvents));
    std::error_code error;fs::create_directories(folder,error);
    ConfigFiles::Write(StatePath(folder),state.dump(2));
}
inline void Record(Json& state,std::string event,int64_t at,Json detail=Json::object()){
    detail["event"]=std::move(event);detail["at"]=at;state["events"].push_back(std::move(detail));
}

// Parses one signed statement. The statement must carry the exact prefix, one line of JSON,
// and name the mod whose folder it came from.
inline std::optional<Flag> ParseStatement(std::string_view statement,std::string_view mod){
    if(statement.size()>MaxStatementBytes||!statement.starts_with(StatementPrefix))return std::nullopt;
    const auto body=statement.substr(StatementPrefix.size());
    if(body.find('\n')!=body.npos)return std::nullopt;
    try{
        const auto value=Json::parse(body);
        if(!value.is_object())return std::nullopt;
        Flag flag;
        flag.Mod=value.at("mod").get<std::string>();flag.Target=value.at("target").get<std::string>();
        flag.Id=NormalizeGuid(value.at("id").get<std::string>());flag.Name=value.at("flag").get<std::string>();
        flag.On=value.at("on").get<bool>();flag.Seq=value.at("seq").get<uint64_t>();
        flag.At=value.value("at",int64_t{});flag.Message=value.value("message",std::string{});
        if(const auto until=value.find("until");until!=value.end()){
            // A positive whole number of unix seconds. Anything else makes the statement invalid, so
            // a malformed lift can never stand in for an unlimited one.
            if(!until->is_number_unsigned())return std::nullopt;
            const auto seconds=until->get<uint64_t>();
            if(seconds==0||seconds>static_cast<uint64_t>(INT64_MAX))return std::nullopt;
            // Meaningless on a pause and on an exception's revocation; ignored there.
            if(flag.Name==PauseExceptionFlag?flag.On:!flag.On)flag.Until=static_cast<int64_t>(seconds);
        }
        if(flag.Mod!=mod||(flag.Target!="character"&&flag.Target!="world")||flag.Id.empty()||flag.Name.empty()||flag.Message.size()>300)return std::nullopt;
        // An exception names one character and, when granted, is a bounded lease, never open-ended.
        if(flag.Name==PauseExceptionFlag&&(flag.Target!="character"
            ||(flag.On&&(flag.At<=0||flag.Until<=flag.At||flag.Until-flag.At>MaxExceptionSeconds))))return std::nullopt;
        return flag;
    }catch(...){return std::nullopt;}
}

// Reads every mod folder. record=true lets the caller pin first-seen keys in helpy-state.json.
inline Snapshot Read(const fs::path& folder,const Signature::Verifier& verifier,int64_t now,bool record=true){
    Snapshot snapshot;snapshot.State=ReadState(folder);bool changed=false;
    std::error_code error;
    if(!fs::is_directory(folder,error))return snapshot;
    std::vector<fs::path> mods;
    for(fs::directory_iterator it(folder,fs::directory_options::skip_permission_denied,error),end;!error&&it!=end;it.increment(error)){
        if(it->is_symlink(error)||!it->is_directory(error))continue;
        if(mods.size()>=MaxMods){snapshot.Problems.push_back("More than "+std::to_string(MaxMods)+" mod folders; the rest were ignored.");break;}
        mods.push_back(it->path());
    }
    std::sort(mods.begin(),mods.end());
    std::map<std::tuple<std::string,std::string,std::string,std::string>,Flag> newest,pauses; // pauses: newest "on": true, for its message.
    for(const auto& path:mods){
        ModStatus status;status.Id=path.filename().string();status.Name=status.Id;
        auto& saved=snapshot.State["mods"][status.Id];if(!saved.is_object())saved=Json::object();
        status.AllowPause=saved.value("allowPause",true);
        const auto finish=[&]{snapshot.Mods.push_back(std::move(status));};
        if(!ValidModId(status.Id)){snapshot.State["mods"].erase(status.Id);status.Problem="Folder name is not a valid mod id.";finish();continue;}
        std::vector<uint8_t> key;
        try{
            const auto registration=Json::parse(ConfigFiles::Read(path/"mod.json",64*1024));
            if(registration.at("id").get<std::string>()!=status.Id)throw std::runtime_error("mod.json id does not match its folder.");
            status.Name=registration.value("name",status.Id).substr(0,80);status.Url=registration.value("url",std::string{}).substr(0,300);
            key=Signature::FromHex(registration.at("publicKey").get<std::string>());
            if(!Signature::P256Point(key))throw std::runtime_error("publicKey is not an ECDSA P-256 public key.");
            status.Fingerprint=Signature::Fingerprint(key);
        }catch(const std::exception& failure){status.Problem=std::string("Registration is invalid: ")+failure.what();finish();continue;}
        // Key trust: the built-in registry wins; otherwise the first key seen stays pinned.
        const auto known=std::find_if(Registry().begin(),Registry().end(),[&](const KnownMod& mod){return mod.Id==status.Id;});
        const auto pinned=saved.value("fingerprint",std::string{});
        if(known!=Registry().end())status.Key=known->Fingerprint==status.Fingerprint?ModStatus::Trust::Registry:ModStatus::Trust::Mismatch;
        else if(pinned.empty()){
            status.Key=ModStatus::Trust::FirstSeen;
            if(record){saved["fingerprint"]=status.Fingerprint;saved["pinnedAt"]=now;saved["name"]=status.Name;Record(snapshot.State,"key-pinned",now,{{"mod",status.Id},{"fingerprint",status.Fingerprint}});changed=true;}
        }else status.Key=pinned==status.Fingerprint?ModStatus::Trust::Pinned:ModStatus::Trust::Mismatch;
        if(status.Key==ModStatus::Trust::Mismatch){
            status.Problem=known!=Registry().end()?"Its key does not match the key Helpy knows for this mod. Its flags are ignored.":"Its key changed since Helpy first saw it. Reset the key in Settings if the change is expected.";
            finish();continue;
        }
        try{
            const auto document=Json::parse(ConfigFiles::Read(path/"flags.json",MaxFlagsFileBytes));
            const auto& rows=document.at("flags");
            if(!rows.is_array()||rows.size()>MaxStatements)throw std::runtime_error("flags must be an array of at most 4096 statements.");
            for(const auto& row:rows){
                const auto statement=row.value("statement",std::string{});std::vector<uint8_t> signature;
                try{signature=Signature::FromHex(row.value("sig",std::string{}));}catch(...){++status.Rejected;continue;}
                auto flag=ParseStatement(statement,status.Id);
                if(!flag||!verifier(key,statement,signature)){++status.Rejected;continue;}
                ++status.Valid;
                if(flag->On){auto& pause=pauses[{flag->Mod,flag->Target,flag->Id,flag->Name}];if(pause.Mod.empty()||flag->Seq>=pause.Seq)pause=*flag;}
                auto& slot=newest[{flag->Mod,flag->Target,flag->Id,flag->Name}];
                // Highest seq wins. On a tie the restrictive statement stands: a pause's "on", an
                // exception's "off". A replay can therefore never lift a pause or revive an exception.
                const auto restrictive=[](const Flag& value){return value.Name==PauseExceptionFlag?!value.On:value.On;};
                if(slot.Mod.empty()||flag->Seq>slot.Seq||(flag->Seq==slot.Seq&&restrictive(*flag)&&!restrictive(slot)))slot=std::move(*flag);
            }
        }catch(const std::exception& failure){
            std::error_code missing;
            if(fs::exists(path/"flags.json",missing))status.Problem=std::string("flags.json is invalid: ")+failure.what();
        }
        finish();
    }
    for(auto& [key,flag]:newest){
        if(flag.Name==PauseExceptionFlag){if(flag.On&&now<flag.Until)snapshot.Excepted.push_back(flag);continue;}
        if(flag.Name!=PauseFlag)continue;
        if(!flag.On&&flag.Until){
            if(now<flag.Until){snapshot.Lifted.push_back(flag);continue;}
            // The temporary lift has ended: the pause is back, with the mod's own words if it gave any.
            const auto pause=pauses.find(key);
            flag.On=true;flag.Lapsed=true;flag.Message=pause!=pauses.end()?pause->second.Message:std::string{};
        }
        if(!flag.On)continue;
        const auto mod=std::find_if(snapshot.Mods.begin(),snapshot.Mods.end(),[&](const ModStatus& status){return status.Id==flag.Mod;});
        (mod!=snapshot.Mods.end()&&mod->AllowPause?snapshot.Active:snapshot.Declined).push_back(flag);
    }
    if(changed)try{WriteState(folder,snapshot.State);}catch(const std::exception& failure){snapshot.Problems.push_back(std::string("Could not record pinned keys: ")+failure.what());}
    return snapshot;
}

inline bool UserAllows(const Json& state,std::string_view character){
    const auto guid=NormalizeGuid(character);const auto characters=state.find("characters");
    if(guid.empty()||characters==state.end()||!characters->is_object())return true;
    const auto row=characters->find(guid);
    return row==characters->end()||!row->is_object()||row->value("playerDeliveries",true);
}
inline std::string ModName(const Snapshot& snapshot,std::string_view id){
    for(const auto& mod:snapshot.Mods)if(mod.Id==id)return mod.Name;
    return std::string(id);
}
// The character exception a mod has in force for this character, if any.
inline const Flag* ExceptionFor(const Snapshot& snapshot,std::string_view mod,std::string_view character){
    const auto guid=NormalizeGuid(character);if(guid.empty())return nullptr;
    for(const auto& exception:snapshot.Excepted)if(exception.Mod==mod&&exception.Id==guid)return &exception;
    return nullptr;
}
// The reason a player-driven delivery to this character (and world, when known) must not run.
// exempt names one mod whose own pauses are skipped for this delivery: RuneSchema passes it only
// for a grant the mod's own plugin makes while serving that character's own server request
// (see PLUGIN-SERVER-REQUESTS.md). The player's switch and every other mod's pauses still apply.
inline std::optional<Block> Check(const Snapshot& snapshot,std::string_view character,std::string_view world,std::string_view exempt={}){
    const auto guid=NormalizeGuid(character),place=NormalizeGuid(world);
    if(!UserAllows(snapshot.State,guid))
        return Block{"helpy","Helpy","character",guid,"You turned off Helpy deliveries for this character in Settings > Deliveries.",true};
    for(const auto& flag:snapshot.Active){
        if(!exempt.empty()&&flag.Mod==exempt)continue;
        if(!((flag.Target=="character"&&flag.Id==guid&&!guid.empty())||(flag.Target=="world"&&flag.Id==place&&!place.empty())))continue;
        if(ExceptionFor(snapshot,flag.Mod,guid))continue; // That mod exempted this character from its own pauses.
        const auto name=ModName(snapshot,flag.Mod);
        return Block{flag.Mod,name,flag.Target,flag.Id,flag.Message.empty()?name+" has paused Helpy deliveries to this "+flag.Target+".":flag.Message,false};
    }
    return std::nullopt;
}
// True when a mod pauses this target, so a caller that cannot identify the character still
// knows whether refusing is necessary.
inline bool AnyCharacterPause(const Snapshot& snapshot){
    if(std::any_of(snapshot.Active.begin(),snapshot.Active.end(),[](const Flag& flag){return flag.Target=="character";}))return true;
    const auto characters=snapshot.State.find("characters");
    return characters!=snapshot.State.end()&&characters->is_object()&&std::any_of(characters->begin(),characters->end(),[](const Json& row){return row.is_object()&&!row.value("playerDeliveries",true);});
}

// Player choices. Each is recorded in helpy-state.json, where mods can read it.
inline void SetUserDeliveries(const fs::path& folder,std::string_view character,bool allowed,int64_t now,std::string_view name={}){
    const auto guid=NormalizeGuid(character);if(guid.empty())throw std::runtime_error("Choose a character with a valid GUID.");
    auto state=ReadState(folder);
    if(allowed)state["characters"].erase(guid);
    else state["characters"][guid]={{"playerDeliveries",false},{"changedAt",now},{"name",std::string(name).substr(0,80)}};
    Record(state,allowed?"user-deliveries-on":"user-deliveries-off",now,{{"character",guid}});WriteState(folder,std::move(state));
}
inline void SetModAllowed(const fs::path& folder,std::string_view mod,bool allowed,int64_t now){
    if(!ValidModId(mod))throw std::runtime_error("Unknown mod id.");
    auto state=ReadState(folder);auto& row=state["mods"][std::string(mod)];if(!row.is_object())row=Json::object();
    row["allowPause"]=allowed;row["allowPauseChangedAt"]=now;
    Record(state,allowed?"mod-pause-allowed":"mod-pause-declined",now,{{"mod",mod}});WriteState(folder,std::move(state));
}
// Forget a first-seen key so the mod's current key is pinned on the next read.
inline void ResetKey(const fs::path& folder,std::string_view mod,int64_t now){
    if(!ValidModId(mod))throw std::runtime_error("Unknown mod id.");
    auto state=ReadState(folder);auto& row=state["mods"][std::string(mod)];if(!row.is_object())row=Json::object();
    const auto previous=row.value("fingerprint",std::string{});row.erase("fingerprint");row.erase("pinnedAt");
    Record(state,"key-reset",now,{{"mod",mod},{"previous",previous}});WriteState(folder,std::move(state));
}
}
