#include "Core/DiagnosticCenterReport.h"
#include <cassert>
#include <string>
using namespace PS;
int main(){
    assert(DiagnosticCenterReport::IsReportDestination("RuneSchema-diagnostics.TXT"));
    assert(DiagnosticCenterReport::IsReportDestination(std::filesystem::path(u8"診断.txt")));
    for(const auto* path:{"Mods.txt","enabled.txt","character.sav","RuneSchema.profile","RSDiagLog.jsonl","settings.ini"})
        assert(!DiagnosticCenterReport::IsReportDestination(path));
    DiagnosticCenterModel::Store store;
    auto a=LiveLog::Parse("[RuneSchema] [warning] [WEARABLE-BRIDGE][NOT-ENROLLED] C:\\Users\\Alice\\Mods\\Cape.json");
    auto b=LiveLog::Parse("[RuneSchema] [error] asset /Game/Items/DragonLimb not found C:/Users/Bob/Documents/notes.txt");
    const auto id=store.Accept(a);store.Accept(a);store.Accept(b);
    const auto* group=store.FindGroup(id);
    assert(group&&group->Count==2&&group->EvidenceIds.size()==2);
    const auto report=DiagnosticCenterReport::Issue(store,*group,"Current session");
    assert(report.find("occurrences")==std::string::npos || report.find("Occurrences reported: 2")!=std::string::npos);
    assert(report.find("Alice")==std::string::npos);
    assert(report.find("<user>")!=std::string::npos);
    assert(report.find("Cape.json")!=std::string::npos);
    assert(report.find("[warning]")!=std::string::npos);
    assert(report.find("Viewed:")==std::string::npos);
    assert(report.find("Retained events:")==std::string::npos);
    assert(report.find("Diagnostic code: none emitted")==std::string::npos || group->Code.empty());
    const auto concise=DiagnosticCenterReport::Issue(store,*group,"Current session",{false,true});
    assert(concise.find("Retained issue evidence")==std::string::npos);
    assert(concise.find("Original raw evidence excluded")!=std::string::npos);
    const auto unredacted=DiagnosticCenterReport::Issue(store,*group,"Current session",{true,false});
    assert(unredacted.find("Alice")!=std::string::npos);
    const auto withoutEvidence=DiagnosticCenterReport::Issue(store,*group,"Current session",{false,true});
    assert(withoutEvidence.find("Original retained evidence")==std::string::npos);
    auto session=DiagnosticCenterReport::Session(store,"Current session");
    assert(session.find("Groups: 1 error, 1 warning")!=std::string::npos);
    assert(session.find("Alice")==std::string::npos);
    assert(session.find("Bob")==std::string::npos);
    assert(session.find("/Game/Items/DragonLimb")!=std::string::npos);
    assert(DiagnosticCenterReport::Session(store,"Current session",{false,true}).find("CHRONOLOGICAL RETAINED EVIDENCE")==std::string::npos);
    // Best-effort redaction must handle more than one user-home path per line.
    assert(DiagnosticCenterReport::Sanitize("C:\\Users\\A\\x and D:/Users/B/y",true)=="C:\\Users\\<user>\\x and D:/Users/<user>/y");
}
