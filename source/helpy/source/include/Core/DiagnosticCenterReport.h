#pragma once

#include "Core/DiagnosticCenterModel.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>

// HELPY-local, read-only report formatter. Never queries game objects or reads
// RuneSchema.log. An export is only as complete as the selected bounded store.
namespace PS::DiagnosticCenterReport {
inline bool IsReportDestination(const std::filesystem::path& path){
    const auto utf8=path.filename().u8string();
    const auto name=LiveLog::Lower(std::string(utf8.begin(),utf8.end()));
    return name.ends_with(".txt")&&name!="mods.txt"&&name!="enabled.txt";
}
struct Options {
    bool IncludeEvidence=true;
    bool RedactUserPaths=true;
};
inline std::string Date(DiagnosticCenterModel::Clock::time_point time){
    const auto stamp=DiagnosticCenterModel::Clock::to_time_t(time);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local,&stamp);
#else
    localtime_r(&stamp,&local);
#endif
    char buf[64]{};
    if(!std::strftime(buf,sizeof(buf),"%Y-%m-%d %H:%M:%S (local)",&local))return "unknown";
    return buf;
}
inline std::string Sanitize(std::string_view value,bool redact){
    if(!redact)return std::string(value);
    std::string text(value);
    // Mask the account name in Windows user-home paths without erasing the
    // mod/asset filename that a support volunteer actually needs to diagnose.
    // This is best-effort redaction, not a claim of complete anonymization.
    std::size_t begin=0;
    while(begin<text.size()){
        const auto lower=LiveLog::Lower(text);
        const auto position=lower.find(":\\users\\",begin);
        const auto alternate=lower.find(":/users/",begin);
        const auto found=(position==std::string::npos?alternate:
                         alternate==std::string::npos?position:std::min(position,alternate));
        if(found==std::string::npos)break;
        const auto nameStart=found+8;
        const auto nameEnd=text.find_first_of("\\/\r\n\t\"' <>|",nameStart);
        if(nameStart>=text.size()||nameEnd==nameStart){begin=nameStart;continue;}
        const auto end=nameEnd==std::string::npos?text.size():nameEnd;
        text.replace(nameStart,end-nameStart,"<user>");
        // Re-evaluate after replacement rather than using stale search offsets.
        begin=nameStart+6;
    }
    return text;
}
inline void WriteHeader(std::ostringstream& out,std::string_view kind,std::string_view session,
                        const DiagnosticCenterModel::Store& store,const Options& options){
    out<<"RuneSchema / HELPY 0.8 - "<<kind<<"\n";
    out<<"Session: "<<session<<"\n";
    out<<"Generated: "<<Date(DiagnosticCenterModel::Clock::now())<<"\n";
    out<<"Retained events: "<<store.Events().size()<<"; earlier dropped: "<<store.Dropped()<<"\n";
    out<<"Evidence: "<<(options.IncludeEvidence?"included":"omitted by user")<<"\n";
    out<<"Windows user-home paths: "<<(options.RedactUserPaths?"best-effort redacted":"not redacted")<<"\n";
    out<<"PRIVACY: Inspect this report before sharing. Log content can contain private paths, identifiers or player data.\n";
    out<<"LIMIT: This is bounded captured evidence, not a complete RuneSchema.log or proof of current health.\n";
    out<<"----------------------------------------------------------------------\n";
}
inline std::string Issue(const DiagnosticCenterModel::Store& store,const DiagnosticCenterModel::Group& group,
                         std::string_view session,Options options={}){
    std::ostringstream out;
    out<<"RuneSchema / HELPY — Issue handoff\n";
    out<<"Session: "<<Sanitize(session,options.RedactUserPaths)<<"\n";
    if(!group.Code.empty())out<<"Diagnostic code: "<<group.Code<<
        (group.Key.starts_with("structured:")?" (emitted in structured report)\n":" (emitted in legacy log)\n");
    else {
        out<<"Diagnostic code: none emitted (legacy log message)\n";
        if(group.Help>=0 && group.Help<static_cast<int>(LiveLog::Guides.size())){
            const auto code=LiveLog::Guides[group.Help].Code;
            if(code!="RS-LOG-000" && code!="RS-GAME-000")
                out<<"Suggested reference (not emitted): "<<code<<"\n";
        }
    }
    if(!group.Key.starts_with("structured:"))out<<"Evidence source: RuneSchema.log (legacy, not JSONL)\n";
    out<<"Severity: "<<LiveLog::Label(group.Severity)<<"\n";
    if(!group.Source.empty())out<<"Source: "<<Sanitize(group.Source,options.RedactUserPaths)<<"\n";
    out<<"Failure: "<<Sanitize(group.Title,options.RedactUserPaths)<<"\n";
    if(!group.Message.empty()&&group.Message!=group.Title)
        out<<"Reported detail: "<<Sanitize(group.Message,options.RedactUserPaths)<<"\n";
    if(!group.ModId.empty())out<<"Mod: "<<Sanitize(group.ModId,options.RedactUserPaths)<<"\n";
    if(!group.File.empty())out<<"File: "<<Sanitize(group.File,options.RedactUserPaths)<<"\n";
    if(!group.Target.empty())out<<"Target: "<<Sanitize(group.Target,options.RedactUserPaths)<<"\n";
    if(!group.Property.empty())out<<"Property: "<<Sanitize(group.Property,options.RedactUserPaths)<<"\n";
    if(!group.Outcome.empty())out<<"Outcome: "<<group.Outcome<<"\n";
    if(!group.Phase.empty())out<<"Phase: "<<group.Phase<<"\n";
    if(!group.CauseStatus.empty())out<<"Cause confidence: "<<group.CauseStatus<<"\n";
    if(!group.CauseDescription.empty())out<<"Reported cause: "<<Sanitize(group.CauseDescription,options.RedactUserPaths)<<"\n";
    out<<"Occurrences: "<<group.Count<<"\n";
    out<<"First / latest: "<<Date(group.FirstSeen)<<" / "<<Date(group.LastSeen)<<"\n";
    if(options.IncludeEvidence){
        out<<"\nRetained issue evidence (up to 8 recent samples):\n";
        if(group.Count>group.EvidenceIds.size())out<<"NOTE: Earlier occurrences are not retained.\n";
        const std::size_t start=group.EvidenceIds.size()>8?group.EvidenceIds.size()-8:0;
        for(std::size_t i=start;i<group.EvidenceIds.size();++i){
            if(const auto* event=store.FindEvent(group.EvidenceIds[i]))
                out<<"["<<Date(event->Timestamp)<<"] "<<Sanitize(event->Text,options.RedactUserPaths)<<"\n";
        }
    }else out<<"\nOriginal raw evidence excluded; enable it only when needed.\n";
    out<<"Privacy: review paths, identifiers and names before sharing; redaction is best-effort.\n";
    return out.str();
}
inline std::string Session(const DiagnosticCenterModel::Store& store,std::string_view session,Options options={}){
    std::ostringstream out;
    WriteHeader(out,"Session report",session,store,options);
    std::size_t errors=0,warnings=0,other=0;
    for(const auto& group:store.Groups()){
        if(group.Severity==LiveLog::Level::Error)++errors;
        else if(group.Severity==LiveLog::Level::Warning)++warnings;
        else ++other;
    }
    out<<"Groups: "<<errors<<" error, "<<warnings<<" warning, "<<other<<" other\n\n";
    out<<"ISSUE SUMMARY\n";
    for(const auto& group:store.Groups()){
        out<<"["<<LiveLog::Label(group.Severity)<<"] ("<<group.Count<<") "
           <<Sanitize(group.Title,options.RedactUserPaths)<<"\n";
        if(!group.Source.empty())out<<"  source: "<<Sanitize(group.Source,options.RedactUserPaths)<<"\n";
    }
    if(options.IncludeEvidence){
        out<<"\nCHRONOLOGICAL RETAINED EVIDENCE ("<<store.Events().size()<<" entries)\n";
        for(const auto& event:store.Events()){
            out<<"["<<Date(event.Timestamp)<<"] ["<<LiveLog::Label(event.Severity)<<"] "
               <<Sanitize(event.Text,options.RedactUserPaths)<<"\n";
        }
    }
    return out.str();
}
} // namespace PS::DiagnosticCenterReport
