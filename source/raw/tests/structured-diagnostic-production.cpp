#include "Utility/StructuredDiagnostics.h"
#include "Runtime/HostServices.h"
#include "Utility/DiagnosticLog.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <cassert>
#include <chrono>
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
    PS::StructuredDiagnostics::Shutdown();
    assert(PS::StructuredDiagnostics::Counters().Written>=1);
    std::ifstream input(path/"RSDiagLog.jsonl",std::ios::binary);std::string line;
    assert(std::getline(input,line));auto parsed=PS::StructuredDiagnostic::Parse(line);
    assert(parsed&&parsed->SessionId.starts_with("s-")&&parsed->EventId.find("-e")!=std::string::npos);
    assert(parsed->Code==report.Code&&parsed->Source==report.Source);
    assert(!std::getline(input,line));
    assert(mirrors.size()==1&&mirrors.front().find("[RS-JSON-001]")!=std::string::npos);
    assert(!PS::StructuredDiagnostics::Emit(report));
    input.close();std::filesystem::remove_all(path);
}
