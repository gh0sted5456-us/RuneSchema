#include "Core/StructuredDiagnosticFile.h"
#include "Core/StructuredDiagnosticLegacyMirror.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
namespace {
std::vector<std::string> mirrored;
void OnWritten(const PS::StructuredDiagnostic::Record& record) noexcept {
    mirrored.push_back(PS::StructuredDiagnosticLegacyMirror::Format(record));
}
PS::StructuredDiagnostic::Record Make(unsigned i){
    PS::StructuredDiagnostic::Record r;
    r.SessionId="s-commit";r.EventId="s-commit-e"+std::to_string(i);
    r.Timestamp="2026-10-23T18:00:00.000Z";
    r.Code="RS-JSON-001";r.Severity="Error";r.Outcome="Failed";
    r.Source="JsonParser";r.Title="Invalid config";r.Message="Malformed JSON document";
    r.Context={{"ModId","Sample"},{"File","mod.jsonc"}};
    return r;
}
}
int main(){
    const auto file=std::filesystem::temp_directory_path()/"rsdiag-legacy-commit-test.jsonl";
    std::filesystem::remove(file);
    PS::StructuredDiagnostic::File sink;
    sink.OnWritten(&OnWritten);
    assert(sink.Open(file));
    for(unsigned i=1;i<=3;++i)assert(sink.Emit(Make(i)));
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    sink.Close();
    assert(sink.Written()==2);
    assert(mirrored.size()==2);
    assert(mirrored[0].find("[RS-JSON-001]")!=std::string::npos);
    std::ifstream input(file,std::ios::binary);std::string line;std::vector<PS::StructuredDiagnostic::Record> docs;
    while(std::getline(input,line)){auto r=PS::StructuredDiagnostic::Parse(line);assert(r);docs.push_back(*r);}
    assert(docs.size()==2);
    assert(docs[1].Evidence.at("RepeatCount")==2);
    for(std::size_t i=0;i<docs.size();++i)
        assert(PS::StructuredDiagnosticLegacyMirror::EventReference(mirrored[i])==
            PS::StructuredDiagnosticLegacyMirror::Reference(docs[i]));
    input.close();std::filesystem::remove(file);
}
