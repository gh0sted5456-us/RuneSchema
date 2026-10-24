#pragma once
#include "Core/LiveLog.h"
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace PS::DiagnosticToastPatterns {
inline std::string_view Trim(std::string_view text) {
    while (!text.empty() && (text.front()==' ' || text.front()=='\t' || text.front()=='\r')) text.remove_prefix(1);
    while (!text.empty() && (text.back()==' ' || text.back()=='\t' || text.back()=='\r')) text.remove_suffix(1);
    return text;
}
inline std::string_view Body(std::string_view text) {
    const auto tag=text.find("[RuneSchema]");
    if(tag!=text.npos) text.remove_prefix(tag+sizeof("[RuneSchema]")-1);
    text=Trim(text);
    for(int i=0;i<4 && text.starts_with('[');++i) {
        const auto end=text.find(']');
        if(end==text.npos)break;
        const auto token=LiveLog::Lower(text.substr(1,end-1));
        if(token!="summary" && token!="info" && token!="warning" && token!="error" && token!="diagnostic" && token!="debug" && token!="verbose" && token!="fatal")break;
        text=Trim(text.substr(end+1));
    }
    return text;
}
inline bool Number(std::string_view value,std::uint32_t& out) {
    value=Trim(value);
    const auto* begin=value.data();
    auto result=std::from_chars(begin,begin+value.size(),out);
    return result.ec==std::errc{} && result.ptr==begin+value.size();
}
inline bool Counts(std::string_view line,const std::vector<std::string_view>& keys,std::vector<std::uint32_t>& values) {
    values.clear();
    for(const auto key:keys) {
        line=Trim(line);
        if(!line.starts_with(key))return false;
        line.remove_prefix(key.size());
        const auto sep=line.find('|');
        std::uint32_t count=0;
        if(!Number(line.substr(0,sep),count))return false;
        values.push_back(count);
        if(sep==line.npos){line={};continue;}
        line.remove_prefix(sep+1);
    }
    return Trim(line).empty();
}
struct BlueprintSummary {std::uint32_t Loaded=0,Created=0,Updated=0,Placed=0,Errors=0;};
inline bool ParseBlueprintSummary(std::string_view text,BlueprintSummary& summary){
    auto body=Body(text);
    constexpr std::string_view head="blueprints | ";
    if(!body.starts_with(head))return false;
    body.remove_prefix(head.size());
    std::vector<std::uint32_t> values;
    if(!Counts(body,{"loaded=","created=","updated=","placed=","errors="},values))return false;
    summary={values[0],values[1],values[2],values[3],values[4]};return true;
}
inline bool ParseBlueprintOmitted(std::string_view text,std::uint32_t& count){
    auto body=Body(text);
    constexpr std::string_view prefix="Blueprints: ";
    constexpr std::string_view suffix=" additional successful change detail(s) omitted.";
    if(!body.starts_with(prefix)||!body.ends_with(suffix))return false;
    body.remove_prefix(prefix.size());body.remove_suffix(suffix.size());
    return Number(body,count);
}
struct ImportCounts {std::uint32_t Requested=0,Resolved=0,Planned=0,Reused=0,Errors=0;bool operator==(const ImportCounts&)const=default;};
inline bool ParseImportStatus(std::string_view text,bool& ready,ImportCounts& report){
    auto body=Body(text);
    constexpr std::string_view ok="READY | ",blocked="BLOCKED | ";
    if(body.starts_with(ok)){ready=true;body.remove_prefix(ok.size());}
    else if(body.starts_with(blocked)){ready=false;body.remove_prefix(blocked.size());}
    else return false;
    std::vector<std::uint32_t> values;
    if(!Counts(body,{"requested ","resolved ","planned ","reused ","errors "},values))return false;
    report={values[0],values[1],values[2],values[3],values[4]};return true;
}
inline bool ParseImportTotals(std::string_view text,ImportCounts& report){
    auto body=Body(text);
    std::vector<std::uint32_t> values;
    if(!Counts(body,{"requested ","resolved ","planned ","reused ","errors "},values))return false;
    report={values[0],values[1],values[2],values[3],values[4]};return true;
}
inline bool ParseImportRequested(std::string_view text,std::string& modules){
    const auto body=Body(text);
    if(!body.starts_with("Requested:")&&!body.starts_with("Requested :"))return false;
    const auto colon=body.find(':');
    const auto names=Trim(body.substr(colon+1));
    if(names.empty()||names.size()>1024)return false;
    modules=std::string(names);return true;
}
inline bool IsImportContinuation(std::string_view raw){
    // Continuation lines are emitted by one multi-line $ImpDry report. Never
    // absorb a new RuneSchema envelope, which may belong to another operation.
    if(raw.find("[RuneSchema]")!=raw.npos)return false;
    if(raw.empty()||raw=="\r")return true;
    if(raw.front()==' '||raw.front()=='\t')return true;
    constexpr std::string_view headings[]={"IMPORT MAP","Dependencies","Required","Optional","Plan","Portability:","Structural hidden:","Conflict","Conflicts","Verification"};
    for(const auto heading:headings)if(Trim(raw).starts_with(heading))return true;
    return false;
}
}
