#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace PS::ServerModVersions {
enum class State {Match,Different,Missing,Disabled,Unknown,Ambiguous};
enum class Tone {Neutral,Aligned,Differences};
struct Result {State Status=State::Unknown;std::string Version;bool Excluded=false;};
struct Summary {
    bool Ready=false;size_t Matched=0,Different=0,Missing=0,Disabled=0,Ambiguous=0,Unversioned=0,ClientOnly=0;
    Tone Alignment()const{return !Ready?Tone::Neutral:Different||Missing||Disabled||Ambiguous?Tone::Differences:Matched?Tone::Aligned:Tone::Neutral;}
    bool operator==(const Summary&)const=default;
};
struct Report {Summary Counts;std::vector<Result> Host;std::vector<size_t> ClientOnly;};
inline std::string Fold(std::string value){for(auto& c:value)if(c>='A'&&c<='Z')c+=32;return value;}
inline std::string Version(std::string value){const auto first=value.find_first_not_of(" \t\r\n");return first==value.npos?std::string{}:value.substr(first,value.find_last_not_of(" \t\r\n")-first+1);}
inline Report Build(const nlohmann::json& server,const nlohmann::json& installed,bool ready){
    using Index=std::map<std::string,std::vector<size_t>>;Index folders,identities;std::set<size_t> shared;
    if(ready)for(size_t i=0;i<installed.size();++i){const auto& row=installed[i];if(!row.value("Present",false))continue;
        folders[Fold(row.value("Name",std::string{}))].push_back(i);
        const auto id=row.value("Metadata",nlohmann::json::object()).value("modId",std::string{});
        if(!id.empty())identities[Fold(id)].push_back(i);
    }
    Report report;report.Counts.Ready=ready;report.Host.reserve(server.size());
    for(const auto& remote:server){Result match;const auto version=Version(remote.value("Version",std::string{}));match.Excluded=version.empty();
        if(ready){
            const auto id=Fold(remote.value("ModId",remote.value("Id",std::string{})));
            const auto canonical=identities.find(id),folder=folders.find(id);
            const auto* candidates=canonical!=identities.end()?&canonical->second:!remote.contains("ModId")&&folder!=folders.end()?&folder->second:nullptr;
            if(!candidates)match.Status=State::Missing;
            else{shared.insert(candidates->begin(),candidates->end());
                match.Excluded|=std::all_of(candidates->begin(),candidates->end(),[&](size_t i){return Version(installed[i].value("Metadata",nlohmann::json::object()).value("version",std::string{})).empty();});
                if(candidates->size()!=1)match.Status=State::Ambiguous;
                else{const auto& row=installed[candidates->front()];match.Version=Version(row.value("Metadata",nlohmann::json::object()).value("version",std::string{}));
                    match.Excluded|=match.Version.empty();
                    match.Status=!row.value("Enabled",false)?State::Disabled:match.Excluded?State::Unknown:match.Version==version?State::Match:State::Different;
                }
            }
            if(match.Excluded)++report.Counts.Unversioned;
            else switch(match.Status){case State::Match:++report.Counts.Matched;break;case State::Different:++report.Counts.Different;break;case State::Missing:++report.Counts.Missing;break;case State::Disabled:++report.Counts.Disabled;break;case State::Ambiguous:++report.Counts.Ambiguous;break;default:break;}
        }report.Host.push_back(std::move(match));
    }
    if(ready)for(size_t i=0;i<installed.size();++i)if(installed[i].value("Present",false)&&!shared.contains(i))report.ClientOnly.push_back(i);
    report.Counts.ClientOnly=report.ClientOnly.size();return report;
}
inline std::vector<Result> Compare(const nlohmann::json& server,const nlohmann::json& installed,bool ready){return Build(server,installed,ready).Host;}
inline const char* Label(State state){switch(state){case State::Match:return "Match";case State::Different:return "Different";case State::Missing:return "Missing";case State::Disabled:return "Disabled";case State::Ambiguous:return "Copies";default:return "Unknown";}}
inline std::string Text(const nlohmann::json& server,const nlohmann::json& installed,const Report& report){
    const auto& n=report.Counts;std::string text="RuneSchema client / host mod report\n";
    if(!n.Ready)return text+"Installed client metadata is not ready.\n";
    text+="Matched: "+std::to_string(n.Matched)+" | Different: "+std::to_string(n.Different)+" | Missing: "+std::to_string(n.Missing)+" | Disabled: "+std::to_string(n.Disabled)+" | Multiple copies: "+std::to_string(n.Ambiguous)+"\nUnversioned host/shared entries excluded: "+std::to_string(n.Unversioned)+" | Client-only: "+std::to_string(n.ClientOnly)+"\nVersion labels only; registry compatibility is checked separately.\n\nHost mods\n";
    for(size_t i=0;i<server.size();++i){const auto& row=server[i];const auto& match=report.Host.at(i);const auto version=Version(row.value("Version",std::string{}));
        text+=row.value("Name",row.value("Id",std::string{}))+" ["+row.value("ModId",row.value("Id",std::string{}))+"] | Host: "+(version.empty()?"Unversioned":version)+" | Client: "+(match.Status==State::Missing?"Missing":match.Status==State::Ambiguous?"Multiple copies":match.Version.empty()?"Unversioned":match.Version)+" | "+Label(match.Status)+(match.Excluded?" | Unversioned - excluded":"")+"\n";}
    text+="\nClient-only mods (excluded from alignment)\n";
    for(const auto i:report.ClientOnly){const auto& row=installed.at(i);const auto metadata=row.value("Metadata",nlohmann::json::object());const auto version=Version(metadata.value("version",std::string{}));
        text+=metadata.value("name",row.value("Name",std::string{}))+" ["+metadata.value("modId",row.value("Name",std::string{}))+"] | "+(version.empty()?"Unversioned - excluded":version)+" | "+(row.value("Enabled",false)?"Enabled":"Disabled")+"\n";}
    return text;
}
}
