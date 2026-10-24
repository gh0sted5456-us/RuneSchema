#include "Utility/StructuredDiagnosticB3B.h"
#include <cassert>
#include <string>
#include <string_view>
#include <vector>
using PS::StructuredDiagnostic::Record;
static std::vector<Record> written;
namespace PS::StructuredDiagnostics {
bool Emit(Record r) noexcept {
    r.EventId = "event-1";
    r.SessionId = "session-1";
    r.Timestamp = "2026-10-21T10:00:00Z";
    if (!StructuredDiagnostic::Validate(r)) return false;
    written.push_back(std::move(r));
    return true;
}
}
int main() {
    using PS::StructuredDiagnosticB3B::Report;
    static constexpr std::string_view building[] = {
        "RS-BLD-001","RS-BLD-002","RS-BLD-003","RS-BLD-004","RS-BLD-005",
        "RS-BLD-006","RS-BLD-007","RS-BLD-008","RS-BLD-009","RS-BLD-010",
        "RS-BLD-011","RS-BLD-012","RS-BLD-013","RS-BLD-014","RS-BLD-015"
    };
    static constexpr std::string_view spawn[] = {
        "RS-SPAWN-001","RS-SPAWN-002","RS-SPAWN-003","RS-SPAWN-004",
        "RS-SPAWN-005","RS-SPAWN-006","RS-SPAWN-007"
    };
    for (auto code : building) {
        assert(Report(std::string(code),"Error","Skipped","BuildingLoader",
            "Building definition rejected","The building definition violates the supported contract.",
            "BuilderMod","nested/build.jsonc","BuildingOne","Validation","Properties"));
        const auto& latest=written.back();
        assert(latest.Code==code && latest.Context.at("ModId")=="BuilderMod");
        assert(latest.Context.at("File")=="nested/build.jsonc");
        assert(latest.Context.at("Target")=="BuildingOne");
        assert(PS::StructuredDiagnostic::Parse(PS::StructuredDiagnostic::Encode(latest).value()).has_value());
    }
    for (auto code : spawn) {
        assert(Report(std::string(code),"Warning","Deferred","SpawnLoader",
            "Spawn change deferred","The live spawn is not replaceable at this lifecycle phase.",
            "SpawnMod","nested/spawn.jsonc","SpA","Reload"));
        const auto& latest=written.back();
        assert(latest.Code==code && latest.Context.at("File")=="nested/spawn.jsonc");
    }
    assert(written.size()==22);
    // Missing optional provenance must not become an invented mod or path.
    assert(Report("RS-SPAWN-006","Error","Failed","SpawnLoader",
        "Spawn hook unavailable","A world callback could not be installed.",
        {},{},{},"Finalization"));
    assert(written.back().Context.size()==1);
}
