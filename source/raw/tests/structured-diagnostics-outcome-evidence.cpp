#include "Utility/StructuredDiagnosticOutcomeEvidence.h"
#include "Loader/StringReplacementRules.h"
#include "Utility/StructuredDiagnosticB3C.h"
#include <cassert>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using PS::StructuredDiagnosticOutcomeEvidence::AppliedProgress;

namespace PS::StructuredDiagnostics {
static std::vector<PS::StructuredDiagnostic::Record> sink;
bool Emit(PS::StructuredDiagnostic::Record record) noexcept {
    record.EventId="outcome-event";record.SessionId="outcome-session";
    record.Timestamp="2026-10-23T12:00:00Z";
    if(!PS::StructuredDiagnostic::Validate(record))return false;
    sink.push_back(std::move(record));return true;
}
}

static void VerifyReport(const char* code,AppliedProgress evidence,const char* source){
    assert(PS::StructuredDiagnosticB3C::Report(code,"Error",evidence.Interrupted(),source,
        "Interrupted definition","Observed mutation boundary", "ExampleMod","file.jsonc",{},evidence.InterruptedPhase()));
    const auto& r=PS::StructuredDiagnostics::sink.back();
    assert(r.Code==code && r.Outcome==evidence.Interrupted());
    assert(r.Context.at("Phase")==evidence.InterruptedPhase());
    const auto encoded=PS::StructuredDiagnostic::Encode(r);
    assert(encoded && PS::StructuredDiagnostic::Parse(*encoded));
}


static std::string_view SimulateFailureAt(std::size_t failAt) {
    std::map<std::string,std::string> values;
    AppliedProgress applied;
    try {
        for(std::size_t i=0;i<3;++i){
            if(i==failAt)throw std::runtime_error("insertion interrupted");
            values[std::to_string(i)]="ok";
            applied.MarkApplied();
        }
    }catch(const std::exception&){
        assert(values.size()==failAt);
        return applied.Interrupted();
    }
    return "Succeeded";
}

int main(){
    AppliedProgress attempt;
    assert(!attempt.HasApplied());
    assert(std::string_view(attempt.Interrupted())=="Skipped");
    assert(std::string_view(attempt.InterruptedPhase())=="Validation");
    attempt.MarkApplied();
    assert(attempt.HasApplied());
    assert(std::string_view(attempt.Interrupted())=="Partial");
    assert(std::string_view(attempt.InterruptedPhase())=="Application");
    assert(SimulateFailureAt(0)=="Skipped");
    assert(SimulateFailureAt(1)=="Partial");
    assert(SimulateFailureAt(2)=="Partial");
    assert(SimulateFailureAt(3)=="Succeeded");
    AppliedProgress empty;
    VerifyReport("RS-ENUM-002",empty,"EnumLoader");
    VerifyReport("RS-STR-003",empty,"StringLoader");
    empty.MarkApplied();
    VerifyReport("RS-ENUM-002",empty,"EnumLoader");
    VerifyReport("RS-STR-003",empty,"StringLoader");
    assert(PS::StructuredDiagnostics::sink.size()==4);

    const nlohmann::json valid={{"scope",{{"hello","world"}}}};
    DragonWilds::StringReplacementRules::Validate(valid);
    const nlohmann::json invalid={{"scope",{{"a","ok"},{"b",42}}}};
    bool rejected=false;
    try{DragonWilds::StringReplacementRules::Validate(invalid);}catch(const std::runtime_error&){rejected=true;}
    assert(rejected);
}
