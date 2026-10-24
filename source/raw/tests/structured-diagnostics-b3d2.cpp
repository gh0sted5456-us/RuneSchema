#include "Utility/StructuredDiagnosticB3C.h"
#include "Utility/StructuredDiagnosticNpcOnce.h"
#include <cassert>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace PS::StructuredDiagnostics {
static std::vector<PS::StructuredDiagnostic::Record> sink;
bool Emit(PS::StructuredDiagnostic::Record record) noexcept {
    record.EventId="b3d2-event";
    record.SessionId="b3d2-session";
    record.Timestamp="2026-10-21T13:00:00Z";
    if(!PS::StructuredDiagnostic::Validate(record))return false;
    sink.push_back(std::move(record));
    return true;
}
}
int main() {
    using PS::StructuredDiagnosticB3C::Report;
    using PS::StructuredDiagnosticNpcOnce::ReportOnce;
    const std::pair<std::string_view,std::string_view> categories[] = {
        {"RS-NPC-002","Skipped"},
        {"RS-NPC-003","Skipped"},
        {"RS-NPC-004","Blocked"},
        {"RS-NPC-005","Blocked"},
        {"RS-NPC-006","Failed"},
        {"RS-NPC-007","Failed"},
        {"RS-NPC-008","Failed"},
        {"RS-NPC-009","Failed"},
        {"RS-NPC-010","Failed"},
        {"RS-NPC-011","Failed"},
        {"RS-NPC-012","Failed"}
    };
    for(const auto& [code, outcome] : categories) {
        const std::string severity=(code=="RS-NPC-002" || code=="RS-NPC-003")?"Warning":"Error";
        assert(Report(std::string(code),severity,std::string(outcome),"NpcLoader",
            "NPC failure","Real observed failure", "ExampleMod",{},"VendorA","Application"));
        const auto& record=PS::StructuredDiagnostics::sink.back();
        assert(record.Code==code && record.Source=="NpcLoader" && record.Outcome==outcome);
        assert(record.Context.at("ModId")=="ExampleMod");
        assert(record.Context.at("Target")=="VendorA");
        const auto encoded=PS::StructuredDiagnostic::Encode(record);
        assert(encoded && PS::StructuredDiagnostic::Parse(*encoded).has_value());
    }
    assert(PS::StructuredDiagnostics::sink.size()==std::size(categories));
    std::unordered_set<std::string> seen;
    unsigned send=0,log=0;
    auto accepted=[&] {++send;return true;};
    auto rejected=[&] {++send;return false;};
    auto legacy=[&] {++log;};
    assert(ReportOnce(seen,"npc-a",false,accepted,legacy));
    assert(send==1 && log==0);
    assert(!ReportOnce(seen,"npc-a",true,rejected,legacy));
    assert(send==1 && log==0);
    assert(ReportOnce(seen,"npc-b",false,rejected,legacy));
    assert(send==2 && log==1);
    assert(ReportOnce(seen,"npc-c",true,accepted,legacy));
    assert(send==3 && log==2);
    assert(ReportOnce(seen,"npc-mirrored",true,accepted,legacy,true));
    assert(send==4 && log==2);
    assert(ReportOnce(seen,"npc-d",false,[&]() -> bool {++send;throw std::runtime_error("writer failed");},legacy));
    assert(send==5 && log==3);
    assert(Report("RS-NPC-012","Error","Failed","NpcLoader",
        "Saved dialogue state import failed","Import rejected", "ModA",{},"QuestB","Application"));
    auto first=PS::StructuredDiagnostics::sink.back();
    assert(Report("RS-NPC-012","Error","Failed","NpcLoader",
        "Saved dialogue state import failed","Import rejected", "ModB",{},"QuestB","Application"));
    assert(PS::StructuredDiagnostic::IssueKey(first)!=
           PS::StructuredDiagnostic::IssueKey(PS::StructuredDiagnostics::sink.back()));
    assert(!Report("RS-NPC-012","Error","Failed","NpcLoader", "Bad","Bad",{}, {}, {},"UnsupportedPhase"));
}
