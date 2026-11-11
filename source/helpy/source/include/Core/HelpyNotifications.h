#pragma once
#include "Core/RuneSchemaUpdateInfo.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace PS::HelpyNotifications {
using Json=nlohmann::json;
struct Mod {std::string Id,Name,Version;bool Enabled=true,Present=true;std::string Fingerprint;};
struct Notice {
    std::string Id,Kind,Source,Title,Detail,Version,Action;
    uint64_t Created=0;bool Read=false,Dismissed=false,Resolved=false,Active=true;
};
class Store {
    static bool Text(const std::string& value,size_t limit){return value.size()<=limit&&std::none_of(value.begin(),value.end(),[](unsigned char c){return c<32||c==127;});}
    void Put(Notice next,bool repeat=false){
        const auto clean=[](std::string& text,size_t limit){for(auto& c:text)if(static_cast<unsigned char>(c)<32||c==127)c=' ';if(text.size()>limit)text.resize(limit);};
        clean(next.Id,512);clean(next.Source,256);clean(next.Title,2048);clean(next.Detail,4096);clean(next.Version,256);
        auto found=std::find_if(Notices.begin(),Notices.end(),[&](const auto& n){return n.Id==next.Id;});
        if(found!=Notices.end()){if(repeat){*found=std::move(next);++Revision;}return;}
        if(Notices.size()>=256){auto oldest=std::min_element(Notices.begin(),Notices.end(),[](const auto& a,const auto& b){return a.Created<b.Created;});Notices.erase(oldest);}
        Notices.push_back(std::move(next));++Revision;
    }
public:
    bool ObserveRelease(const std::string& installed,const std::string& version,uint64_t now){
        using namespace RuneSchemaUpdateInfo;
        if(!RuneSchemaUpdateInfo::Parse(installed))return false;
        const bool newer=IsStableReleaseTag(version)&&Compare(installed,version)==Comparison::Newer;
        for(auto& notice:Notices)if(notice.Kind=="schema-update"&&notice.Active){
            const auto state=Compare(installed,notice.Version);
            if(state==Comparison::Current||state==Comparison::Ahead){
                notice.Resolved=true;notice.Active=false;++Revision;
            }else if(newer&&Compare(notice.Version,version)==Comparison::Newer){
                notice.Active=false;++Revision;
            }
        }
        const auto seen=Compare(LastReleaseNotice,version);
        if(!newer||seen==Comparison::Current||seen==Comparison::Ahead)return false;
        LastReleaseNotice=version;++Revision;
        Put({"release:"+version,"schema-update","RuneSchema","RuneSchema and Helpy "+version+" available",
             "Check the release notes and choose the download for your platform. Availability can vary by site.",version,"",now});
        return true;
    }
    void Hotfix(const std::string& identity,const std::string& version,uint64_t now){
        for(auto& notice:Notices)if(notice.Kind=="hotfix"&&notice.Active!=(notice.Id==identity)){notice.Active=notice.Id==identity;++Revision;}
        Put({identity,"hotfix","RuneSchema","Emergency hotfix available: "+version,"Download and review the affected RuneSchema files with the game closed, then apply and restart Helpy.",version,"hotfix",now});
    }
    void AppliedHotfix(const std::string& identity){for(auto& notice:Notices)if(notice.Id==identity&&!notice.Resolved){notice.Resolved=true;++Revision;}}
    std::string Game,LastReleaseNotice;
    std::map<std::string,std::string> Versions;
    std::map<std::string,std::string> Fingerprints;
    std::vector<Notice> Notices;
    uint64_t Revision=0;
    void Observe(const std::string& game,const std::vector<Mod>& mods,uint64_t now){
        if(!Text(game,256))throw std::runtime_error("Invalid game version in notifications.");
        std::map<std::string,std::string> active,activeFiles;
        for(const auto& mod:mods){
            if(!Text(mod.Id,256)||!Text(mod.Name,1024)||!Text(mod.Version,128)||mod.Id.empty())continue;
            if(!mod.Enabled||!mod.Present)continue;active[mod.Id]=mod.Version;
            if(mod.Fingerprint.size()==64&&mod.Fingerprint.find_first_not_of("0123456789abcdef")==mod.Fingerprint.npos){
                activeFiles[mod.Id]=mod.Fingerprint;const auto previous=Fingerprints.find(mod.Id);
                if(previous!=Fingerprints.end()&&previous->second!=mod.Fingerprint)Put({"files:"+mod.Id+":"+mod.Fingerprint,"mod-update",mod.Id,mod.Name+" files have changed","A locally tracked mod changed without needing an author-provided version. Keep using the current cache or rebuild it for the updated files.","files:"+mod.Fingerprint,"build-mod",now},true);
                if((previous!=Fingerprints.end()||Fingerprints.size()<2048)&&(previous==Fingerprints.end()||previous->second!=mod.Fingerprint)){Fingerprints[mod.Id]=mod.Fingerprint;++Revision;}
            }
            if(mod.Version.empty())continue;
            const auto previous=Versions.find(mod.Id);
            if(previous!=Versions.end()&&!previous->second.empty()&&previous->second!=mod.Version)
                Put({"mod:"+mod.Id+":"+mod.Version,"mod-update",mod.Id,mod.Name+" has changed",previous->second+" → "+mod.Version+". Existing cached content remains available; rebuild its cache to include changes.",mod.Version,"build-mod",now},true);
            if(previous==Versions.end()&&Versions.size()>=2048)continue;
            if(previous==Versions.end()||previous->second!=mod.Version){Versions[mod.Id]=mod.Version;++Revision;}
        }
        for(auto& notice:Notices)if(notice.Kind=="mod-update"){
            const bool enabled=active.contains(notice.Source)&&((!active[notice.Source].empty()&&active[notice.Source]==notice.Version)||(activeFiles.contains(notice.Source)&&notice.Version=="files:"+activeFiles[notice.Source]));
            if(notice.Active!=enabled){notice.Active=enabled;++Revision;}
        }
        if(!game.empty()&&game!=Game){
            if(!Game.empty())Put({"game:"+game,"game-update",{},"Dragonwilds has changed",Game+" → "+game+". Your vanilla cache is still available. Resync it to pick up game changes.",game,"resync-vanilla",now},true);
            Game=game;++Revision;
        }
        for(auto& notice:Notices)if(notice.Kind=="game-update"&&notice.Active!=(notice.Version==Game)){notice.Active=notice.Version==Game;++Revision;}
    }
    void Built(const std::string& source,const std::string& version,bool vanilla){
        for(auto& n:Notices)if(n.Kind==(vanilla?"game-update":"mod-update")&&(vanilla||n.Source==source)&&n.Version==version&&!n.Resolved){n.Resolved=true;n.Read=true;++Revision;}
    }
    void Result(const std::string& job,const std::string& source,const std::string& name,bool success,const std::string& detail,uint64_t now){
        Put({"build:"+job,success?"cache-ready":"cache-failed",source,success?name+" cache is ready":name+" cache needs attention",detail,{},success?"":source.empty()?"resync-vanilla":"build-mod",now});
    }
    void Read(const std::string& id){for(auto& n:Notices)if(n.Id==id&&!n.Read){n.Read=true;++Revision;}}
    void ReadAll(){for(auto& n:Notices)if(!n.Read){n.Read=true;++Revision;}}
    void DeleteAll(){if(!Notices.empty()){Notices.clear();++Revision;}}
    void Dismiss(const std::string& id){for(auto& n:Notices)if(n.Id==id&&!n.Dismissed){n.Dismissed=true;n.Read=true;++Revision;}}
    size_t Unread()const{return std::count_if(Notices.begin(),Notices.end(),[](const auto& n){return n.Active&&!n.Read&&!n.Dismissed&&!n.Resolved;});}
    bool VanillaPending()const{return std::any_of(Notices.begin(),Notices.end(),[](const auto& n){return n.Kind=="game-update"&&n.Active&&!n.Resolved&&!n.Dismissed;});}
    Json Document()const{
        auto entries=Json::array();for(const auto& n:Notices)entries.push_back({{"Id",n.Id},{"Kind",n.Kind},{"Source",n.Source},{"Title",n.Title},{"Detail",n.Detail},{"Version",n.Version},{"Action",n.Action},{"Created",n.Created},{"Read",n.Read},{"Dismissed",n.Dismissed},{"Resolved",n.Resolved},{"Active",n.Active}});
        return {{"Format",1},{"Game",Game},{"LastReleaseNotice",LastReleaseNotice},{"Versions",Versions},{"Fingerprints",Fingerprints},{"Notices",std::move(entries)}};
    }
    static Store Parse(const Json& doc){
        if(!doc.is_object()||!doc.at("Versions").is_object()||doc["Versions"].size()>2048||!doc.at("Notices").is_array()||doc["Notices"].size()>256)throw std::runtime_error("Notification history has an invalid shape.");
        Store result;result.Game=doc.value("Game",std::string{});if(!Text(result.Game,256))throw std::runtime_error("Invalid notification game version.");
        result.LastReleaseNotice=doc.value("LastReleaseNotice",std::string{});
        if(!RuneSchemaUpdateInfo::IsStableReleaseTag(result.LastReleaseNotice))result.LastReleaseNotice.clear();
        for(const auto& [id,version]:doc["Versions"].items()){if(!Text(id,256)||!version.is_string()||!Text(version.get_ref<const std::string&>(),128))throw std::runtime_error("Invalid notification mod version.");result.Versions[id]=version.get<std::string>();}
        if(doc.contains("Fingerprints")){const auto& fingerprints=doc["Fingerprints"];if(!fingerprints.is_object()||fingerprints.size()>2048)throw std::runtime_error("Invalid local mod fingerprints.");
            for(const auto& [id,value]:fingerprints.items()){if(!Text(id,256)||!value.is_string())throw std::runtime_error("Invalid local mod fingerprint.");const auto hash=value.get<std::string>();if(hash.size()!=64||hash.find_first_not_of("0123456789abcdef")!=hash.npos)throw std::runtime_error("Invalid local mod fingerprint.");result.Fingerprints[id]=hash;}}
        std::set<std::string> ids;
        for(const auto& value:doc["Notices"]){
            Notice n{value.at("Id").get<std::string>(),value.at("Kind").get<std::string>(),value.value("Source",std::string{}),value.at("Title").get<std::string>(),value.value("Detail",std::string{}),value.value("Version",std::string{}),value.value("Action",std::string{}),value.value("Created",uint64_t{}),value.value("Read",false),value.value("Dismissed",false),value.value("Resolved",false),value.value("Active",true)};
            if(n.Id.empty()||!ids.insert(n.Id).second||!Text(n.Id,512)||!Text(n.Kind,32)||!Text(n.Source,256)||!Text(n.Title,2048)||!Text(n.Detail,4096)||!Text(n.Version,256)||!std::set<std::string>{"","build-mod","resync-vanilla","hotfix"}.contains(n.Action))throw std::runtime_error("Invalid notification entry.");
            result.Notices.push_back(std::move(n));
        }return result;
    }
};
}
