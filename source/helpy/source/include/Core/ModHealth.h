#pragma once
#include "Core/LiveLog.h"
namespace PS::ModHealth {
inline bool Attributed(std::string_view name,const LiveLog::Entry& row){
    const auto owner=LiveLog::Lower(name);if(owner.empty())return false;
    const auto& text=row.Search;
    // An explicit registrar owner wins over paths mentioned as dependencies.
    for(const auto& [begin,end]:{std::pair{"[mod:","]"},std::pair{"owner='","'"},std::pair{"in mod '","'"}}){const auto at=text.find(begin);if(at!=text.npos){const auto first=at+std::string_view(begin).size(),last=text.find(end,first);if(last!=text.npos)return text.substr(first,last-first)==owner;}}
    for(const auto& pattern:{"[mod:"+owner+"]","["+owner+"]","mods/"+owner+"/","mods\\"+owner+"\\","/"+owner+"/"})
        if(text.find(pattern)!=text.npos)return true;
    return false;
}
struct State {size_t Errors=0,Warnings=0,OptionalProbes=0,AcceptedDeclarations=0,RejectedDeclarations=0;std::vector<std::string> Details;};
inline bool OptionalProbe(std::string_view owner,const LiveLog::Entry& row,std::string_view stream){
    if(stream!="Game"||row.Severity!=LiveLog::Level::Warning)return false;
    const auto name=LiveLog::Lower(owner),asset="da_runeschemaregistry_"+name;
    const auto path="/game/mods/"+name+"/registry/"+asset;
    return row.Search.find(path)!=row.Search.npos
        &&(row.Search.find("loadpackage: skippackage:")!=row.Search.npos||row.Search.find("failed to find object 'object ")!=row.Search.npos);
}
inline void Observe(State& state,std::string_view owner,const LiveLog::Entry& row,std::string_view stream){
    if((row.Severity!=LiveLog::Level::Error&&row.Severity!=LiveLog::Level::Warning)||!Attributed(owner,row))return;
    // This optional convention is not required by JSON or nonconventional PAK declarations.
    // Registrar rejection remains an error; retain the raw probe in Live Log.
    if(OptionalProbe(owner,row,stream)){++state.OptionalProbes;return;}
    if(row.Severity==LiveLog::Level::Error)++state.Errors;else ++state.Warnings;
    if(state.Details.size()<24)state.Details.push_back(std::string(stream)+": "+row.Text.substr(0,2048));
}
inline const char* Label(const State& state,bool checked){return state.Errors||state.RejectedDeclarations?"Errors observed":state.Warnings?"Warnings observed":checked?"No errors observed":"Not checked / incomplete logs";}
}
