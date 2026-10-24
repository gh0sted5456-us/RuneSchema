#include "Utility/StructuredDiagnosticB3C.h"
#include <cassert>
#include <string>
#include <string_view>
#include <vector>
#include <utility>
namespace PS::StructuredDiagnostics {
static std::vector<PS::StructuredDiagnostic::Record> sink;
bool Emit(PS::StructuredDiagnostic::Record r) noexcept {
    r.EventId="evt-1";r.SessionId="session-1";r.Timestamp="2026-10-21T13:00:00Z";
    if(!PS::StructuredDiagnostic::Validate(r))return false;
    sink.push_back(std::move(r));return true;
}
}
int main() {
    using PS::StructuredDiagnosticB3C::Report;
    const std::pair<std::string_view,std::string_view> records[]={
        {"RS-CRS-001","CourseLoader"},
        {"RS-CRS-002","CourseLoader"},
        {"RS-CRS-003","CourseLoader"},
        {"RS-CRS-004","CourseLoader"},
        {"RS-CRS-005","CourseLoader"},
        {"RS-CRS-006","CourseLoader"},
        {"RS-ENUM-001","EnumLoader"},
        {"RS-ENUM-002","EnumLoader"},
        {"RS-EQP-001","EquipmentLoader"},
        {"RS-EQP-002","EquipmentLoader"},
        {"RS-EQP-003","EquipmentLoader"},
        {"RS-EQP-004","EquipmentLoader"},
        {"RS-EQP-005","EquipmentLoader"},
        {"RS-EQP-006","EquipmentLoader"},
        {"RS-GE-001","GameplayEffectLoader"},
        {"RS-GE-002","GameplayEffectLoader"},
        {"RS-GE-003","GameplayEffectLoader"},
        {"RS-GE-004","GameplayEffectLoader"},
        {"RS-JRN-001","JournalLoader"},
        {"RS-JRN-002","JournalLoader"},
        {"RS-JRN-003","JournalLoader"},
        {"RS-JRN-004","JournalLoader"},
        {"RS-JRN-005","JournalLoader"},
        {"RS-JRN-006","JournalLoader"},
        {"RS-NIA-001","NiagaraLoader"},
        {"RS-NIA-002","NiagaraLoader"},
        {"RS-NIA-003","NiagaraLoader"},
        {"RS-NIA-004","NiagaraLoader"},
        {"RS-NPC-001","NpcLoader"},
        {"RS-RGST-001","RegistryLoader"},
        {"RS-RGST-002","RegistryLoader"},
        {"RS-RGST-003","RegistryLoader"},
        {"RS-STR-001","StringLoader"},
        {"RS-STR-002","StringLoader"},
        {"RS-STR-003","StringLoader"},
        {"RS-VND-001","VendorLoader"},
    };
    for(const auto& [code,source]:records){
        assert(Report(std::string(code),"Error","Skipped",std::string(source),
            "Definition rejected","The declaration violated the supported definition contract.",
            "ExampleMod","nested/unicode-\xC3\xA9.jsonc","TestTarget","Validation","Properties.Test"));
        const auto& row=PS::StructuredDiagnostics::sink.back();
        assert(row.Code==code && row.Source==source);
        assert(row.Context.at("ModId")=="ExampleMod");
        assert(row.Context.at("File")=="nested/unicode-\xC3\xA9.jsonc");
        const auto encoded=PS::StructuredDiagnostic::Encode(row);
        assert(encoded && PS::StructuredDiagnostic::Parse(*encoded).has_value());
        assert(PS::StructuredDiagnostic::IssueKey(row)==PS::StructuredDiagnostic::IssueKey(PS::StructuredDiagnostic::Parse(*encoded).value()));
    }
    assert(PS::StructuredDiagnostics::sink.size()==std::size(records));
    assert(PS::StructuredDiagnostic::Contains(PS::StructuredDiagnostic::Sources,"NpcLoader"));
}
