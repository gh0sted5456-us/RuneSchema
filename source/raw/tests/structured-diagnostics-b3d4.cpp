#include "Utility/StructuredDiagnosticB3D.h"
#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace PS::StructuredDiagnostics {
static std::vector<PS::StructuredDiagnostic::Record> emitted;
bool Emit(PS::StructuredDiagnostic::Record record) noexcept {
    record.EventId="b3d4-event";
    record.SessionId="b3d4-session";
    record.Timestamp="2026-10-21T19:00:00Z";
    if (!PS::StructuredDiagnostic::Validate(record)) return false;
    emitted.push_back(std::move(record));
    return true;
}
}

struct Site {
    std::string_view code;
    std::string_view file;
    std::string_view severity;
    std::string_view outcome;
    std::string_view phase;
    int count;
};

static std::string load(const std::filesystem::path& path) {
    std::ifstream stream(path);
    assert(stream.good());
    return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}

int main(int argc,char** argv) {
    assert(argc==2);
    const auto root=std::filesystem::path{argv[1]};
    constexpr std::array sites={
        Site{"RS-LOAD-001","DragonWildsModLoaderBase.cpp","Warning","Skipped","Discovery",1},
        Site{"RS-LOAD-002","DragonWildsModLoaderBase.cpp","Warning","Blocked","Resolution",1},
        Site{"RS-LOAD-003","DragonWildsModLoaderBase.cpp","Warning","Partial","Application",1},
        Site{"RS-LOAD-004","DragonWildsModLoaderBase.cpp","Warning","Partial","Application",1},
        Site{"RS-CORE-019","DragonWildsMainLoader.cpp","Warning","Blocked","Resolution",2},
        Site{"RS-CORE-020","DragonWildsMainLoader.cpp","Warning","Partial","Finalization",2},
        Site{"RS-CORE-021","DragonWildsMainLoader.cpp","Warning","Partial","Resolution",1},
        Site{"RS-CORE-022","DragonWildsMainLoader.cpp","Warning","Partial","Finalization",1},
        Site{"RS-CORE-023","DragonWildsMainLoader.cpp","Warning","Skipped","Application",1},
        Site{"RS-CORE-024","DragonWildsMainLoader.cpp","Warning","Partial","Discovery",1},
        Site{"RS-CORE-025","DragonWildsMainLoader.cpp","Warning","Skipped","Application",1},
        Site{"RS-CORE-026","DragonWildsMainLoader.cpp","Warning","Partial","Finalization",1},
        Site{"RS-CORE-027","DragonWildsMainLoader.cpp","Warning","Skipped","Application",1},
        Site{"RS-CORE-028","DragonWildsMainLoader.cpp","Warning","Partial","Finalization",1},
        Site{"RS-CORE-029","DragonWildsMainLoader.cpp","Warning","Skipped","Finalization",1},
    };
    for (const auto& site:sites) {
        const auto content=load(root/std::string{site.file});
        const auto match="\""+std::string{site.code}+"\"";
        int count=0;
        for (std::size_t position=0; (position=content.find(match,position))!=std::string::npos;
             position+=match.size()) {
            ++count;
            const auto prefix=content.rfind("StructuredDiagnosticB3D::Report(",position);
            assert(prefix!=std::string::npos && position-prefix<100);
        }
        assert(count==site.count);
        const bool accepted=PS::StructuredDiagnosticB3D::Report(std::string{site.code},
            std::string{site.severity},std::string{site.outcome},"Observed loader failure",
            "The failing operation was observed; no underlying cause is assumed.",
            std::string{site.file},std::string{site.phase},"TestMod");
        assert(accepted);
        const auto& record=PS::StructuredDiagnostics::emitted.back();
        assert(record.Source=="LoaderCore" && record.Code==site.code);
        assert(record.Context.is_object() && record.Context.value("ModId",std::string{})=="TestMod");
        assert(record.Outcome==site.outcome && record.Severity==site.severity);
        const auto json=PS::StructuredDiagnostic::Encode(record);
        assert(json);
        const auto parsed=PS::StructuredDiagnostic::Parse(*json);
        assert(parsed && parsed->Code==site.code && parsed->Outcome==site.outcome);
    }
    const auto first=PS::StructuredDiagnostic::IssueKey(PS::StructuredDiagnostics::emitted.back());
    assert(PS::StructuredDiagnosticB3D::Report("RS-CORE-029","Warning","Skipped",
        "Ledger failed","Exception", "PersistenceLedger","Finalization","DifferentMod"));
    assert(first!=PS::StructuredDiagnostic::IssueKey(PS::StructuredDiagnostics::emitted.back()));

    const auto main=load(root/"DragonWildsMainLoader.cpp");
    assert(main.find("[PERF][ASSETS] Native prefetch unavailable")!=std::string::npos);
    assert(main.find("RS-CORE-030")==std::string::npos);
    const auto base=load(root/"DragonWildsModLoaderBase.cpp");
    assert(base.find("if (!PS::PSConfig::Get()->IsLoaderEnabled(m_modFolderType)")!=std::string::npos);
    assert(base.find("return true;\n            }\n            return false;")!=std::string::npos);
}
