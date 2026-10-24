#include "Utility/StructuredDiagnosticB3C.h"
#include "Utility/StructuredDiagnosticNpcOnce.h"
#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace PS::StructuredDiagnostics {
static std::vector<PS::StructuredDiagnostic::Record> records;
bool Emit(PS::StructuredDiagnostic::Record record) noexcept {
    record.EventId="b3d3-event";
    record.SessionId="b3d3-session";
    record.Timestamp="2026-10-21T14:20:00Z";
    if(!PS::StructuredDiagnostic::Validate(record))return false;
    records.push_back(std::move(record));
    return true;
}
}

struct Site {
    const char* Code;
    const char* Filename;
    const char* Severity;
    const char* Outcome;
    unsigned Count;
};

int main(int argc,char** argv) {
    assert(argc==2);
    const std::filesystem::path source=argv[1];
    using PS::StructuredDiagnosticB3C::Report;
    using PS::StructuredDiagnosticNpcOnce::ReportOnce;
    constexpr std::array sites={
        Site{"RS-NPC-013","NpcQuestAcquisition.inl","Error","Partial",1},
        Site{"RS-NPC-014","NpcQuestAcquisition.inl","Warning","Skipped",1},
        Site{"RS-NPC-015","NpcQuestAcquisition.inl","Warning","Skipped",2},
        Site{"RS-NPC-016","NpcQuestAcquisition.inl","Error","Partial",1},
        Site{"RS-NPC-017","NpcQuestCompletion.inl","Error","Partial",1},
        Site{"RS-NPC-018","NpcQuestPresentation.inl","Warning","Skipped",1},
        Site{"RS-NPC-019","NpcCleanup.inl","Error","Deferred",1},
        Site{"RS-NPC-020","NpcDialogue.inl","Warning","Blocked",1},
        Site{"RS-NPC-021","NpcDialogue.inl","Warning","Skipped",1},
        Site{"RS-NPC-022","NpcDialogue.inl","Error","Partial",1},
        Site{"RS-NPC-023","NpcDialogue.inl","Error","Partial",1},
        Site{"RS-NPC-024","NpcDialogue.inl","Error","Failed",1},
        Site{"RS-NPC-025","NpcDialogue.inl","Error","Partial",1},
        Site{"RS-NPC-026","NpcEquipment.inl","Error","Partial",1},
        Site{"RS-NPC-027","NpcDialogue.inl","Error","Deferred",1},
        Site{"RS-NPC-028","NpcDialogue.inl","Error","Failed",1},
        Site{"RS-NPC-029","NpcQuestScroll.inl","Error","Partial",1}
    };
    for(const Site& site:sites) {
        std::ifstream stream(source/site.Filename,std::ios::binary);
        assert(stream.good());
        const std::string file{std::istreambuf_iterator<char>{stream},std::istreambuf_iterator<char>{}};
        const std::string token=std::string{"\""}+site.Code+"\"";
        unsigned found=0;
        for(std::size_t position=0;(position=file.find(token,position))!=std::string::npos;position+=token.size())++found;
        assert(found==site.Count);
        const auto first=file.find(token);
        assert(first!=std::string::npos);
        const auto reportCall=file.rfind("ErrorOnceDiagnostic(",first);
        assert(reportCall!=std::string::npos && first-reportCall<550);
        assert(Report(site.Code,site.Severity,site.Outcome,"NpcLoader", "Observed callback failure",
            "A source-grounded branch failed","ExampleMod",{},"QuestTarget","Application"));
        const auto& report=PS::StructuredDiagnostics::records.back();
        assert(report.Code==site.Code && report.Source=="NpcLoader");
        assert(report.Severity==site.Severity && report.Outcome==site.Outcome);
        const auto encoded=PS::StructuredDiagnostic::Encode(report);
        assert(encoded);
        const auto decoded=PS::StructuredDiagnostic::Parse(*encoded);
        assert(decoded && decoded->Code==site.Code && decoded->Outcome==site.Outcome);
    }
    assert(PS::StructuredDiagnostics::records.size()==sites.size());
    std::unordered_set<std::string> seen;
    int reports=0,logs=0;
    const auto sink=[&](){++reports;return true;};
    const auto legacy=[&](){++logs;};
    assert(ReportOnce(seen,"quest-staged",true,sink,legacy));
    assert(logs==1 && reports==1);
    assert(!ReportOnce(seen,"quest-staged",true,sink,legacy));
    assert(logs==1 && reports==1);
    assert(ReportOnce(seen,"quest-skipped",false,sink,legacy));
    assert(logs==1 && reports==2);
    assert(ReportOnce(seen,"quest-writer-down",false,[](){return false;},legacy));
    assert(logs==2);
    assert(ReportOnce(seen,"quest-writer-throw",false,[]()->bool{throw std::runtime_error("writer");},legacy));
    assert(logs==3);
    assert(Report("RS-NPC-016","Error","Partial","NpcLoader", "Acquisition","Uncertain", "ModA",{},"QuestA","Application"));
    const auto key=PS::StructuredDiagnostic::IssueKey(PS::StructuredDiagnostics::records.back());
    assert(Report("RS-NPC-016","Error","Partial","NpcLoader", "Acquisition","Uncertain", "ModB",{},"QuestA","Application"));
    assert(key!=PS::StructuredDiagnostic::IssueKey(PS::StructuredDiagnostics::records.back()));
}
