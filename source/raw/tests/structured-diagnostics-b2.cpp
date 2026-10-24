#include "Utility/StructuredDiagnosticB2.h"
#include <cassert>
#include <string>
#include <vector>

static std::vector<PS::StructuredDiagnostic::Record> emitted;
namespace PS::StructuredDiagnostics {
bool Emit(StructuredDiagnostic::Record r) noexcept {
    r.EventId="b2-e"+std::to_string(emitted.size()+1);
    r.SessionId="test-b2";
    r.Timestamp="2026-10-21T11:00:00.123Z";
    if(!StructuredDiagnostic::Validate(r))return false;
    emitted.push_back(std::move(r));return true;
}
}
int main(){
    using namespace PS::StructuredDiagnosticB2;
    const auto first=NewOperationId("asset"),second=NewOperationId("asset");
    assert(first!=second);
    assert(SourceWithinSection(std::filesystem::path("mods/A/assets/gear/sub.jsonc"),"assets")=="gear/sub.jsonc");
    assert(SourceWithinSection(std::filesystem::path("mods/A/blueprints/foo.jsonc"),"blueprints")=="foo.jsonc");
    assert(SourceWithinSection(std::filesystem::path("other/foo.jsonc"),"assets")=="foo.jsonc");
    assert(SameAsciiDirectory("MODS","mods"));
    assert(!SameAsciiDirectory("modX","mods"));
    assert(SourceWithinSection(std::filesystem::path("Mods/A/ASSETS/Gear/Sub.jsonc"),"assets")=="Gear/Sub.jsonc");
    assert(SourceWithinSection(std::filesystem::path("Mods/A/blueprints/Foo.jsonc"),"BLUEPRINTS")=="Foo.jsonc");
    assert(Utf8Path(std::filesystem::path(u8"blueprints/école.jsonc")).find("école.jsonc")!=std::string::npos);
    Provenance asset{"ModA","assets/target.jsonc","/Game/Test.Item",first};
    assert(Report("RS-ASSET-001","Error","Failed","AssetLoader",
        "Asset unresolved","Target not available",asset,"Resolution"));
    assert(Report("RS-ASSET-002","Warning","Skipped","AssetLoader",
        "Invalid asset root","Expected object",asset,"Validation"));
    assert(Report("RS-ASSET-003","Warning","Partial","AssetLoader",
        "Rejected field","Property write failed",asset,"Application","Damage"));
    Provenance blueprint{"ModB","blueprints/spell.jsonc","BP_Spell_C",NewOperationId("blueprint")};
    assert(Report("RS-BP-001","Warning","Partial","BlueprintLoader",
        "Missing property","Not found",blueprint,"Application","Effect.Particle.Radius"));
    assert(Report("RS-BP-002","Error","Failed","BlueprintLoader",
        "Hook unavailable","Disabled",blueprint,"Validation"));
    assert(Report("RS-BP-003","Warning","Deferred","BlueprintLoader",
        "Requires restart","Reload deferred",blueprint,"Reload"));
    assert(emitted.size()==6);
    assert(emitted[0].Context["ModId"]=="ModA"&&emitted[0].Context["File"]=="assets/target.jsonc");
    assert(emitted[3].Context["Property"]=="Effect.Particle.Radius");
    assert(emitted[0].OperationId==first);
    assert(PS::StructuredDiagnostic::IssueKey(emitted[3])!=PS::StructuredDiagnostic::IssueKey(emitted[4]));
    for(const auto& record:emitted){auto line=PS::StructuredDiagnostic::Encode(record);assert(line);}
}
