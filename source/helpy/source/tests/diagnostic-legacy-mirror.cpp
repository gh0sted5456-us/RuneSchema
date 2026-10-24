#include "Core/DiagnosticCenterModel.h"
#include "Core/StructuredDiagnosticLegacyMirror.h"
#include <cassert>
#include <string>
using namespace PS;
static StructuredDiagnostic::Record Make(std::string id="s-test-e1",std::string sev="Error") {
    StructuredDiagnostic::Record r;
    r.EventId=id;r.SessionId="s-test";r.Timestamp="2026-10-23T18:30:00.000Z";
    r.Code=sev=="Error"?"RS-BP-009":"RS-BP-010";r.Severity=sev;
    r.Source="BlueprintLoader";r.Outcome="Skipped";
    r.Title="Could not edit property";r.Message="Property write failed.\nCheck target.";
    r.Context={{"ModId","TestMod"},{"Phase","Application"}};
    assert(StructuredDiagnostic::Validate(r));return r;
}
int main(){
    namespace Mirror=StructuredDiagnosticLegacyMirror;
    using namespace DiagnosticCenterModel;
    const auto r=Make();
    const auto line=Mirror::Format(r);
    assert(line.starts_with("[RuneSchema] [error] [RS-BP-009]"));
    assert(line.find("failed. Check target.")!=std::string::npos);
    assert(line.find("{\"Version\"")==std::string::npos);
    assert(Mirror::EventReference(line)=="s-test:s-test-e1");
    const auto legacy=LiveLog::Parse(line);
    assert(legacy.ReportedCode==r.Code);
    assert(legacy.Severity==LiveLog::Level::Error);
    assert(DiagnosticLegacyIntake::MirrorReference(legacy)=="s-test:s-test-e1");
    assert(Mirror::Format(Make("s-warning", "Warning")).find("[warning] [RS-BP-010]")!=std::string::npos);
    auto noMirror=Make();noMirror.Severity="Info";assert(Mirror::Format(noMirror).empty());
    assert(Mirror::EventReference("[diag-event=bad bracket]]").empty());
    assert(Mirror::EventReference("[diag-event=bad token]").empty());
    std::string longMessage(800,'X');longMessage+="\n[warning] injection";
    auto lengthy=Make();lengthy.Title=std::string(350,'a');lengthy.Message=longMessage;
    auto bounded=Mirror::Format(lengthy);
    assert(bounded.size()<1024 && std::count(bounded.begin(),bounded.end(),'\n')==1);
    Store first;
    auto mirrorGroup=first.Accept(legacy);
    assert(mirrorGroup && first.Groups().size()==1 && first.Events().size()==1);
    assert(first.Accept(legacy)==0);
    const auto authoritative=first.AcceptStructured(r);
    assert(authoritative && first.Groups().size()==1 && first.Events().size()==1);
    assert(first.Groups()[0].Code==r.Code && first.Groups()[0].Count==1);
    assert(!first.Events()[0].StructuredJson.empty());
    assert(first.Accept(legacy)==0 && first.AcceptStructured(r)==0);
    Store second;
    assert(second.AcceptStructured(r));
    assert(second.Accept(legacy)==0 && second.Events().size()==1);
    auto unrelated=LiveLog::Parse("[RuneSchema] [error] [RS-BP-009] Other operation failed");
    const auto independent=second.Accept(unrelated);
    assert(independent && second.Groups().size()==2 && second.Events().size()==2);
    Store fallback;
    assert(fallback.Accept(legacy));
    assert(fallback.FindGroup(fallback.Events().front().GroupId)->Code==r.Code);
    fallback.Clear();assert(fallback.Groups().empty()&&fallback.Events().empty());
    Store multiple;
    auto other=Make("s-test-e2");
    assert(multiple.Accept(LiveLog::Parse(Mirror::Format(other))));
    assert(multiple.AcceptStructured(r));
    assert(multiple.AcceptStructured(other));
    assert(multiple.Groups().size()==1 && multiple.Groups()[0].Count==2);
    Store small({1,1024,2});
    assert(small.Accept(legacy));
    assert(small.Accept(unrelated));
    assert(small.AcceptStructured(r));
    assert(small.UnreadIssues()>=1);
}
