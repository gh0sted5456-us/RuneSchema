#include "Utility/StructuredDiagnostics.h"
#include "Utility/DiagnosticLog.h"
#include "Core/StructuredDiagnosticLegacyMirror.h"
#include "Core/StructuredDiagnosticFile.h"
#include "Runtime/HostServices.h"
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace PS::StructuredDiagnostics {
namespace {
StructuredDiagnostic::File& Sink(){static StructuredDiagnostic::File sink;return sink;}
std::once_flag Start;
std::atomic<bool> Ready{false};
std::atomic<std::uint64_t> Sequence{0};
std::string Session;
void MirrorCommitted(const StructuredDiagnostic::Record& record) noexcept {
    try {
        const auto summary=StructuredDiagnosticLegacyMirror::Format(record);
        if(!summary.empty())DiagnosticLog::WriteUtf8(summary,true);
    }catch(...){}
}
std::string UtcNow() {
    const auto now=std::chrono::system_clock::now();
    const auto seconds=std::chrono::system_clock::to_time_t(now);
    std::tm utc{};gmtime_s(&utc,&seconds);
    std::ostringstream out;
    out<<std::put_time(&utc,"%Y-%m-%dT%H:%M:%S")<<'.'
       <<std::setfill('0')<<std::setw(3)
       <<std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()%1000<<'Z';
    return out.str();
}
}
void Initialize() noexcept {
    try{std::call_once(Start,[]{
        const auto tick=std::chrono::steady_clock::now().time_since_epoch().count();
        Session="s-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+
                "-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(tick);
        Sink().OnWritten(&MirrorCommitted);
        Ready=Sink().Open(HostServices::ModDirectory()/L"RSDiagLog.jsonl");
    });}catch(...){Ready=false;}
}
bool Emit(StructuredDiagnostic::Record record) noexcept {
    if(!Ready.load(std::memory_order_acquire))return false;
    try{
        record.SessionId=Session;
        record.EventId=Session+"-e"+std::to_string(++Sequence);
        record.Timestamp=UtcNow();
        return Sink().Emit(std::move(record));
    }catch(...){return false;}
}
Health Counters() noexcept{return {Sink().Written(),Sink().Dropped(),Sink().Errors()};}
void Shutdown() noexcept {Ready=false;Sink().Close();}
}
