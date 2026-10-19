#include "Utility/DiagnosticLog.h"
#include "Core/DiagnosticLogFile.h"
#include "Core/SystemDiagnostics.h"
#include "Runtime/ExecutableVersion.h"
#include "Runtime/HostServices.h"
#include <DynamicOutput/DynamicOutput.hpp>
#include <Windows.h>
#include <atomic>
#include <limits>

namespace PS::DiagnosticLog {
namespace {
File& Sink(){static File file;return file;}
std::once_flag Start;
std::atomic<bool> Ready=false,Warned=false;
void ReportFailure() noexcept {
    if(Warned.exchange(true))return;
    try {RC::Output::send<RC::LogLevel::Warning>(STR("[RuneSchema] [warning] [RS-LOG-001] Cannot start RuneSchema.log; check mod-root/logs folder permissions and free space. A failed archive leaves the previous log untouched. File logging disabled; UE4SS summaries, warnings and errors remain available.\n"));}catch(...){}
}
}
void Initialize(bool archiveLogs,bool freshSession) noexcept {
    try {
        std::call_once(Start,[=]{
            const auto mode=archiveLogs?Session::ArchivePrevious:freshSession?Session::Fresh:Session::Append;
            if(!Sink().Open(HostServices::ModDirectory()/"RuneSchema.log",mode)){ReportFailure();return;}
            Ready=true;
            if(!Sink().Write("[RuneSchema] ========== Startup diagnostics ==========",true,"system"))ReportFailure();
            for(const auto& row:SystemDiagnostics::Startup(HostServices::ModDirectory(),HostServices::CacheDirectory()))if(!Sink().Write("[RuneSchema] "+row,true,"system"))ReportFailure();
            Sink().Write("[RuneSchema] Game executable version: "+ExecutableVersion::Current(),true,"system");
            Sink().Write("[RuneSchema] ========== Runtime events ==========",true,"system");
            if(!Sink().Write(archiveLogs?"[RuneSchema] Session started. Previous nonempty log archived in logs/ (UTC filenames). RuneSchema.log continuously records this run.":"[RuneSchema] Session started. RuneSchema.log continuously records this run; enable archiveLogs to keep previous runs.",true))ReportFailure();
        });
    }catch(...){ReportFailure();}
}
void Write(std::wstring_view text,bool urgent) noexcept {
    if(!Ready.load())return;
    try {
        if(text.size()>static_cast<size_t>((std::numeric_limits<int>::max)()))return;
        const auto size=WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
        if(!size&& !text.empty())return;
        std::string utf8(size,'\0');
        if(size)WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),utf8.data(),size,nullptr,nullptr);
        if(!Sink().Write(utf8,urgent)&&Sink().TakeFailure())ReportFailure();
    }catch(...){};
}
void Shutdown() noexcept {
    if(Ready.exchange(false))Sink().Write("[RuneSchema] Session ended.",true);
    Sink().Close();
}
}
