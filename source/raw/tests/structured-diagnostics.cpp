#include "Core/StructuredDiagnosticContract.h"
#include "Core/StructuredDiagnosticFile.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
using namespace PS::StructuredDiagnostic;
static Record Valid(){
    Record r;r.EventId="s-x-e1";r.SessionId="s-x";r.Timestamp="2026-10-21T10:30:10.123Z";
    r.Code="RS-ASSET-001";r.Severity="Error";r.Outcome="Failed";
    r.Source="AssetLoader";r.Title="Resolution exhausted";r.Message="Could not resolve requested asset";
    r.Context={{"ModId","Blue"},{"Target","/Game/Example"},{"Phase","Resolution"}};
    r.Cause={{"Status","Unknown"}};r.Evidence={{"RetryCount",3}};return r;
}
int main(){
    auto r=Valid();assert(Validate(r));
    auto encoded=Encode(r);assert(encoded&&encoded->back()=='\n'&&encoded->find("thread=")==std::string::npos);
    auto parsed=Parse(encoded->substr(0,encoded->size()-1));assert(parsed&&parsed->Code==r.Code);
    auto other=r;other.EventId="s-x-e2";assert(IssueKey(other)==IssueKey(r));
    other.Context["ModId"]="Other";assert(IssueKey(other)!=IssueKey(r));
    r.Severity="Verbose";assert(!Validate(r));r=Valid();
    r.Context["Phase"]="Anything";assert(!Validate(r));r=Valid();
    r.Evidence["RetryCount"]=-1;assert(!Validate(r));r=Valid();
    r.Code="RS-ASSET-0A1";assert(!Validate(r));r=Valid();
    r.Message=std::string(70000,'x');assert(!Encode(r));
    const auto folder=std::filesystem::temp_directory_path()/"rs-structured-test-b1";
    std::filesystem::create_directories(folder);const auto path=folder/"RSDiagLog.jsonl";
    for(const auto* name:{"repeated.jsonl","truncated.jsonl","overflow.jsonl","overflow.jsonl.1"}){
        std::error_code ec;std::filesystem::remove(folder/name,ec);
    }
    for(int i=0;i<4;++i){std::error_code ec;std::filesystem::remove(i?std::filesystem::path(path.string()+"."+std::to_string(i)):path,ec);}
    {
        File sink;assert(sink.Open(path,MaxRecord+512,3,2048));
        for(int i=0;i<60;++i){auto entry=Valid();entry.EventId="s-x-e"+std::to_string(i);entry.Context["Target"]="/Game/Example"+std::to_string(i);entry.Message=std::string(2048,'z');assert(sink.Emit(std::move(entry)));}
        sink.Close();assert(sink.Written()>=1&&sink.Written()<=60);assert(sink.Errors()==0);
    }
    assert(std::filesystem::exists(path));assert(std::filesystem::exists(std::filesystem::path(path.string()+".1")));
    std::ifstream in(path);std::string line;while(std::getline(in,line))assert(Parse(line));
    // Repeated, identical reports are aggregated without losing count.
    const auto repeats=folder/"repeated.jsonl";
    {
        File sink;assert(sink.Open(repeats));
        for(int i=0;i<8;++i){auto entry=Valid();entry.EventId="e-"+std::to_string(i);assert(sink.Emit(std::move(entry)));}
        std::this_thread::sleep_for(std::chrono::milliseconds(1450));
        sink.Close();assert(sink.Errors()==0);
    }
    std::ifstream summary(repeats);std::uint64_t seen=0;int records=0;
    while(std::getline(summary,line)){
        auto parsed=Parse(line);assert(parsed);++records;
        seen+=parsed->Evidence.value("RepeatCount",std::uint64_t{1});
    }
    assert(records>=1&&records<=8&&seen==8);
    // Writer preserves a bounded truncated record when JSON escaping alone
    // pushes otherwise valid narrative beyond the 64 KiB wire limit.
    const auto oversized=folder/"truncated.jsonl";
    {
        File sink;assert(sink.Open(oversized));
        auto entry=Valid();entry.Message=std::string(16000,'\0');
        assert(Validate(entry)&&!Encode(entry));
        assert(sink.Emit(std::move(entry)));sink.Close();assert(sink.Errors()==0);
    }
    {std::ifstream inOversized(oversized);std::string encodedLine;
     assert(std::getline(inOversized,encodedLine));const auto entry=Parse(encodedLine);
     assert(entry&&entry->Evidence.value("Truncated",false));}
    // A deliberately tiny queue refuses work under flooding without blocking.
    const auto overflow=folder/"overflow.jsonl";
    {
        File sink;assert(sink.Open(overflow,MaxRecord+512,1,1));
        for(int i=0;i<250;++i){auto entry=Valid();entry.EventId="f-"+std::to_string(i);entry.Message=std::string(16000,'x');(void)sink.Emit(std::move(entry));}
        sink.Close();assert(sink.Errors()==0);
    }
    in.close();summary.close();
    std::filesystem::remove_all(folder);
}
