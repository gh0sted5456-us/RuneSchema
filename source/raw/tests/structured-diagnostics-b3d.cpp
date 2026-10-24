#include "Utility/StructuredDiagnosticB3D.h"
#include <cassert>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace PS::StructuredDiagnostics {
static std::vector<PS::StructuredDiagnostic::Record> sink;
bool Emit(PS::StructuredDiagnostic::Record r) noexcept {
    r.EventId="evt-b3d";
    r.SessionId="session-b3d";
    r.Timestamp="2026-10-21T13:00:00Z";
    if (!PS::StructuredDiagnostic::Validate(r)) return false;
    sink.push_back(std::move(r));
    return true;
}
}
int main() {
    using PS::StructuredDiagnosticB3D::Report;
    using PS::StructuredDiagnostic::Encode;
    using PS::StructuredDiagnostic::Parse;
    static constexpr std::string_view codes[] = {
        "RS-CORE-001","RS-CORE-002","RS-CORE-003","RS-CORE-004",
        "RS-CORE-005","RS-CORE-006","RS-CORE-007","RS-CORE-008",
        "RS-CORE-009","RS-CORE-010","RS-CORE-011","RS-CORE-012",
        "RS-CORE-013","RS-CORE-014","RS-CORE-015","RS-CORE-016",
        "RS-CORE-017","RS-CORE-018"
    };
    for (auto code : codes) {
        assert(Report(std::string(code),"Error","Blocked","Core operation unavailable",
            "An observed core operation failed.","GameInstance::Init","Application"));
        const auto& r=PS::StructuredDiagnostics::sink.back();
        assert(r.Code==code && r.Source=="LoaderCore");
        assert(r.Context.at("Target")=="GameInstance::Init");
        assert(r.Context.at("Phase")=="Application");
        assert(r.Context.find("ModId")==r.Context.end());
        assert(r.Context.find("File")==r.Context.end());
        auto serialized=Encode(r);
        assert(serialized && Parse(*serialized).has_value());
    }
    assert(PS::StructuredDiagnostics::sink.size()==std::size(codes));
    assert(Report("RS-CORE-011","Error","Failed","Mod auto-reload failed",
        "Parser rejected the file.","blueprints","Reload","TestMod","test.jsonc"));
    const auto& reload=PS::StructuredDiagnostics::sink.back();
    assert(reload.Context.at("ModId")=="TestMod");
    assert(reload.Context.at("File")=="test.jsonc");
    assert(reload.Context.at("Phase")=="Reload");
    assert(Report("RS-CORE-004","Warning","Skipped","Early table observation unavailable",
        "Native signature could not be resolved.","UDataTable::Serialize","Resolution"));
    const auto& early=PS::StructuredDiagnostics::sink.back();
    assert(early.Severity=="Warning" && early.Outcome=="Skipped");
    assert(early.Context.at("Phase")=="Resolution");
    assert(!Report("RS-CORE-019","Error","Failed","Unknown","Unknown",{},"InvalidPhase"));
}
