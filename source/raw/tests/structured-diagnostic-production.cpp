#include "Utility/StructuredDiagnostics.h"
#include "Runtime/HostServices.h"
#include "Utility/DiagnosticLog.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <cassert>
#include <chrono>
#include <thread>
#include <unordered_set>
#include <iostream>
namespace { std::vector<std::string> mirrors; }
namespace PS::DiagnosticLog {void WriteUtf8(std::string_view text,bool) noexcept {mirrors.emplace_back(text);}}
namespace PS::HostServices {std::filesystem::path ModDirectory(){return std::filesystem::temp_directory_path()/"rsdiag-production-b1";}}
int main(){
    const auto path=PS::HostServices::ModDirectory();std::filesystem::create_directories(path);
    std::filesystem::remove(path/"RSDiagLog.jsonl");
    PS::StructuredDiagnostics::Initialize();
    PS::StructuredDiagnostic::Record report;
    report.Code="RS-JSON-001";report.Severity="Error";report.Outcome="Failed";report.Source="JsonParser";
    report.Title="Invalid configuration";report.Message="JSON document was rejected.";
    report.Context={{"ModId","TestMod"},{"Phase","Parsing"}};
    assert(PS::StructuredDiagnostics::Emit(report));
    constexpr int producers=8,perProducer=1000;std::atomic<unsigned> accepted{1};std::vector<std::thread> threads;
    const auto started=std::chrono::steady_clock::now();
    for(int producer=0;producer<producers;++producer)threads.emplace_back([&,producer]{
        for(int i=0;i<perProducer;++i){
            auto event=report;event.Code="RS-IMP-013";event.Severity="Info";event.Outcome="Succeeded";event.Source="ImportPlanner";
            event.Title="Dry-run fixture";event.OperationId="stress-"+std::to_string(producer)+"-"+std::to_string(i);
            event.Context["ModId"]="Producer"+std::to_string(producer);event.Context["Phase"]="Planning";
            if(i==perProducer/2){event.Code="RS-IMP-014";event.Severity="Error";event.Outcome="Blocked";event.Title="Critical import fixture";event.Context["Phase"]="Application";}
            if(PS::StructuredDiagnostics::Emit(std::move(event)))++accepted;
        }
    });
    for(auto& thread:threads)thread.join();
    PS::StructuredDiagnostics::Shutdown();
    const auto health=PS::StructuredDiagnostics::Counters();
    assert(health.Written<=accepted&&health.Dropped+health.Written==1+producers*perProducer&&health.Errors==0);
    std::ifstream input(path/"RSDiagLog.jsonl",std::ios::binary);std::string line;
    assert(std::getline(input,line));auto parsed=PS::StructuredDiagnostic::Parse(line);
    assert(parsed&&parsed->SessionId.starts_with("s-")&&parsed->EventId.find("-e")!=std::string::npos);
    assert(parsed->Code==report.Code&&parsed->Source==report.Source);
    std::unordered_set<std::string> events{parsed->EventId},operations;unsigned critical=0;
    while(std::getline(input,line)){
        parsed=PS::StructuredDiagnostic::Parse(line);assert(parsed&&parsed->Source=="ImportPlanner");
        if(parsed->Code=="RS-IMP-014"){++critical;assert(parsed->Severity=="Error"&&parsed->Context["Phase"]=="Application");}
        else assert(parsed->Code=="RS-IMP-013"&&parsed->Severity=="Info"&&parsed->Context["Phase"]=="Planning");
        assert(events.insert(parsed->EventId).second&&operations.insert(parsed->OperationId).second);
        const auto prefix="stress-"+parsed->Context["ModId"].get<std::string>().substr(8)+"-";
        assert(parsed->OperationId.starts_with(prefix));
    }
    assert(events.size()==health.Written&&critical==producers&&mirrors.size()==producers+1&&mirrors.front().find("[RS-JSON-001]")!=std::string::npos);
    std::cout<<"Structured sink: "<<producers<<" producers, "<<1+producers*perProducer<<" attempts, "<<health.Written<<" written, "<<health.Dropped<<" accounted drops, "<<health.Errors<<" errors, "
             <<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count()<<" ms.\n";
    assert(!PS::StructuredDiagnostics::Emit(report));
    input.close();std::filesystem::remove_all(path);
}
