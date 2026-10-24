#include "Core/DiagnosticLegacyIntake.h"
#include "Core/DiagnosticCenterModel.h"
#include "Core/DiagnosticCenterReport.h"
#include <cassert>
using namespace PS;
int main(){
    const auto warning=LiveLog::Parse("[RuneSchema] [warning] [RS-CACHE-001] Source changed");
    assert(DiagnosticLegacyIntake::Eligible(warning));
    assert(DiagnosticLegacyIntake::HasEmittedCode(warning));
    DiagnosticCenterModel::Store store;
    const auto id=store.Accept(warning);assert(id);
    assert(store.FindGroup(id)->Code=="RS-CACHE-001");
    const auto explicitReport=DiagnosticCenterReport::Issue(store,*store.FindGroup(id),"current");
    assert(explicitReport.find("Diagnostic code: RS-CACHE-001 (emitted in legacy log)")!=std::string::npos);

    const auto suggested=LiveLog::Parse("[RuneSchema] [error] [PERSISTENCE-PRUNER] Duplicate quest owner marker.");
    assert(DiagnosticLegacyIntake::Eligible(suggested));
    assert(!DiagnosticLegacyIntake::HasEmittedCode(suggested));
    assert(!DiagnosticLegacyIntake::SuggestedReference(suggested).empty());
    const auto sid=store.Accept(suggested);
    assert(store.FindGroup(sid)->Code.empty());
    const auto report=DiagnosticCenterReport::Issue(store,*store.FindGroup(sid),"current");
    assert(report.find("none emitted")!=std::string::npos);
    assert(report.find("Suggested reference (not emitted)")!=std::string::npos);

    const auto uncoded=LiveLog::Parse("[RuneSchema] [warning] Unknown component state");
    assert(DiagnosticLegacyIntake::Eligible(uncoded));
    assert(!DiagnosticLegacyIntake::HasEmittedCode(uncoded));
    assert(DiagnosticLegacyIntake::SuggestedReference(uncoded).empty());
    const auto uid=store.Accept(uncoded);
    assert(store.FindGroup(uid)->Code.empty());
    assert(DiagnosticCenterReport::Issue(store,*store.FindGroup(uid),"current").find("none emitted")!=std::string::npos);
    assert(!DiagnosticLegacyIntake::Eligible(LiveLog::Parse("[RuneSchema] [info] ready")));
    assert(!DiagnosticLegacyIntake::Eligible(LiveLog::Parse("[RuneSchema] [debug] verbose")));
    assert(store.UnreadIssues()==3);
}
