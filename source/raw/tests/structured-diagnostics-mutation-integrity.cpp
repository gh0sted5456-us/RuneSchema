#include "Utility/StructuredDiagnosticOutcomeEvidence.h"
#include "Utility/StructuredDiagnosticB2.h"
#include "Core/CookedPakRegistryManifest.h"
#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

namespace PS::StructuredDiagnostics {
static std::vector<StructuredDiagnostic::Record> captured;
bool Emit(StructuredDiagnostic::Record record) noexcept {
    record.EventId="b6a4-test-event";
    record.SessionId="b6a4-test-session";
    record.Timestamp="2026-10-23T12:00:00Z";
    if(!StructuredDiagnostic::Validate(record))return false;
    captured.push_back(std::move(record));return true;
}
}

int main() {
    using namespace PS::StructuredDiagnosticOutcomeEvidence;
    assert(std::string(InterruptedAfter(false))=="Skipped");
    assert(std::string(InterruptedAfter(true))=="Partial");
    assert(std::string(PhaseAfter(false))=="Validation");
    assert(std::string(PhaseAfter(true))=="Application");

    const PS::StructuredDiagnosticB2::Provenance bp{"ExampleMod","blueprints/nested.jsonc",
        "/Game/Test/BP_Test.BP_Test_C","blueprint-8"};
    for(const bool modified:{false,true}){
        assert(PS::StructuredDiagnosticB2::Report("RS-BP-009","Error",InterruptedAfter(modified),
            "BlueprintLoader","Blueprint modification failed","Nested property write failed",bp,
            PhaseAfter(modified),"Component.Speed"));
        const auto& report=PS::StructuredDiagnostics::captured.back();
        assert(report.Code=="RS-BP-009");
        assert(report.Outcome==InterruptedAfter(modified));
        assert(report.OperationId==bp.OperationId);
        assert(report.Context.at("ModId")==bp.ModId);
        assert(report.Context.at("File")==bp.File);
        assert(report.Context.at("Target")==bp.Target);
        assert(report.Context.at("Property")=="Component.Speed");
        assert(report.Context.at("Phase")==PhaseAfter(modified));
        const auto encoded=PS::StructuredDiagnostic::Encode(report);
        assert(encoded&&PS::StructuredDiagnostic::Parse(*encoded));
    }
    assert(PS::StructuredDiagnosticB2::Report("RS-BP-010","Warning","Skipped",
        "BlueprintLoader","Blueprint component template unavailable","Null template",bp,
        "Resolution","Component"));
    assert(PS::StructuredDiagnostics::captured.back().Context.at("Property")=="Component");
    assert(PS::StructuredDiagnosticB2::Report("RS-BP-011","Warning","Skipped",
        "BlueprintLoader","Blueprint component definition invalid","Invalid node",bp,
        "Validation","Component"));

    using namespace PS::CookedPakRegistryManifest;
    Reset();
    assert(!HasPublishedDeclarations());
    const nlohmann::json valid={{"NativeRegistries",{{"Items",nlohmann::json::array({"/Game/Mods/ExampleMod/DA_A.DA_A"})}}}};
    assert(Publish(valid,"ExampleMod","registry/valid.jsonc"));
    const auto before=Snapshot();
    assert(HasPublishedDeclarations());
    assert(before.size()==1&&before.front().Owner=="ExampleMod");
    bool rejected=false;
    try {const nlohmann::json malformed={{"NativeRegistries",{{"Bogus",nlohmann::json::array({"/Game/X.X"})}}}};
         (void)Publish(malformed,"ExampleMod","registry/rejected.jsonc");}
    catch(const std::runtime_error&){rejected=true;}
    assert(rejected&&Snapshot().size()==before.size());
    assert(Snapshot().front().Fingerprint==before.front().Fingerprint);

    Manifest journal; journal.Owner="AnotherMod";journal.Source="journal";
    journal.LoaderDefinitions["Journals"]={{"/Game/Mods/AnotherMod/DA_J.DA_J",nlohmann::json::object()}};
    Manifest lore; lore.Owner="ThirdMod";lore.Source="lore";
    lore.LoaderDefinitions["Lore"]={{"/Game/Mods/AnotherMod/DA_J.DA_J",nlohmann::json::object()}};
    rejected=false;
    try{PublishBatch({journal,lore});}catch(const std::runtime_error&){rejected=true;}
    assert(rejected&&Snapshot().size()==before.size());
    Reset();
    assert(!HasPublishedDeclarations());
}
