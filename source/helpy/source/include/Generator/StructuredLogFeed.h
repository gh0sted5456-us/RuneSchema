#pragma once
#include "Core/ProcessExitFuture.h"
#include "Core/StructuredDiagnosticReader.h"
#include "Core/DiagnosticLogPaths.h"
#include "Runtime/HostServices.h"
#include <chrono>
#include <future>
#include <functional>
#include <string>

namespace PS::StructuredLogFeed {
inline StructuredDiagnosticReader::Cursor Cursor;
inline PS::ProcessExitFuture<StructuredDiagnosticReader::Batch> Worker;
inline std::chrono::steady_clock::time_point NextRead{};
inline std::function<void(const StructuredDiagnostic::Record&)> OnRecord;
inline std::string Status;
inline std::uint64_t Invalid=0;
inline std::uint64_t Gaps=0;
inline bool RestartPending=false;
inline void Restart(){
    RestartPending=Worker.valid();Cursor={};NextRead={};
    Status.clear();Invalid=0;Gaps=0;
}
inline void ConsumeBatch(StructuredDiagnosticReader::Batch batch){
    Cursor=std::move(batch.Next);
    if(batch.Gap)++Gaps;
    Invalid+=batch.Invalid;
    if(!batch.Error.empty())Status=std::move(batch.Error);
    else if(batch.Gap)Status="A rotated diagnostic log was missed; retained history may be incomplete.";
    else Status.clear();
    if(batch.More)NextRead={};
    for(const auto& entry:batch.Records)if(OnRecord)OnRecord(entry);
}
inline void DrainPending(){
    if(!Worker.valid())return;
    const bool discard=std::exchange(RestartPending,false);
    try{auto batch=Worker.get();if(!discard)ConsumeBatch(std::move(batch));}
    catch(...){if(!discard)Status="Failed to process structured diagnostic log batch."; }
}
inline void Poll(bool background=false){
    if(Worker.valid()&&Worker.wait_for(std::chrono::seconds(0))==std::future_status::ready)
        DrainPending();
    if(!Worker.valid()&&std::chrono::steady_clock::now()>=NextRead){
        NextRead=std::chrono::steady_clock::now()+std::chrono::milliseconds(background?1000:250);
        const auto path=DiagnosticLogPaths::Directory(HostServices::ModDirectory())/L"RSDiagLog.jsonl";const auto cursor=Cursor;
        try{Worker=std::async(std::launch::async,[path,cursor]{return StructuredDiagnosticReader::Read(path,cursor);});}
        catch(...){Status="Could not start the structured diagnostic reader.";}
    }
}
inline void Shutdown(){DrainPending();OnRecord={};}
}
