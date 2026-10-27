#pragma once
#include "Core/ProcessExitFuture.h"
#include "Core/StructuredDiagnosticReader.h"
#include "Runtime/HostServices.h"
#include <chrono>
#include <future>
#include <functional>
#include <string>
#include <cstdio>

namespace PS::StructuredLogFeed {
inline StructuredDiagnosticReader::Cursor Cursor;
inline PS::ProcessExitFuture<StructuredDiagnosticReader::Batch> Worker;
inline std::chrono::steady_clock::time_point NextRead{};
inline std::function<void(const StructuredDiagnostic::Record&)> OnRecord;
inline std::string Status;
inline std::uint64_t Invalid=0;
inline std::uint64_t Gaps=0;
// RuneSchema and HELPY occupy the same game process. Old records in the
// append-only active JSONL file must not become new current-session issues.
inline bool InThisProcess(const StructuredDiagnostic::Record& entry){
#ifdef _WIN32
    static const std::string processStart=[]{
        FILETIME start{},exit{},kernel{},user{};SYSTEMTIME utc{};
        if(!GetProcessTimes(GetCurrentProcess(),&start,&exit,&kernel,&user)||!FileTimeToSystemTime(&start,&utc))return std::string{};
        char time[32]{};std::snprintf(time,sizeof(time),"%04u-%02u-%02uT%02u:%02u:%02u",
             static_cast<unsigned>(utc.wYear),static_cast<unsigned>(utc.wMonth),static_cast<unsigned>(utc.wDay),
             static_cast<unsigned>(utc.wHour),static_cast<unsigned>(utc.wMinute),static_cast<unsigned>(utc.wSecond));
        return std::string(time);
    }();
    return processStart.empty()||entry.Timestamp.substr(0,19)>=processStart;
#else
    (void)entry;return true;
#endif
}
inline void ConsumeBatch(StructuredDiagnosticReader::Batch batch){
    Cursor=std::move(batch.Next);
    if(batch.Gap)++Gaps;
    Invalid+=batch.Invalid;
    if(!batch.Error.empty())Status=std::move(batch.Error);
    else if(batch.Gap)Status="A rotated diagnostic log was missed; retained history may be incomplete.";
    else Status.clear();
    for(const auto& entry:batch.Records)if(OnRecord&&InThisProcess(entry))OnRecord(entry);
}
inline void DrainPending(){
    if(!Worker.valid())return;
    try{ConsumeBatch(Worker.get());}
    catch(...){Status="Failed to process structured diagnostic log batch.";}
}
inline void Poll(bool background=false){
    if(Worker.valid()&&Worker.wait_for(std::chrono::seconds(0))==std::future_status::ready)
        DrainPending();
    if(!Worker.valid()&&std::chrono::steady_clock::now()>=NextRead){
        NextRead=std::chrono::steady_clock::now()+std::chrono::milliseconds(background?1000:250);
        const auto path=HostServices::ModDirectory()/L"RSDiagLog.jsonl";const auto cursor=Cursor;
        try{Worker=std::async(std::launch::async,[path,cursor]{return StructuredDiagnosticReader::Read(path,cursor);});}
        catch(...){Status="Could not start the structured diagnostic reader.";}
    }
}
inline void Shutdown(){DrainPending();OnRecord={};}
}
