#include "Utility/DiagnosticLog.h"
#include "Core/DiagnosticLogFile.h"
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
    try {RC::Output::send<RC::LogLevel::Warning>(STR("[RuneSchema] Cannot write RuneSchema.log in the RuneSchema root; check folder permissions/free space. Detailed logging disabled; UE4SS summaries, warnings and errors remain available.\n"));}catch(...){}
}
}
void Initialize() noexcept {
    try {
        std::call_once(Start,[]{
            if(!Sink().Open(HostServices::ModDirectory()/"RuneSchema.log")){ReportFailure();return;}
            Ready=true;
            if(!Sink().Write("[RuneSchema] Session started. Advanced diagnostics are file-only when advancedLogging=true; summaries, warnings and errors remain in UE4SS.",true))ReportFailure();
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
